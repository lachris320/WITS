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

    QColor adminPrimary() const      { return BrandTheme::current().brandBase; }
    QColor adminPrimaryHover() const { return BrandTheme::current().brandDeep; }
    QColor adminOnPrimary() const    { return BrandTheme::current().brandOn; }
    QColor adminPrimarySoft() const  { return BrandTheme::current().brandSoft; }
    QColor kioskPrimary() const      { return BrandTheme::current().accentBase; }
    QColor kioskPrimaryHover() const { return BrandTheme::current().accentDeep; }
    QColor kioskOnPrimary() const    { return BrandTheme::current().accentOn; }
    QColor kioskPrimarySoft() const  { return BrandTheme::current().accentSoft; }
    QColor secondary() const         { return BrandTheme::current().accentBase; }
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
