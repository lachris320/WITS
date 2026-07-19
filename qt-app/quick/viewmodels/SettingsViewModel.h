#ifndef SETTINGSVIEWMODEL_H
#define SETTINGSVIEWMODEL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <qqml.h>
#include "settingscontroller.h"
#include "settingsdata.h"

class QNetworkAccessManager;

// The Settings screen's ViewModel (spec §4.1) — the ONLY QML-facing C++ for
// SettingsScreen. Owns a SettingsController (QSettings persistence + logo
// import) and posts admin-info / key-change / reset-visits via HttpForm, using
// the admin key held in AdminSession. Dirty tracking drives the Save button.
class SettingsViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString schoolName    READ schoolName    WRITE setSchoolName    NOTIFY schoolNameChanged)
    Q_PROPERTY(QString schoolAddress READ schoolAddress WRITE setSchoolAddress NOTIFY schoolAddressChanged)
    Q_PROPERTY(QString logoPath      READ logoPath      NOTIFY logoChanged)
    Q_PROPERTY(QUrl    logoUrl       READ logoUrl       NOTIFY logoChanged)
    Q_PROPERTY(bool    hasLogo       READ hasLogo       NOTIFY logoChanged)
    Q_PROPERTY(QString adminName     READ adminName     WRITE setAdminName     NOTIFY adminNameChanged)
    Q_PROPERTY(QString adminPosition READ adminPosition WRITE setAdminPosition NOTIFY adminPositionChanged)
    Q_PROPERTY(int     openHour      READ openHour      WRITE setOpenHour      NOTIFY openHourChanged)
    Q_PROPERTY(int     closeHour     READ closeHour     WRITE setCloseHour     NOTIFY closeHourChanged)
    Q_PROPERTY(bool    guestEnabled  READ guestEnabled  WRITE setGuestEnabled  NOTIFY guestEnabledChanged)
    Q_PROPERTY(bool    dirty         READ dirty         NOTIFY dirtyChanged)
    Q_PROPERTY(bool    busy          READ busy          NOTIFY busyChanged)
    Q_PROPERTY(QStringList departments READ departments NOTIFY departmentsChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)

public:
    explicit SettingsViewModel(QObject *parent = nullptr);

    QString schoolName() const    { return m_cur.schoolName; }
    QString schoolAddress() const { return m_cur.schoolAddress; }
    QString logoPath() const      { return m_cur.logoPath; }
    QUrl    logoUrl() const;
    bool    hasLogo() const;
    QString adminName() const     { return m_cur.adminName; }
    QString adminPosition() const { return m_cur.adminPosition; }
    int     openHour() const      { return m_cur.libraryOpenHour; }
    int     closeHour() const     { return m_cur.libraryCloseHour; }
    bool    guestEnabled() const  { return m_cur.guestLoginEnabled; }
    bool    dirty() const         { return m_dirty; }
    bool    busy() const          { return m_busy; }
    QStringList departments() const { return m_departments; }
    QString statusMessage() const { return m_statusMessage; }

    void setSchoolName(const QString &v);
    void setSchoolAddress(const QString &v);
    void setAdminName(const QString &v);
    void setAdminPosition(const QString &v);
    void setOpenHour(int v);
    void setCloseHour(int v);
    void setGuestEnabled(bool v);

    // Re-reads the persisted baseline. NO-OP while dirty(): AdminScreen's
    // Loader destroys/recreates SettingsScreen on every navigation, so
    // re-entering Settings re-runs load() — overwriting m_cur there would
    // silently discard the user's unsaved edits.
    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    // NO ThemeViewModel member here — the VM only imports+persists the logo.
    // Live re-theme is wired in QML on Theme's own instance (T14); see the
    // CRITICAL note in the Phase 4c plan, Task 9.
    //
    // Takes the QUrl that FileDialog hands back rather than a path string: the
    // url -> path conversion belongs here (QUrl::toLocalFile handles Windows,
    // POSIX, UNC and percent-encoding) and NOT in hand-rolled QML string
    // surgery. SettingsController below still needs a filesystem path because
    // it bottoms out in QFile::exists()/QFile::copy().
    Q_INVOKABLE void importLogo(const QUrl &sourceUrl);
    Q_INVOKABLE void saveAdminInfo();
    // update_admin_key.php bcrypt-verifies oldKey server-side, so no admin_key
    // field is sent (it is NOT guarded by requireAdminAuth). On success the
    // held session key must move forward so subsequent destructive ops in
    // this same session don't fail auth on the now-stale old key (spec §3.3).
    Q_INVOKABLE void changeAdminKey(const QString &oldKey, const QString &newKey);
    // GETs get_departments.php to fill the flat department picker used by the
    // reset-visits flow (T13/T14). Flat list only — the cascading
    // Department -> Course -> Year selector belongs to tracks 4a/4b.
    Q_INVOKABLE void loadDepartments();

    // Network-free decode seam (spec §6.1): the async saveAdminInfo() wires
    // HttpForm::submit's success callback straight to this so tests can drive
    // response classification with synthetic QByteArray payloads.
    void applyAdminInfoResponse(const QByteArray &json);
    // Same seam pattern for the key-change response; needs newKey to hand to
    // AdminSession::refresh() on success.
    void applyKeyChangeResponse(const QByteArray &json, const QString &newKey);
    // Same seam pattern for the departments GET: reuses the shipped, unit-
    // tested ReportController::parseDepartments parser, which already
    // degrades to an empty QStringList on error/empty (picker placeholder).
    void applyDepartmentsResponse(const QByteArray &json);

    // Pure, network-free CSV serializer (export-before-destroy). RFC-4180-ish:
    // CRLF line endings; a cell is quoted iff it contains a comma, quote, CR,
    // or LF; embedded quotes are doubled. Unit-tested standalone.
    static QString serializeCsv(const QStringList &headers,
                                const QList<QStringList> &rows);

    // Tier-2 destructive op (spec §3.3): zeroes students.visits AND permanently
    // DELETEs library_visits rows for department on the backend. adminKey MUST
    // be the freshly re-typed key from the tier-2 confirmation dialog, NOT the
    // held AdminSession key — the re-typing is deliberate extra friction in
    // front of the most irreversible action in Phase 4c.
    Q_INVOKABLE void resetVisits(const QString &department, const QString &adminKey);
    void applyResetVisitsResponse(const QByteArray &json);

    // Reset Manifest — operation metadata only, NOT a visit backup (owner
    // decision 2026-07-19). Full pre-reset row export lands in Phase 4a.
    Q_INVOKABLE bool writeResetManifest(const QString &department, const QUrl &fileUrl);
    // Absolute file: URL the save dialog pre-fills with. Built in C++ (and unit
    // tested) because a bare relative string assigned to FileDialog's url-typed
    // selectedFile resolves against the component's base URL — which is a
    // "qrc:" path for this module, so the pre-fill would silently do nothing.
    // Named "Reset_Manifest_…", never "Backup"/"Export" (owner decision).
    Q_INVOKABLE QUrl defaultManifestUrl(const QString &department) const;

signals:
    void schoolNameChanged();
    void schoolAddressChanged();
    void logoChanged();
    void adminNameChanged();
    void adminPositionChanged();
    void openHourChanged();
    void closeHourChanged();
    void guestEnabledChanged();
    void dirtyChanged();
    void busyChanged();
    void departmentsChanged();
    void statusChanged();
    void saved();
    void saveFailed(const QString &message);
    void adminInfoSaved();
    void adminInfoFailed(const QString &message);
    void keyChanged();
    void keyChangeFailed(const QString &message);
    void authFailed();
    void networkError();
    void visitsReset();
    void resetFailed(const QString &message);

private:
    void recomputeDirty();
    void setBusy(bool v);
    void setStatus(const QString &msg);
    // Shared auth-failure classification: the two message strings
    // requireAdminAuth (T17) / its guard return on a bad or missing key.
    static bool isAuthFailureMessage(const QString &message);
    // Single url -> filesystem-path conversion used by every file-taking entry
    // point, so the "file:///…" / UNC handling lives in exactly one place.
    static QString localPath(const QUrl &url);

    SettingsController m_controller;
    QNetworkAccessManager *m_nam = nullptr;
    SettingsData m_cur;       // edited (live) values
    SettingsData m_saved;     // last loaded/saved baseline (dirty is m_cur != m_saved)
    QStringList m_departments;
    QString m_statusMessage;
    bool m_dirty = false;
    bool m_busy = false;
};

#endif // SETTINGSVIEWMODEL_H
