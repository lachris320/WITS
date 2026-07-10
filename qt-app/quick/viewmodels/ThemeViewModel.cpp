#include "ThemeViewModel.h"
#include "brandtheme.h"

ThemeViewModel::ThemeViewModel(QObject *parent)
    : QObject(parent)
{
    // Phase 1: read the engine's process-wide default (current() defaults to
    // fallbackPalette(), brandtheme.h:67). Cache-first BrandingController sync
    // (§13.2/13.3) lands in Phase 2 with the kiosk branding network path.
    m_config.palette = BrandTheme::current();
}

void ThemeViewModel::refresh()
{
    m_config.palette = BrandTheme::current();
    emit changed();
}

bool ThemeViewModel::regenerateFromImportedLogo(const QString &path)
{
    QString err;
    const bool ok = BrandTheme::regenerateFromLogo(m_config, path, &err);
    if (ok) {
        BrandTheme::setCurrent(m_config.palette);
        emit changed();
    }
    return ok;
}
