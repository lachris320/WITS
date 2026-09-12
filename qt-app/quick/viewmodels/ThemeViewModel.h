#ifndef THEMEVIEWMODEL_H
#define THEMEVIEWMODEL_H

#include <QObject>
#include <QColor>
#include <QString>
#include <qnamespace.h>   // Qt::ColorScheme
#include <qqml.h>
#include "brandtheme.h"
#include "brandthemedata.h"

// QML-facing wrapper over the free-function brand engine (brandtheme.h:67-68).
// Does NOT modify BrandTheme: it only calls the engine's public API and emits
// changed() so QML property bindings re-evaluate (BrandTheme is deliberately
// not a QObject). Exposes every BrandPalette role as a read-only QColor.
class ThemeViewModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // Role-named properties (Phase 4d) — the single source of truth for the
    // brand/accent colours, read directly from the engine.
    Q_PROPERTY(QColor brandBase     READ brandBase     NOTIFY changed)
    Q_PROPERTY(QColor brandDeep     READ brandDeep     NOTIFY changed)
    Q_PROPERTY(QColor brandSoft     READ brandSoft     NOTIFY changed)
    Q_PROPERTY(QColor brandOn       READ brandOn       NOTIFY changed)
    Q_PROPERTY(QColor brandOnMuted  READ brandOnMuted  NOTIFY changed)
    Q_PROPERTY(QColor brandText     READ brandText     NOTIFY changed)
    Q_PROPERTY(QColor accentBase    READ accentBase    NOTIFY changed)
    Q_PROPERTY(QColor accentDeep    READ accentDeep    NOTIFY changed)
    Q_PROPERTY(QColor accentSoft    READ accentSoft    NOTIFY changed)
    Q_PROPERTY(QColor accentOn      READ accentOn      NOTIFY changed)
    Q_PROPERTY(QColor accentText    READ accentText    NOTIFY changed)

    Q_PROPERTY(QColor sidebarBase       READ sidebarBase       NOTIFY changed)
    Q_PROPERTY(QColor card              READ card              NOTIFY changed)
    Q_PROPERTY(QColor appBackground     READ appBackground     NOTIFY changed)
    Q_PROPERTY(QColor border            READ border            NOTIFY changed)
    Q_PROPERTY(QColor text              READ text              NOTIFY changed)
    Q_PROPERTY(QColor mutedText         READ mutedText         NOTIFY changed)
    Q_PROPERTY(QColor success           READ success           NOTIFY changed)
    Q_PROPERTY(QColor error             READ error             NOTIFY changed)

    // Dark-surface roles (Phase 5) — Theme.qml selects between the light role
    // above and its *Dark twin here via isDark. Backed by m_darkCache.
    Q_PROPERTY(QColor cardDark          READ cardDark          NOTIFY changed)
    Q_PROPERTY(QColor appBackgroundDark READ appBackgroundDark NOTIFY changed)
    Q_PROPERTY(QColor borderDark        READ borderDark        NOTIFY changed)
    Q_PROPERTY(QColor textDark          READ textDark          NOTIFY changed)
    Q_PROPERTY(QColor mutedTextDark     READ mutedTextDark     NOTIFY changed)
    Q_PROPERTY(QColor successDark       READ successDark       NOTIFY changed)
    Q_PROPERTY(QColor errorDark         READ errorDark         NOTIFY changed)
    Q_PROPERTY(QColor sidebarBaseDark   READ sidebarBaseDark   NOTIFY changed)
    Q_PROPERTY(QColor brandTextDark     READ brandTextDark     NOTIFY changed)
    Q_PROPERTY(QColor brandSoftDark     READ brandSoftDark     NOTIFY changed)
    Q_PROPERTY(QColor brandOnMutedDark  READ brandOnMutedDark  NOTIFY changed)
    Q_PROPERTY(QColor accentTextDark    READ accentTextDark    NOTIFY changed)
    Q_PROPERTY(QColor accentSoftDark    READ accentSoftDark    NOTIFY changed)
    Q_PROPERTY(QColor brandBaseDark     READ brandBaseDark     NOTIFY changed)
    Q_PROPERTY(QColor brandDeepDark     READ brandDeepDark     NOTIFY changed)

    // Theme mode (Phase 5). mode is Light|Dark|System; resolvedDark folds mode
    // with the OS colorScheme (System-only). Surface-scoping to admin lives in
    // Theme.qml, not here.
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool resolvedDark READ resolvedDark NOTIFY resolvedDarkChanged)

public:
    // Outcome of a logo-driven re-theme, exposed to QML (QML sees
    // ThemeViewModel.Ok / .FellBack / .Failed via Q_ENUM + QML_ELEMENT):
    //   Ok       — a usable logo palette was derived and applied,
    //   FellBack — the logo read but its palette failed the quality gate, so the
    //              fallback palette was applied (still a valid visible result),
    //   Failed   — the logo could not be read/decoded (or Manual mode).
    // Q_ENUM requires a member enum of a Q_OBJECT class; the engine returns
    // bool + BrandingConfig::didFallBack, mapped to this in the .cpp.
    enum class RegenResult { Ok, FellBack, Failed };
    Q_ENUM(RegenResult)

    explicit ThemeViewModel(QObject *parent = nullptr);

    // Role-named accessors (Phase 4d) — single source of truth for the
    // brand/accent colours; read directly from the engine.
    QColor brandBase() const     { return BrandTheme::current().brandBase; }
    QColor brandDeep() const     { return BrandTheme::current().brandDeep; }
    QColor brandSoft() const     { return BrandTheme::current().brandSoft; }
    QColor brandOn() const       { return BrandTheme::current().brandOn; }
    QColor brandOnMuted() const  { return BrandTheme::current().brandOnMuted; }
    QColor brandText() const     { return BrandTheme::current().brandText; }
    QColor accentBase() const    { return BrandTheme::current().accentBase; }
    QColor accentDeep() const    { return BrandTheme::current().accentDeep; }
    QColor accentSoft() const    { return BrandTheme::current().accentSoft; }
    QColor accentOn() const      { return BrandTheme::current().accentOn; }
    QColor accentText() const    { return BrandTheme::current().accentText; }

    QColor sidebarBase() const       { return BrandTheme::current().sidebarBase; }
    QColor card() const              { return BrandTheme::current().card; }
    QColor appBackground() const     { return BrandTheme::current().appBackground; }
    QColor border() const            { return BrandTheme::current().border; }
    QColor text() const              { return BrandTheme::current().text; }
    QColor mutedText() const         { return BrandTheme::current().mutedText; }
    QColor success() const           { return BrandTheme::current().success; }
    QColor error() const             { return BrandTheme::current().error; }

    QColor cardDark() const          { return m_darkCache.card; }
    QColor appBackgroundDark() const { return m_darkCache.appBackground; }
    QColor borderDark() const        { return m_darkCache.border; }
    QColor textDark() const          { return m_darkCache.text; }
    QColor mutedTextDark() const     { return m_darkCache.mutedText; }
    QColor successDark() const       { return m_darkCache.success; }
    QColor errorDark() const         { return m_darkCache.error; }
    QColor sidebarBaseDark() const   { return m_darkCache.sidebarBase; }
    QColor brandTextDark() const     { return m_darkCache.brandText; }
    QColor brandSoftDark() const     { return m_darkCache.brandSoft; }
    QColor brandOnMutedDark() const  { return m_darkCache.brandOnMuted; }
    QColor accentTextDark() const    { return m_darkCache.accentText; }
    QColor accentSoftDark() const    { return m_darkCache.accentSoft; }
    QColor brandBaseDark() const     { return m_darkCache.brandBase; }
    QColor brandDeepDark() const     { return m_darkCache.brandDeep; }

    // Re-notify QML after an external BrandTheme::setCurrent (e.g. a remote
    // branding config arriving in a later phase).
    Q_INVOKABLE void refresh();

    // Live re-theme hook (§13.2). Auto mode re-extracts from the logo, applies
    // it via BrandTheme::setCurrent, and emits changed() (for both Ok and
    // FellBack — the fallback palette IS the correct visible result); Manual
    // mode / unreadable logo is a no-op returning Failed. See RegenResult.
    Q_INVOKABLE RegenResult regenerateFromImportedLogo(const QString &path);

    QString mode() const { return m_mode; }
    // Q_INVOKABLE (not just the property WRITE) so QuickTests can call
    // `Theme._vm.setMode("Dark")` directly — a plain WRITE-only method isn't
    // registered as a callable meta-method for QML/JS call syntax.
    Q_INVOKABLE void setMode(const QString &mode);   // persists theme/mode, recomputes resolvedDark
    bool resolvedDark() const;

    // System-appearance seam (testable): the ctor connects
    // QStyleHints::colorSchemeChanged to this; tests call it directly so the
    // System branch never depends on the host OS appearance.
    void applySystemColorScheme(Qt::ColorScheme scheme);

signals:
    void changed();
    void modeChanged();
    void resolvedDarkChanged();

private:
    BrandingConfig m_config; // scratch for regenerateFromImportedLogo only; NOT a palette cache
    void loadMode();
    static bool schemeIsDark(Qt::ColorScheme s);
    QString m_mode = QStringLiteral("System");
    Qt::ColorScheme m_systemScheme = Qt::ColorScheme::Unknown;

    void rebuildDarkCache();     // m_darkCache = darkPalette(current())
    BrandPalette m_darkCache;    // derived dark palette; kept in sync with BrandTheme::current()
};

#endif // THEMEVIEWMODEL_H
