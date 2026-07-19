#include "SettingsViewModel.h"

#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QSettings>

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
