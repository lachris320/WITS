#include "SettingsViewModel.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include "AdminSession.h"
#include "HttpForm.h"
#include "apiconfig.h"
#include "reportcontroller.h"

SettingsViewModel::SettingsViewModel(QObject *parent)
    : QObject(parent)
    , m_controller(this)
    , m_nam(new QNetworkAccessManager(this))
{
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

void SettingsViewModel::importLogo(const QString &sourcePath)
{
    const QString dest = m_controller.importLogo(sourcePath);
    if (dest.isEmpty()) {
        // SettingsController already emitted importError; surface a status.
        setStatus(QStringLiteral("Could not import logo."));
        return;
    }
    m_cur.logoPath = dest;
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    s.setValue(QStringLiteral("school/logoPath"), dest);
    s.sync();

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

void SettingsViewModel::applyAdminInfoResponse(const QByteArray &json)
{
    setBusy(false);
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    const QString status = obj.value(QStringLiteral("status")).toString();
    const QString message = obj.value(QStringLiteral("message")).toString();
    if (status == QLatin1String("success")) {
        emit adminInfoSaved();
    } else if (isAuthFailureMessage(message)) {
        emit authFailed();
    } else {
        emit adminInfoFailed(message.isEmpty()
            ? QStringLiteral("Failed to update admin info.") : message);
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
        [this]() { setBusy(false); emit networkError(); });   // 401 lands here in production (see plan T10 known limitation)
}

void SettingsViewModel::applyKeyChangeResponse(const QByteArray &json, const QString &newKey)
{
    setBusy(false);
    const QJsonObject obj = QJsonDocument::fromJson(json).object();
    if (obj.value(QStringLiteral("status")).toString() == QLatin1String("success")) {
        AdminSession::instance().refresh(newKey);   // same-session key change (§3.3)
        emit keyChanged();
    } else {
        emit keyChangeFailed(obj.value(QStringLiteral("message")).toString());
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
    const QStringList parsed = ReportController::parseDepartments(json);
    if (parsed == m_departments)
        return;
    m_departments = parsed;
    emit departmentsChanged();
}

void SettingsViewModel::loadDepartments()
{
    // GET (no body) — HttpForm is POST-only, so use the NAM directly here,
    // mirroring ReportController's own GET helpers.
    QNetworkReply *reply =
        m_nam->get(QNetworkRequest(ApiConfig::endpoint(QStringLiteral("get_departments.php"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const bool netErr = reply->error() != QNetworkReply::NoError;
        reply->deleteLater();
        if (netErr) { emit networkError(); return; }
        applyDepartmentsResponse(body);
    });
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
