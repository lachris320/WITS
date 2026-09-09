#include "ThemeViewModel.h"
#include "brandtheme.h"
#include <QGuiApplication>
#include <QStyleHints>
#include "appsettings.h"

namespace {
const char *const kThemeGroup = "theme";
const char *const kModeKey = "mode";
QString sanitizeMode(const QString &m)
{
    const QString t = m.trimmed();
    if (t.compare(QLatin1String("Dark"),  Qt::CaseInsensitive) == 0) return QStringLiteral("Dark");
    if (t.compare(QLatin1String("Light"), Qt::CaseInsensitive) == 0) return QStringLiteral("Light");
    return QStringLiteral("System");
}
} // namespace

ThemeViewModel::ThemeViewModel(QObject *parent)
    : QObject(parent)
{
    // Single source of truth for colors is BrandTheme::current(); every getter
    // reads it live. No palette cache here — a cached copy would only drift.
    loadMode();
    if (auto *hints = QGuiApplication::styleHints()) {
        m_systemScheme = hints->colorScheme();
        connect(hints, &QStyleHints::colorSchemeChanged,
                this, &ThemeViewModel::applySystemColorScheme);
    }
}

void ThemeViewModel::loadMode()
{
    AppSettings s;
    s.beginGroup(QLatin1String(kThemeGroup));
    m_mode = sanitizeMode(
        s.value(QLatin1String(kModeKey), QStringLiteral("System")).toString());
    s.endGroup();
}

void ThemeViewModel::setMode(const QString &mode)
{
    const QString m = sanitizeMode(mode);
    if (m == m_mode)
        return;
    const bool wasDark = resolvedDark();
    m_mode = m;
    {
        AppSettings s;
        s.beginGroup(QLatin1String(kThemeGroup));
        s.setValue(QLatin1String(kModeKey), m_mode);
        s.endGroup();
        s.sync();
    }
    emit modeChanged();
    // No emit changed(): the dark accessor VALUES don't change on a mode flip
    // (the dark cache is unchanged). Theme.qml's `isDark ? *Dark : *` tokens
    // re-evaluate off isDark, which reacts to resolvedDarkChanged directly.
    if (resolvedDark() != wasDark)
        emit resolvedDarkChanged();
}

bool ThemeViewModel::resolvedDark() const
{
    if (m_mode == QLatin1String("Dark"))  return true;
    if (m_mode == QLatin1String("Light")) return false;
    return schemeIsDark(m_systemScheme);  // System
}

bool ThemeViewModel::schemeIsDark(Qt::ColorScheme s)
{
    return s == Qt::ColorScheme::Dark;
}

void ThemeViewModel::applySystemColorScheme(Qt::ColorScheme scheme)
{
    if (scheme == m_systemScheme)
        return;
    const bool wasDark = resolvedDark();
    m_systemScheme = scheme;
    if (resolvedDark() != wasDark)
        emit resolvedDarkChanged();   // isDark re-evaluates; no changed() needed
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
