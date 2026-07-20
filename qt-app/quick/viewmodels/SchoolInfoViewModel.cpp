#include "SchoolInfoViewModel.h"

#include <QSettings>

#include "appsettings.h"
#include <QFileInfo>

SchoolInfoViewModel::SchoolInfoViewModel(QObject *parent)
    : QObject(parent)
{
    AppSettings s;
    loadFrom(s);
}

SchoolInfoViewModel::SchoolInfoViewModel(QSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(&settings)
{
    loadFrom(settings);
}

void SchoolInfoViewModel::reload()
{
    bool changed = false;
    if (m_settings) {
        // The writer (SettingsController::save) uses its own QSettings object
        // over the same store; sync() is what makes those writes visible here.
        m_settings->sync();
        changed = loadFrom(*m_settings);
    } else {
        AppSettings s;
        s.sync();
        changed = loadFrom(s);
    }
    if (changed)
        emit schoolInfoChanged();
}

bool SchoolInfoViewModel::loadFrom(QSettings &settings)
{
    const QString name    = settings.value(QStringLiteral("school/name")).toString();
    const QString address = settings.value(QStringLiteral("school/address")).toString();

    const QString path = settings.value(QStringLiteral("school/logoPath")).toString();
    // Only expose a URL when the configured file is actually there right
    // now. This is the graceful-degradation seam: QML keys its placeholder
    // fallback off hasLogo, never off "is logoUrl non-empty" or a raw path
    // string, so a rotted path can never reach an Image element.
    const bool hasLogo = !path.isEmpty() && QFileInfo::exists(path);
    const QUrl logoUrl = hasLogo ? QUrl::fromLocalFile(path) : QUrl();

    const bool changed = name != m_schoolName || address != m_schoolAddress
                      || hasLogo != m_hasLogo || logoUrl != m_logoUrl;

    m_schoolName    = name;
    m_schoolAddress = address;
    m_hasLogo       = hasLogo;
    m_logoUrl       = logoUrl;
    return changed;
}
