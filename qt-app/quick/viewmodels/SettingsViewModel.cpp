#include "SettingsViewModel.h"

#include <QNetworkAccessManager>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

#include "AdminSession.h"
#include "HttpForm.h"
#include "apiconfig.h"
#include "reportcontroller.h"

SettingsViewModel::SettingsViewModel(QObject *parent)
    : QObject(parent)
    , m_controller(this)
    , m_nam(new QNetworkAccessManager(this))
{
    // SettingsController reports WHY an import failed ("File not found: …",
    // "Not a valid image: …", "Could not copy to: …") and nothing was listening
    // — the admin got one generic string for three very different problems.
    // Relay the real reason into statusMessage, which the screen already shows.
    connect(&m_controller, &SettingsController::importError, this,
            [this](const QString &message) {
        m_importErrorSeen = true;
        setStatus(message);
    });
}

QUrl SettingsViewModel::logoUrl() const
{
    // A bare path is not a valid Image.source — mirror SchoolInfoViewModel.
    if (m_cur.logoPath.isEmpty() || !QFileInfo::exists(m_cur.logoPath))
        return {};
    return QUrl::fromLocalFile(m_cur.logoPath);
}

bool SettingsViewModel::hasLogo() const
{
    return !m_cur.logoPath.isEmpty() && QFileInfo::exists(m_cur.logoPath);
}

void SettingsViewModel::load()
{
    // Guard the re-entry case: AdminScreen's Loader destroys and recreates
    // SettingsScreen on every navigation, so load() runs again each time the
    // admin returns to Settings. Re-reading the baseline over a dirty form
    // would discard the pending edits (and reset `dirty`) with no warning, so
    // a dirty form keeps what it has — the baseline is only re-read when
    // there is nothing to lose. save() clears dirty, which re-arms this.
    if (m_dirty)
        return;

    m_cur = m_controller.load();
    m_saved = m_cur;
    recomputeDirty();
    emit schoolNameChanged();    emit schoolAddressChanged();
    emit logoChanged();
    emit adminNameChanged();     emit adminPositionChanged();
    emit openHourChanged();      emit closeHourChanged();
    emit guestEnabledChanged();
}

void SettingsViewModel::save()
{
    if (!m_controller.save(m_cur)) {
        emit saveFailed(QStringLiteral("Could not save settings."));
        return;
    }
    // RECONCILE (spec §4.1 / plan T8): SettingsController writes
    // features/guestLogin, but KioskViewModel reads kiosk/guestEnabled. Mirror
    // the flag onto the kiosk's key so the toggle actually reaches the kiosk,
    // without changing the shared controller's key contract (legacy
    // adminwindow.cpp still reads features/guestLogin).
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    s.setValue(QStringLiteral("kiosk/guestEnabled"), m_cur.guestLoginEnabled);
    s.sync();

    m_saved = m_cur;
    recomputeDirty();      // -> clears dirty
    emit saved();
}

QString SettingsViewModel::localPath(const QUrl &url)
{
    // isLocalFile() covers "file:///C:/x", "file:///home/x" AND the UNC form
    // "file://server/share/x"; toLocalFile() also percent-decodes. A bare path
    // (no scheme) round-trips unchanged through toString().
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

void SettingsViewModel::importLogo(const QUrl &sourceUrl)
{
    m_importErrorSeen = false;
    const QString dest = m_controller.importLogo(localPath(sourceUrl));
    if (dest.isEmpty()) {
        // The constructor's importError relay has already put the specific
        // reason in statusMessage; this generic line is only the backstop for
        // an empty return with no signal.
        if (!m_importErrorSeen)
            setStatus(QStringLiteral("Could not import logo."));
        return;
    }
    m_cur.logoPath = dest;
    // NO QSettings write here. An import marks the form dirty, i.e. the UI
    // presents it as an unsaved edit — persisting it immediately made that a
    // lie: the logo was already applied for good, navigating away could not
    // abandon it, and load() is a no-op while dirty so nothing could undo it.
    // save() writes m_cur.logoPath through SettingsController like every other
    // field, so the Save gate is honest in both directions. The live re-theme
    // (QML, T14) is unaffected — it re-extracts from the imported file path,
    // which m_cur.logoPath already carries.

    emit logoChanged();
    recomputeDirty();
    // NB: no re-theme here. The live palette re-extraction runs in QML on
    // Theme._vm (T14) — see the CRITICAL note above for why a VM-owned
    // ThemeViewModel would not reach the running UI.
}

bool SettingsViewModel::isAuthFailureMessage(const QString &message)
{
    return message.contains(QStringLiteral("Invalid admin key"), Qt::CaseInsensitive)
        || message.contains(QStringLiteral("authentication required"), Qt::CaseInsensitive);
}

SettingsViewModel::Outcome SettingsViewModel::classify(const QByteArray &json, QString *message)
{
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    const QString msg = obj.value(QStringLiteral("message")).toString();
    if (message)
        *message = msg;
    // A non-JSON body (PHP fatal, HTML error page) yields an empty object, so
    // status is empty, so it lands in Error — failing loudly, never silently.
    if (obj.value(QStringLiteral("status")).toString() == QLatin1String("success"))
        return Outcome::Success;
    return isAuthFailureMessage(msg) ? Outcome::Auth : Outcome::Error;
}

void SettingsViewModel::applyAdminInfoResponse(const QByteArray &json)
{
    setBusy(false);
    QString message;
    switch (classify(json, &message)) {
    case Outcome::Success:
        emit adminInfoSaved();
        break;
    case Outcome::Auth:
        emit authFailed();
        break;
    case Outcome::Error:
        emit adminInfoFailed(message.isEmpty()
            ? QStringLiteral("Failed to update admin info.") : message);
        break;
    }
}

void SettingsViewModel::saveAdminInfo()
{
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("admin_name"), m_cur.adminName},
        {QStringLiteral("admin_position"), m_cur.adminPosition},
        {QStringLiteral("admin_key"), AdminSession::instance().key()},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("update_admin_info.php")),
                     fields, this,
        [this](const QByteArray &body) { applyAdminInfoResponse(body); },
        // Only a genuine transport failure lands here: HttpForm::isServerAnswer
        // routes requireAdminAuth's 401 (which carries a JSON body) to the
        // decode seam above, so a bad key surfaces as authFailed, not this.
        [this]() { setBusy(false); emit networkError(); });
}

void SettingsViewModel::applyKeyChangeResponse(const QByteArray &json, const QString &newKey)
{
    setBusy(false);
    QString message;
    // DELIBERATELY two-way, not three-way: update_admin_key.php is NOT behind
    // requireAdminAuth (it bcrypt-verifies old_key itself), so there is no
    // session-auth failure to route to authFailed() — a rejected old key is an
    // ordinary key-change failure. Auth therefore folds into the same branch as
    // Error, and the message is passed through verbatim with no default.
    if (classify(json, &message) == Outcome::Success) {
        AdminSession::instance().refresh(newKey);   // same-session key change (§3.3)
        emit keyChanged();
    } else {
        emit keyChangeFailed(message);
    }
}

void SettingsViewModel::changeAdminKey(const QString &oldKey, const QString &newKey)
{
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("old_key"), oldKey},
        {QStringLiteral("new_key"), newKey},
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("update_admin_key.php")),
                     fields, this,
        [this, newKey](const QByteArray &body) { applyKeyChangeResponse(body, newKey); },
        [this]() { setBusy(false); emit networkError(); });
}

void SettingsViewModel::applyDepartmentsResponse(const QByteArray &json)
{
    // Classify BEFORE parsing. parseDepartments() returns an empty QStringList
    // for an error envelope, a non-JSON body AND a genuinely empty list alike,
    // so handing it a failure body produced an empty picker with no signal and
    // no status — indistinguishable from "this school has no departments", on
    // the screen where a dead admin session matters most. Note the 401 case is
    // reachable here even though this is a GET: requireAdminAuth answers with a
    // body, which HttpForm::isServerAnswer routes to this seam by design.
    QString message;
    const Outcome outcome = classify(json, &message);
    if (outcome != Outcome::Success) {
        // Leave m_departments alone: a later failure must not wipe a picker
        // that is already populated.
        setStatus(message.isEmpty() ? QStringLiteral("Could not load departments.") : message);
        if (outcome == Outcome::Auth)
            emit authFailed();
        return;
    }

    const QStringList parsed = ReportController::parseDepartments(json);
    if (parsed == m_departments)
        return;
    m_departments = parsed;
    emit departmentsChanged();
}

void SettingsViewModel::loadDepartments()
{
    // HttpForm::get applies the SAME isServerAnswer() classification as the
    // POST paths, so this GET agrees with them: an HTTP error carrying a body
    // is the server answering and goes to the decode seam (which classifies
    // auth-vs-error from the envelope); only a transport failure is a
    // networkError.
    HttpForm::get(m_nam, ApiConfig::endpoint(QStringLiteral("get_departments.php")), this,
        [this](const QByteArray &body) { applyDepartmentsResponse(body); },
        [this]() { emit networkError(); });
}

QString SettingsViewModel::serializeCsv(const QStringList &headers,
                                        const QList<QStringList> &rows)
{
    auto cell = [](const QString &v) -> QString {
        const bool needsQuote = v.contains(QLatin1Char(',')) || v.contains(QLatin1Char('"'))
                             || v.contains(QLatin1Char('\n')) || v.contains(QLatin1Char('\r'));
        if (!needsQuote)
            return v;
        QString q = v;
        q.replace(QLatin1Char('"'), QLatin1String("\"\""));
        return QLatin1Char('"') + q + QLatin1Char('"');
    };
    auto line = [&cell](const QStringList &cells) -> QString {
        QStringList out;
        out.reserve(cells.size());
        for (const QString &c : cells) out << cell(c);
        return out.join(QLatin1Char(',')) + QLatin1String("\r\n");
    };
    QString csv = line(headers);
    for (const QStringList &r : rows) csv += line(r);
    return csv;
}

void SettingsViewModel::applyResetVisitsResponse(const QByteArray &json)
{
    setBusy(false);
    QString message;
    switch (classify(json, &message)) {
    case Outcome::Success:
        emit visitsReset();
        break;
    case Outcome::Auth:
        emit authFailed();
        break;
    case Outcome::Error:
        emit resetFailed(message.isEmpty() ? QStringLiteral("Reset failed.") : message);
        break;
    }
}

void SettingsViewModel::resetVisits(const QString &department, const QString &adminKey)
{
    if (department.trimmed().isEmpty() || department == QLatin1String("All")) {
        emit resetFailed(QStringLiteral("Please select a department."));
        return;
    }
    setBusy(true);
    const QList<QPair<QString, QString>> fields = {
        {QStringLiteral("department"), department},
        {QStringLiteral("admin_key"), adminKey},   // FRESH re-typed key (§3.3), not the held one
    };
    HttpForm::submit(m_nam, ApiConfig::endpoint(QStringLiteral("reset_visits.php")),
                     fields, this,
        [this](const QByteArray &body) { applyResetVisitsResponse(body); },
        [this]() { setBusy(false); emit networkError(); });
}

bool SettingsViewModel::writeResetManifest(const QString &department, const QUrl &fileUrl)
{
    // Operation metadata only — 4c has no visits-by-department fetch, so we do
    // NOT fabricate a partial backup. The count + full row export land in 4a.
    const QStringList headers{ QStringLiteral("Field"), QStringLiteral("Value") };
    const QList<QStringList> rows{
        { QStringLiteral("Document"),        QStringLiteral("Visit Reset Manifest") },
        { QStringLiteral("Department"),      department },
        { QStringLiteral("Reset timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate) },
        { QStringLiteral("Operator"),        m_cur.adminName },
        { QStringLiteral("Reset requested"), QStringLiteral("yes") },
        { QStringLiteral("Note"), QStringLiteral("Records what was reset. Full pre-reset visit-row export arrives in Phase 4a.") },
    };
    const QString csv = serializeCsv(headers, rows);
    QFile f(localPath(fileUrl));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    f.write(csv.toUtf8());
    f.close();
    return true;
}

QUrl SettingsViewModel::defaultManifestUrl(const QString &department) const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    // A department name reaches us from the backend; keep it filename-safe.
    QString dept = department.trimmed();
    dept.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    const QString name = QStringLiteral("Reset_Manifest_%1_%2.csv")
        .arg(dept, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    return QUrl::fromLocalFile(QDir(dir).filePath(name));
}

void SettingsViewModel::recomputeDirty()
{
    // Field-by-field compare against the saved baseline. logoPath is included
    // so a logo import (Task 9) also marks the form dirty.
    const bool d =
        m_cur.schoolName        != m_saved.schoolName      ||
        m_cur.schoolAddress     != m_saved.schoolAddress   ||
        m_cur.logoPath          != m_saved.logoPath        ||
        m_cur.adminName         != m_saved.adminName       ||
        m_cur.adminPosition     != m_saved.adminPosition   ||
        m_cur.libraryOpenHour   != m_saved.libraryOpenHour ||
        m_cur.libraryCloseHour  != m_saved.libraryCloseHour||
        m_cur.guestLoginEnabled != m_saved.guestLoginEnabled;
    if (d != m_dirty) { m_dirty = d; emit dirtyChanged(); }
}

void SettingsViewModel::setBusy(bool v)   { if (v != m_busy) { m_busy = v; emit busyChanged(); } }
void SettingsViewModel::setStatus(const QString &msg) { m_statusMessage = msg; emit statusChanged(); }

void SettingsViewModel::setSchoolName(const QString &v)
{ if (v == m_cur.schoolName) return; m_cur.schoolName = v; emit schoolNameChanged(); recomputeDirty(); }
void SettingsViewModel::setSchoolAddress(const QString &v)
{ if (v == m_cur.schoolAddress) return; m_cur.schoolAddress = v; emit schoolAddressChanged(); recomputeDirty(); }
void SettingsViewModel::setAdminName(const QString &v)
{ if (v == m_cur.adminName) return; m_cur.adminName = v; emit adminNameChanged(); recomputeDirty(); }
void SettingsViewModel::setAdminPosition(const QString &v)
{ if (v == m_cur.adminPosition) return; m_cur.adminPosition = v; emit adminPositionChanged(); recomputeDirty(); }
void SettingsViewModel::setOpenHour(int v)
{ if (v == m_cur.libraryOpenHour) return; m_cur.libraryOpenHour = v; emit openHourChanged(); recomputeDirty(); }
void SettingsViewModel::setCloseHour(int v)
{ if (v == m_cur.libraryCloseHour) return; m_cur.libraryCloseHour = v; emit closeHourChanged(); recomputeDirty(); }
void SettingsViewModel::setGuestEnabled(bool v)
{ if (v == m_cur.guestLoginEnabled) return; m_cur.guestLoginEnabled = v; emit guestEnabledChanged(); recomputeDirty(); }
