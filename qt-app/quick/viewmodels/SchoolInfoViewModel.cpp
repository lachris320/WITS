#include "SchoolInfoViewModel.h"

#include <QSettings>
#include <QFileInfo>

SchoolInfoViewModel::SchoolInfoViewModel(QObject *parent)
    : QObject(parent)
{
    QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    loadFrom(s);
}

SchoolInfoViewModel::SchoolInfoViewModel(QSettings &settings, QObject *parent)
    : QObject(parent)
{
    loadFrom(settings);
}

void SchoolInfoViewModel::loadFrom(QSettings &settings)
{
    m_schoolName    = settings.value(QStringLiteral("school/name")).toString();
    m_schoolAddress = settings.value(QStringLiteral("school/address")).toString();

    const QString path = settings.value(QStringLiteral("school/logoPath")).toString();
    // Only expose a URL when the configured file is actually there right
    // now. This is the graceful-degradation seam: QML keys its placeholder
    // fallback off hasLogo, never off "is logoUrl non-empty" or a raw path
    // string, so a rotted path can never reach an Image element.
    m_hasLogo = !path.isEmpty() && QFileInfo::exists(path);
    m_logoUrl = m_hasLogo ? QUrl::fromLocalFile(path) : QUrl();
}
