#ifndef IMPORTVIEWMODEL_H
#define IMPORTVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>                 // QML_ELEMENT — DatabaseScreen instantiates this type
#include "importdata.h"

class QNetworkAccessManager;
class ImportController;

// Import screen VM (spec §4.1, increment 4a.3). The ONLY QML-facing surface for
// bulk import. Owns an ImportController + its own QNetworkAccessManager
// (mirrors DatabaseViewModel's ownership). Drives the idle -> check-duplicates
// -> upload -> result flow; parse + client validation are delegated to the
// ImportController pure statics. cancel() is pre-upload only (there is no
// in-flight abort path — spec §4.1).
class ImportViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit ImportViewModel(QObject *parent = nullptr);

    enum class Phase { Idle, CheckingDuplicates, AwaitingDuplicates,
                       Uploading, Processing, Done, Failed };
    Q_ENUM(Phase)

    Q_PROPERTY(Phase phase READ phase NOTIFY phaseChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int parsedCount READ parsedCount NOTIFY parsedCountChanged)
    Q_PROPERTY(int duplicateCount READ duplicateCount NOTIFY duplicateCountChanged)
    Q_PROPERTY(bool allDuplicates READ allDuplicates NOTIFY allDuplicatesChanged)
    Q_PROPERTY(int uploadPercent READ uploadPercent NOTIFY uploadPercentChanged)
    Q_PROPERTY(QString dataFileName READ dataFileName NOTIFY dataFileNameChanged)
    Q_PROPERTY(QString photosZipName READ photosZipName NOTIFY photosZipNameChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultTextChanged)
    Q_PROPERTY(bool authFailure READ authFailure NOTIFY authFailureChanged)

    Phase phase() const { return m_phase; }
    bool busy() const { return m_phase == Phase::CheckingDuplicates
                            || m_phase == Phase::Uploading
                            || m_phase == Phase::Processing; }
    int parsedCount() const { return m_table.rows.size(); }
    int duplicateCount() const { return m_duplicates.size(); }
    bool allDuplicates() const { return parsedCount() > 0
                                     && m_duplicates.size() == parsedCount(); }
    int uploadPercent() const { return m_uploadPercent; }
    QString dataFileName() const { return m_dataFileName; }
    QString photosZipName() const { return m_photosZipName; }
    QString errorText() const { return m_errorText; }
    QString resultText() const { return m_resultText; }
    bool authFailure() const { return m_authFailure; }

    Q_INVOKABLE void setDataFile(const QUrl &fileUrl);
    Q_INVOKABLE void setPhotosZip(const QUrl &fileUrl);
    Q_INVOKABLE void clearPhotosZip();
    Q_INVOKABLE void startImport();
    Q_INVOKABLE void continueAfterDuplicates();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool downloadTemplate(const QUrl &fileUrl);   // body: Task 11

    // Public slots (test seam — driven network-free, like DatabaseViewModel).
    void onDuplicatesResolved(const QStringList &duplicates);
    void onImportError(const QString &title, const QString &message, ImportSeverity severity);
    void onUploadStarted();
    void onUploadProgress(int percent);
    void onUploadFinished(const UploadResult &result);
    void onUploadFailed(const QString &message);

signals:
    void phaseChanged();
    void busyChanged();
    void parsedCountChanged();
    void duplicateCountChanged();
    void allDuplicatesChanged();
    void uploadPercentChanged();
    void dataFileNameChanged();
    void photosZipNameChanged();
    void errorTextChanged();
    void resultTextChanged();
    void authFailureChanged();
    void finishedOk();    // upload succeeded — the dialog can auto-close/toast

private:
    void setPhase(Phase p);
    void setError(const QString &e);
    void setAuthFailure(bool v);
    void failWith(const QString &message);
    void beginUpload();

    QNetworkAccessManager *m_nam = nullptr;
    ImportController *m_controller = nullptr;

    QString m_dataFilePath, m_dataFileName;
    QString m_zipPath, m_photosZipName;
    ParsedTable m_table;
    QStringList m_duplicates;

    Phase m_phase = Phase::Idle;
    int m_uploadPercent = 0;
    QString m_errorText, m_resultText;
    bool m_authFailure = false;
};

#endif // IMPORTVIEWMODEL_H
