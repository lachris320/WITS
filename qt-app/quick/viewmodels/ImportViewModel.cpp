#include "ImportViewModel.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QSaveFile>
#include "importcontroller.h"
#include "AdminSession.h"
#include "SettingsViewModel.h"

ImportViewModel::ImportViewModel(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_controller(new ImportController(m_nam, this))
{
    connect(m_controller, &ImportController::duplicatesResolved,
            this, &ImportViewModel::onDuplicatesResolved);
    connect(m_controller, &ImportController::importError,
            this, &ImportViewModel::onImportError);
    connect(m_controller, &ImportController::uploadStarted,
            this, &ImportViewModel::onUploadStarted);
    connect(m_controller, &ImportController::uploadProgress,
            this, &ImportViewModel::onUploadProgress);
    connect(m_controller, &ImportController::uploadFinished,
            this, &ImportViewModel::onUploadFinished);
    connect(m_controller, &ImportController::uploadFailed,
            this, &ImportViewModel::onUploadFailed);
}

void ImportViewModel::setPhase(Phase p)
{
    if (m_phase == p) return;
    const bool wasBusy = busy();
    m_phase = p;
    emit phaseChanged();
    if (busy() != wasBusy) emit busyChanged();
}

void ImportViewModel::setError(const QString &e)
{
    if (m_errorText != e) { m_errorText = e; emit errorTextChanged(); }
}

void ImportViewModel::setAuthFailure(bool v)
{
    if (m_authFailure != v) { m_authFailure = v; emit authFailureChanged(); }
}

void ImportViewModel::failWith(const QString &message)
{
    setError(message);
    setAuthFailure(SettingsViewModel::isAuthFailureMessage(message));
    setPhase(Phase::Failed);
}

void ImportViewModel::setDataFile(const QUrl &fileUrl)
{
    m_dataFilePath = fileUrl.toLocalFile();
    m_dataFileName = QFileInfo(m_dataFilePath).fileName();
    emit dataFileNameChanged();

    // Parse eagerly so parsedCount is available for the Import button/hint.
    ParsedTable parsed;
    if (m_dataFilePath.endsWith(QLatin1String(".xlsx"), Qt::CaseInsensitive)) {
        parsed = m_controller->parseExcel(m_dataFilePath, nullptr);
    } else if (m_dataFilePath.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
        QFile f(m_dataFilePath);
        if (f.open(QIODevice::ReadOnly))
            parsed = ImportController::parseCsv(QString::fromUtf8(f.readAll()));
    }
    m_table = parsed;
    m_duplicates.clear();
    setError(QString());
    setAuthFailure(false);
    if (!m_resultText.isEmpty()) { m_resultText.clear(); emit resultTextChanged(); }
    if (m_uploadPercent != 0) { m_uploadPercent = 0; emit uploadPercentChanged(); }
    setPhase(Phase::Idle);
    emit parsedCountChanged();
    emit duplicateCountChanged();
    emit allDuplicatesChanged();
}

void ImportViewModel::setPhotosZip(const QUrl &fileUrl)
{
    m_zipPath = fileUrl.toLocalFile();
    m_photosZipName = QFileInfo(m_zipPath).fileName();
    emit photosZipNameChanged();
}

void ImportViewModel::clearPhotosZip()
{
    if (m_zipPath.isEmpty() && m_photosZipName.isEmpty()) return;
    m_zipPath.clear();
    m_photosZipName.clear();
    emit photosZipNameChanged();
}

void ImportViewModel::startImport()
{
    if (busy() || m_dataFilePath.isEmpty()) return;   // re-entry + precondition guard

    QStringList bad;
    const QString problem = ImportController::validateForImport(m_table, &bad);
    if (!problem.isEmpty()) {
        setError(problem);
        setPhase(Phase::Idle);      // stay in idle on a validation failure
        return;
    }
    setError(QString());

    const int idCol = m_table.headerIndex.value(QStringLiteral("school_id"));
    QStringList schoolIds;
    for (const QStringList &row : m_table.rows) {
        const QString id = (idCol >= 0 && idCol < row.size()) ? row.at(idCol).trimmed() : QString();
        if (!id.isEmpty())
            schoolIds << id;
    }

    setPhase(Phase::CheckingDuplicates);
    m_controller->checkDuplicates(schoolIds, AdminSession::instance().key());
}

void ImportViewModel::onDuplicatesResolved(const QStringList &duplicates)
{
    m_duplicates = duplicates;
    emit duplicateCountChanged();
    emit allDuplicatesChanged();

    if (duplicates.isEmpty()) {
        beginUpload();                       // (a) none -> upload
        return;
    }
    setPhase(Phase::AwaitingDuplicates);     // (b) some / (c) all -> inline confirm
}

void ImportViewModel::continueAfterDuplicates()
{
    if (m_phase != Phase::AwaitingDuplicates) return;
    if (allDuplicates()) return;             // (c) all-dupes -> Close only, no upload
    beginUpload();
}

void ImportViewModel::beginUpload()
{
    // skipIds = the client-resolved duplicates. uploadStarted flips the phase.
    m_controller->uploadStudents(m_table, m_zipPath, m_duplicates,
                                 AdminSession::instance().key());
}

void ImportViewModel::onUploadStarted()
{
    m_uploadPercent = 0; emit uploadPercentChanged();
    setPhase(Phase::Uploading);
}

void ImportViewModel::onUploadProgress(int percent)
{
    m_uploadPercent = percent; emit uploadPercentChanged();
    // Bytes are on the wire; once sent, the real wait is the server row insert.
    if (percent >= 100)
        setPhase(Phase::Processing);
}

void ImportViewModel::onUploadFinished(const UploadResult &result)
{
    if (!result.ok && !result.plainText) {
        // HTTP 200 with {"status":"error",...} — a server ANSWER, not a
        // transport failure (that goes through onUploadFailed instead).
        // Route it to Failed rather than displaying a fake "0 imported" success.
        failWith(result.message.isEmpty() ? tr("Import failed.") : result.message);
        return;
    }

    if (result.plainText) {
        m_resultText = result.rawText;       // older/partly-deployed endpoint
    } else {
        m_resultText = tr("%1 imported · %2 skipped · %3 failed")
                           .arg(result.successCount).arg(result.skippedCount).arg(result.errorCount);
    }
    emit resultTextChanged();
    setPhase(Phase::Done);
    emit finishedOk();
}

void ImportViewModel::onUploadFailed(const QString &message)
{
    failWith(message);
}

void ImportViewModel::onImportError(const QString & /*title*/, const QString &message,
                                    ImportSeverity severity)
{
    if (severity == ImportSeverity::Warning)
        return;   // e.g. ZIP-open warning — upload proceeds; nothing to surface fatally
    failWith(message);
}

void ImportViewModel::cancel()
{
    // Pre-upload only (spec §4.1): return to idle from file-select or duplicate
    // confirm. Does NOT abort an in-flight upload.
    if (m_phase == Phase::Uploading || m_phase == Phase::Processing) return;
    m_duplicates.clear();
    emit duplicateCountChanged();
    emit allDuplicatesChanged();
    setError(QString());
    setPhase(Phase::Idle);
}

bool ImportViewModel::downloadTemplate(const QUrl &fileUrl)
{
    const QByteArray bytes = ImportController::importTemplateCsv();
    QSaveFile file(fileUrl.toLocalFile());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(bytes) != bytes.size() || !file.commit())
        return false;
    return true;
}
