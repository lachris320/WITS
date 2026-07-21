#ifndef THEMEVIEWMODEL_H
#define THEMEVIEWMODEL_H

#include <QObject>
#include <QColor>
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

    Q_PROPERTY(QColor adminPrimary      READ adminPrimary      NOTIFY changed)
    Q_PROPERTY(QColor adminPrimaryHover READ adminPrimaryHover NOTIFY changed)
    Q_PROPERTY(QColor adminOnPrimary    READ adminOnPrimary    NOTIFY changed)
    Q_PROPERTY(QColor adminPrimarySoft  READ adminPrimarySoft  NOTIFY changed)
    Q_PROPERTY(QColor kioskPrimary      READ kioskPrimary      NOTIFY changed)
    Q_PROPERTY(QColor kioskPrimaryHover READ kioskPrimaryHover NOTIFY changed)
    Q_PROPERTY(QColor kioskOnPrimary    READ kioskOnPrimary    NOTIFY changed)
    Q_PROPERTY(QColor kioskPrimarySoft  READ kioskPrimarySoft  NOTIFY changed)
    Q_PROPERTY(QColor secondary         READ secondary         NOTIFY changed)

    // Role-named properties (Phase 4d). Same underlying colours as the
    // deprecated block above; old names now forward to these accessors.
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

public:
    explicit ThemeViewModel(QObject *parent = nullptr);

    // DEPRECATED API aliases — removed in PR 2. Forward to the new accessors so
    // there is one source of truth (no second copy of any colour). secondary()
    // and kioskPrimary() both forward to accentBase() intentionally: two
    // deprecated names for one role.
    QColor adminPrimary() const      { return brandBase(); }
    QColor adminPrimaryHover() const { return brandDeep(); }
    QColor adminOnPrimary() const    { return brandOn(); }
    QColor adminPrimarySoft() const  { return brandSoft(); }
    QColor kioskPrimary() const      { return accentBase(); }
    QColor kioskPrimaryHover() const { return accentDeep(); }
    QColor kioskOnPrimary() const    { return accentOn(); }
    QColor kioskPrimarySoft() const  { return accentSoft(); }
    QColor secondary() const         { return accentBase(); }

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

    // Re-notify QML after an external BrandTheme::setCurrent (e.g. a remote
    // branding config arriving in a later phase).
    Q_INVOKABLE void refresh();

    // Live re-theme hook (§13.2). Auto mode re-extracts from the logo, applies
    // it via BrandTheme::setCurrent, and emits changed(); Manual mode is a
    // no-op returning false (brandtheme.cpp:414-419). Returns success.
    Q_INVOKABLE bool regenerateFromImportedLogo(const QString &path);

signals:
    void changed();

private:
    BrandingConfig m_config; // scratch for regenerateFromImportedLogo only; NOT a palette cache
};

#endif // THEMEVIEWMODEL_H
