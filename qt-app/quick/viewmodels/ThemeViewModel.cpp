#include "ThemeViewModel.h"
#include "brandtheme.h"

ThemeViewModel::ThemeViewModel(QObject *parent)
    : QObject(parent)
{
    // Single source of truth for colors is BrandTheme::current(); every getter
    // reads it live. No palette cache here — a cached copy would only drift.
    // m_config is scratch for regenerateFromImportedLogo (below).
}

void ThemeViewModel::refresh()
{
    // Re-notify QML after an external BrandTheme::setCurrent. Nothing to cache —
    // the getters already read the engine live; this just fires the binding.
    emit changed();
}

ThemeViewModel::RegenResult ThemeViewModel::regenerateFromImportedLogo(const QString &path)
{
    // Seed the scratch config from the live palette so regeneration starts from
    // whatever is current, then let the engine overwrite it.
    m_config.palette = BrandTheme::current();
    QString err;
    const bool ok = BrandTheme::regenerateFromLogo(m_config, path, &err);
    if (!ok)
        return RegenResult::Failed; // unreadable logo or Manual mode; config untouched

    // Apply for BOTH Ok and FellBack: on a gate fallback the fallback palette
    // is the correct visible result, so it must still be set and notified.
    BrandTheme::setCurrent(m_config.palette);
    emit changed();
    return m_config.didFallBack ? RegenResult::FellBack : RegenResult::Ok;
}
