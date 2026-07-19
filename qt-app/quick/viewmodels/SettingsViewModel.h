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

    Q_INVOKABLE void load();
    Q_INVOKABLE void save();
    // NO ThemeViewModel member here — the VM only imports+persists the logo.
    // Live re-theme is wired in QML on Theme's own instance (T14); see the
    // CRITICAL note in the Phase 4c plan, Task 9.
    Q_INVOKABLE void importLogo(const QString &sourcePath);
    Q_INVOKABLE void saveAdminInfo();

    // Network-free decode seam (spec §6.1): the async saveAdminInfo() wires
    // HttpForm::submit's success callback straight to this so tests can drive
    // response classification with synthetic QByteArray payloads.
    void applyAdminInfoResponse(const QByteArray &json);

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
    void authFailed();
    void networkError();

private:
    void recomputeDirty();
    void setBusy(bool v);
    void setStatus(const QString &msg);
    // Shared auth-failure classification: the two message strings
    // requireAdminAuth (T17) / its guard return on a bad or missing key.
    static bool isAuthFailureMessage(const QString &message);

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
