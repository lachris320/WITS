# Brand Theme Engine (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a logo-derived dynamic branding engine (BrandTheme) as an additive layer over `theme.h`, with hybrid persistence (new `branding_config` MySQL table + PHP endpoints, QSettings local cache) and a cache-first → background-sync → reapply-if-newer startup path — zero regressions.

**Architecture:** Pure color math lives in a header-only `brandcolormath.h` (theme.h style). `brandtheme.h/.cpp` owns logo validation, deterministic palette extraction, JSON serialization, the QSettings cache, the Manual-mode hook, and the process-wide current palette. `brandingcontroller.h/.cpp` is a thin Qt controller (injected `QNetworkAccessManager`, pure static parser) matching the existing `VisitorController` pattern. Backend adds `get_branding.php` (public read, mirrors `get_visitors.php`) and `save_branding.php` (admin-auth write, mirrors the `auth_helper.php` pattern). No existing file's behavior changes; app integration is additive lines only.

**Tech Stack:** Qt 6.11 Widgets/Gui/Svg/Network/Test, C++17, CMake+Ninja (MinGW), PHP 8 + mysqli (XAMPP), MySQL/MariaDB.

## Global Constraints

- Spec: `docs/LOAMS_UI_Modernization_PRD.md` — Phase 1 only. Stay on Qt Widgets; NO QML.
- MUST NOT change: existing business logic, workflows, API/JSON contracts, existing DB schema, auth, signal/slot connections, or widget object names. Existing tests are the regression baseline; **only `tst_theme.cpp` may change**.
- `theme.h` `Color::*` constants become the fallback palette **verbatim** (e.g. fallback `adminPrimaryHover` is the constant `#1D4ED8`, not a computed shade). Preserve distinct AdminPrimary vs KioskPrimary roles — map logo primary onto Admin, logo secondary/accent onto Kiosk; never collapse them.
- Fallback-palette exemption: the legacy kiosk green `#10B981` has < 3.0 contrast vs white; the fallback palette is legacy-faithful (kioskOnPrimary = white) and is **exempt** from the generated-contrast rule. Contrast assertions apply to logo-extracted palettes only.
- Derivation rules come from the attached design system (`../Library System UI Modernization/`): hover/deep = `shade(color, -0.28)`; soft tint = `mix(color, #FFFFFF, 0.90)`; on-accent text may be a dark deep shade (`shade(color, -0.60)`) when white fails — the design uses `--brand-deep` text on gold buttons.
- Defined minimum contrast ratio: **3.0** (WCAG 2.1 non-text UI component minimum) between each primary and its on-color.
- Logo formats: SVG/PNG preferred, JPG/BMP supported, WEBP best-effort (missing qwebp plugin → clear "unsupported" error, never crash), ICO optional. Corrupt/unsupported files rejected with a specific error message.
- Manual mode = code hook ONLY: a `theme_mode == Manual` branch that skips auto-regen. NO UI.
- Backend files follow existing patterns exactly: `include "db.php"`, `requireAdminAuth($conn)` for writes, `{"status": "success"|"error", ...}` JSON envelope. New PHP files only — zero edits to existing PHP.
- Security hygiene: no secrets, no real PII, no personal paths in committed files.
- NEVER stage `qt-app/adminwindow.ui` from the main checkout (it carries the user's own uncommitted edit). Work happens in a worktree branched from HEAD, so it is absent there anyway.
- Commits follow the `commit` skill's Conventional Commits conventions; subjects are pre-specified per task.
- Build toolchain (tools NOT on PATH — prepend in PowerShell):
  `$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH`
  Configure: `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`
  Build: `cmake --build qt-app/build` · Test: `ctest --test-dir qt-app/build --output-on-failure`
  Ignore: "LF will be replaced by CRLF" warnings and the QXlsx GuiPrivate CMake warning.
- Baseline: 10 green test targets (tst_rfidscandetector, tst_rfidkeyboardfilter, tst_apiconfig, tst_theme, tst_responsive_ui, tst_visitorcontroller, tst_importcontroller, tst_studentcontroller, tst_reportcontroller, tst_reportrenderer). Plan adds tst_brandtheme (Task 1) and tst_brandingcontroller (Task 5) → 12 targets at branch end.

---

### Task 1: Color math, data types, fallback palette, JSON round-trip

**Files:**
- Create: `qt-app/brandcolormath.h`
- Create: `qt-app/brandthemedata.h`
- Create: `qt-app/brandtheme.h`
- Create: `qt-app/brandtheme.cpp`
- Create: `qt-app/tests/tst_brandtheme.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (append new target at end)

**Interfaces:**
- Consumes: `WitsTheme::Color::*` constants from `qt-app/theme.h`.
- Produces (later tasks rely on these exact names):
  - `BrandColorMath::relativeLuminance(QColor) -> double`, `contrastRatio(QColor, QColor) -> double`, `shade(QColor, double) -> QColor`, `mix(QColor, QColor, double) -> QColor`
  - `enum class ThemeMode { Auto, Manual };`
  - `struct BrandPalette` (fields below) with `operator==`/`operator!=`
  - `struct BrandingConfig { ThemeMode mode; BrandPalette palette; QString logoHash; QDateTime updatedAt; }`
  - `BrandTheme::fallbackPalette() -> BrandPalette`
  - `BrandTheme::MinContrast` (`inline constexpr double = 3.0`)
  - `BrandTheme::paletteToJson(const BrandPalette&) -> QJsonObject`, `BrandTheme::paletteFromJson(const QJsonObject&) -> BrandPalette`
  - `BrandTheme::configToJson(const BrandingConfig&) -> QJsonObject`, `BrandTheme::configFromJson(const QJsonObject&) -> BrandingConfig`

- [ ] **Step 1: Write `qt-app/brandcolormath.h`** (complete file):

```cpp
#ifndef BRANDCOLORMATH_H
#define BRANDCOLORMATH_H

#include <QColor>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

// Header-only, pure color math shared by the brand-theme engine and the
// theme tests. Header-only (like theme.h / apiconfig.h) so it links into
// the app and every test target with no extra .cpp.
//
// shade() and mix() reproduce the attached design system's own JS
// derivation functions, so palettes derived here match the HTML mockups:
//   brand-deep = shade(brand, -0.28)   brand-soft = mix(brand, white, 0.90)
namespace BrandColorMath {

// WCAG 2.1 relative luminance of an sRGB color, in [0, 1].
inline double relativeLuminance(const QColor &c)
{
    const auto lin = [](double s) {
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

// WCAG 2.1 contrast ratio between two colors, in [1, 21].
inline double contrastRatio(const QColor &a, const QColor &b)
{
    const double la = relativeLuminance(a);
    const double lb = relativeLuminance(b);
    const double lighter = std::max(la, lb);
    const double darker  = std::min(la, lb);
    return (lighter + 0.05) / (darker + 0.05);
}

// Per-channel multiply by (1 + amt), clamped; amt in [-1, 1]. Negative
// darkens, positive lightens. Mirrors the design system's shade().
inline QColor shade(const QColor &c, double amt)
{
    const auto f = [amt](int v) {
        return qBound(0, qRound(v * (1.0 + amt)), 255);
    };
    return QColor(f(c.red()), f(c.green()), f(c.blue()), c.alpha());
}

// Linear per-channel interpolation from c toward `toward` by t in [0, 1].
// Mirrors the design system's mix().
inline QColor mix(const QColor &c, const QColor &toward, double t)
{
    const auto ch = [t](int x, int y) { return qRound(x + (y - x) * t); };
    return QColor(ch(c.red(),   toward.red()),
                  ch(c.green(), toward.green()),
                  ch(c.blue(),  toward.blue()),
                  c.alpha());
}

} // namespace BrandColorMath

#endif // BRANDCOLORMATH_H
```

- [ ] **Step 2: Write `qt-app/brandthemedata.h`** (complete file):

```cpp
#ifndef BRANDTHEMEDATA_H
#define BRANDTHEMEDATA_H

#include <QColor>
#include <QDateTime>
#include <QString>

// Value types for the brand-theme engine. Plain structs (like settingsdata.h
// / visitordata.h) so they link into the app and test targets with no .cpp.

// Manual mode is a code hook only in v1: regeneration is skipped when a
// config says Manual; there is NO UI that sets it.
enum class ThemeMode { Auto, Manual };

// Role palette the branding engine produces. Brand roles derive from the
// logo in Auto mode; neutral roles always keep the WitsTheme fallback
// values in v1. Admin and Kiosk stay distinct roles by design — the logo's
// primary maps onto Admin, its secondary/accent onto Kiosk.
struct BrandPalette
{
    // Brand roles (logo-derived in Auto mode)
    QColor adminPrimary;
    QColor adminPrimaryHover;
    QColor adminOnPrimary;    // text/icon color on adminPrimary fills
    QColor adminPrimarySoft;  // tinted background (design: brand-soft)
    QColor kioskPrimary;
    QColor kioskPrimaryHover;
    QColor kioskOnPrimary;
    QColor kioskPrimarySoft;
    QColor secondary;

    // Neutral roles (fallback values in v1)
    QColor sidebarBase;
    QColor card;
    QColor appBackground;
    QColor border;
    QColor text;
    QColor mutedText;
    QColor success;
    QColor error;

    bool operator==(const BrandPalette &o) const
    {
        return adminPrimary == o.adminPrimary
            && adminPrimaryHover == o.adminPrimaryHover
            && adminOnPrimary == o.adminOnPrimary
            && adminPrimarySoft == o.adminPrimarySoft
            && kioskPrimary == o.kioskPrimary
            && kioskPrimaryHover == o.kioskPrimaryHover
            && kioskOnPrimary == o.kioskOnPrimary
            && kioskPrimarySoft == o.kioskPrimarySoft
            && secondary == o.secondary
            && sidebarBase == o.sidebarBase
            && card == o.card
            && appBackground == o.appBackground
            && border == o.border
            && text == o.text
            && mutedText == o.mutedText
            && success == o.success
            && error == o.error;
    }
    bool operator!=(const BrandPalette &o) const { return !(*this == o); }
};

// One persisted branding state — what the cache holds locally and the
// branding_config table holds remotely.
struct BrandingConfig
{
    ThemeMode    mode = ThemeMode::Auto;
    BrandPalette palette;   // callers default this to BrandTheme::fallbackPalette()
    QString      logoHash;  // SHA-256 hex of the source logo file bytes
    QDateTime    updatedAt; // invalid == never branded
};

#endif // BRANDTHEMEDATA_H
```

- [ ] **Step 3: Write `qt-app/brandtheme.h`** (complete file — declares everything Tasks 2–3 implement; Task 1 implements only the functions listed in this task's Step 5):

```cpp
#ifndef BRANDTHEME_H
#define BRANDTHEME_H

#include <QJsonObject>
#include <QString>
#include "brandthemedata.h"

class QSettings;

// Logo-derived dynamic branding engine — an additive layer on top of
// theme.h. WitsTheme::Color::* is the fallback palette; extraction maps
// the logo's dominant color onto the Admin role and its secondary color
// onto the Kiosk role (distinct roles, never collapsed).
//
// Deterministic by construction: extraction is pure integer math over the
// decoded image — the same logo always yields the same palette.
namespace BrandTheme {

// Minimum contrast ratio between a primary and its on-color (WCAG 2.1
// non-text UI component minimum). Applies to generated palettes; the
// legacy-faithful fallback palette is exempt (see fallbackPalette()).
inline constexpr double MinContrast = 3.0;

// The legacy palette, verbatim from WitsTheme::Color::* (theme.h).
// kioskOnPrimary is white for fidelity with the existing kiosk styling
// even though the legacy green fails MinContrast — fallback is exempt.
BrandPalette fallbackPalette();

// --- Logo pipeline (Task 2) ---
// Validate a logo file: existence, extension whitelist (svg png jpg jpeg
// bmp webp ico), plugin availability (webp), and a full decode so corrupt
// files are caught. On failure returns false and sets *errorMsg to a
// specific, user-readable reason. Never crashes on bad input.
bool validateLogoFile(const QString &logoPath, QString *errorMsg);

// Extract a deterministic, contrast-enforced palette from a logo. On any
// validation/decode failure returns fallbackPalette() and sets *errorMsg.
// A logo with no usable chromatic pixels (e.g. pure greyscale) returns
// fallbackPalette() with *errorMsg cleared.
BrandPalette extractPalette(const QString &logoPath, QString *errorMsg);

// --- Serialization (Task 1) ---
QJsonObject paletteToJson(const BrandPalette &p);
BrandPalette paletteFromJson(const QJsonObject &o); // missing/invalid fields -> fallback values
QJsonObject configToJson(const BrandingConfig &c);
BrandingConfig configFromJson(const QJsonObject &o);

// --- Local cache (Task 3) --- QSettings is injected so tests use a
// temp-file INI store; the app passes QSettings("MyCompany", "MyApp").
void saveCachedConfig(QSettings &store, const BrandingConfig &config);
BrandingConfig loadCachedConfig(QSettings &store); // never-branded -> mode Auto + fallback palette

// True when `remote` should replace `local` (remote has a valid, strictly
// newer updatedAt; a valid remote always beats a never-branded local).
bool isNewer(const BrandingConfig &remote, const BrandingConfig &local);

// Regenerate config's palette from a logo. THE MANUAL-MODE HOOK: when
// config.mode == ThemeMode::Manual this skips auto-regeneration entirely —
// returns false with *errorMsg cleared, config untouched. In Auto mode it
// re-extracts, updates palette/logoHash/updatedAt, and returns true;
// validation failures return false with *errorMsg set.
bool regenerateFromLogo(BrandingConfig &config, const QString &logoPath,
                        QString *errorMsg);

// Process-wide current palette (defaults to fallbackPalette()). P2 UI
// consumers read current(); Phase 1 only maintains it.
const BrandPalette &current();
void setCurrent(const BrandPalette &palette);

} // namespace BrandTheme

#endif // BRANDTHEME_H
```

- [ ] **Step 4: Write the failing tests** — create `qt-app/tests/tst_brandtheme.cpp` with the Task 1 test slots (the file grows in Tasks 2–3; write it with only these slots now):

```cpp
#include <QtTest>
#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include "brandcolormath.h"
#include "brandtheme.h"
#include "theme.h"

class TestBrandTheme : public QObject
{
    Q_OBJECT
private slots:
    // Color math
    void contrastBlackWhiteIsMax();
    void contrastSameColorIsOne();
    void shadeDarkensAndClamps();
    void mixReachesEndpoints();

    // Fallback palette
    void fallbackMatchesWitsThemeConstants();
    void fallbackKeepsAdminAndKioskDistinct();

    // JSON round-trip
    void paletteJsonRoundTrip();
    void configJsonRoundTrip();
    void paletteFromJsonMissingFieldsFallsBack();
};

void TestBrandTheme::contrastBlackWhiteIsMax()
{
    const double r = BrandColorMath::contrastRatio(QColor(Qt::black), QColor(Qt::white));
    QVERIFY2(r > 20.9 && r <= 21.0, qPrintable(QString::number(r)));
}

void TestBrandTheme::contrastSameColorIsOne()
{
    QCOMPARE(BrandColorMath::contrastRatio(QColor("#7E1A15"), QColor("#7E1A15")), 1.0);
}

void TestBrandTheme::shadeDarkensAndClamps()
{
    const QColor base(100, 200, 50);
    const QColor darker = BrandColorMath::shade(base, -0.28);
    QCOMPARE(darker, QColor(72, 144, 36));
    QCOMPARE(BrandColorMath::shade(QColor(200, 200, 200), 1.0), QColor(255, 255, 255));
    QCOMPARE(BrandColorMath::shade(base, -1.0), QColor(0, 0, 0));
}

void TestBrandTheme::mixReachesEndpoints()
{
    const QColor a(10, 20, 30), b(210, 220, 230);
    QCOMPARE(BrandColorMath::mix(a, b, 0.0), a);
    QCOMPARE(BrandColorMath::mix(a, b, 1.0), b);
    QCOMPARE(BrandColorMath::mix(a, b, 0.5), QColor(110, 120, 130));
}

void TestBrandTheme::fallbackMatchesWitsThemeConstants()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    QCOMPARE(p.adminPrimary,      QColor(WitsTheme::Color::AdminPrimary));
    QCOMPARE(p.adminPrimaryHover, QColor(WitsTheme::Color::AdminPrimaryHover));
    QCOMPARE(p.kioskPrimary,      QColor(WitsTheme::Color::KioskPrimary));
    QCOMPARE(p.kioskPrimaryHover, QColor(WitsTheme::Color::KioskPrimaryHover));
    QCOMPARE(p.secondary,         QColor(WitsTheme::Color::Secondary));
    QCOMPARE(p.sidebarBase,       QColor(WitsTheme::Color::SidebarBase));
    QCOMPARE(p.card,              QColor(WitsTheme::Color::Card));
    QCOMPARE(p.appBackground,     QColor(WitsTheme::Color::AppBackground));
    QCOMPARE(p.border,            QColor(WitsTheme::Color::Border));
    QCOMPARE(p.text,              QColor(WitsTheme::Color::Text));
    QCOMPARE(p.mutedText,         QColor(WitsTheme::Color::MutedText));
    QCOMPARE(p.success,           QColor(WitsTheme::Color::Success));
    QCOMPARE(p.error,             QColor(WitsTheme::Color::Error));
    QCOMPARE(p.adminOnPrimary,    QColor(Qt::white));
    QCOMPARE(p.kioskOnPrimary,    QColor(Qt::white));
    QCOMPARE(p.adminPrimarySoft,
             BrandColorMath::mix(p.adminPrimary, QColor(Qt::white), 0.90));
    QCOMPARE(p.kioskPrimarySoft,
             BrandColorMath::mix(p.kioskPrimary, QColor(Qt::white), 0.90));
}

void TestBrandTheme::fallbackKeepsAdminAndKioskDistinct()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    QVERIFY(p.adminPrimary != p.kioskPrimary);
    QVERIFY(p.adminPrimaryHover != p.kioskPrimaryHover);
}

void TestBrandTheme::paletteJsonRoundTrip()
{
    const BrandPalette p = BrandTheme::fallbackPalette();
    const BrandPalette back = BrandTheme::paletteFromJson(BrandTheme::paletteToJson(p));
    QVERIFY(back == p);
}

void TestBrandTheme::configJsonRoundTrip()
{
    BrandingConfig c;
    c.mode = ThemeMode::Manual;
    c.palette = BrandTheme::fallbackPalette();
    c.logoHash = QStringLiteral("ab").repeated(32);
    c.updatedAt = QDateTime::fromString(QStringLiteral("2026-07-06 12:34:56"),
                                        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const BrandingConfig back = BrandTheme::configFromJson(BrandTheme::configToJson(c));
    QCOMPARE(back.mode, ThemeMode::Manual);
    QVERIFY(back.palette == c.palette);
    QCOMPARE(back.logoHash, c.logoHash);
    QCOMPARE(back.updatedAt, c.updatedAt);
}

void TestBrandTheme::paletteFromJsonMissingFieldsFallsBack()
{
    QJsonObject o;
    o.insert(QStringLiteral("admin_primary"), QStringLiteral("#7E1A15"));
    o.insert(QStringLiteral("kiosk_primary"), QStringLiteral("not-a-color"));
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    const BrandPalette fb = BrandTheme::fallbackPalette();
    QCOMPARE(p.adminPrimary, QColor("#7E1A15"));
    QCOMPARE(p.kioskPrimary, fb.kioskPrimary); // invalid -> fallback
    QCOMPARE(p.text, fb.text);                 // missing -> fallback
}

QTEST_GUILESS_MAIN(TestBrandTheme)
#include "tst_brandtheme.moc"
```

- [ ] **Step 5: Register the test target** — append to `qt-app/tests/CMakeLists.txt`:

```cmake
# 11th — brand-theme engine: pure color math, extraction, cache. Gui for
# QColor/QImage, Svg for QSvgRenderer logo rendering, offscreen because it
# never shows a widget but paints QImages.
qt_add_executable(tst_brandtheme
    tst_brandtheme.cpp
    ${CMAKE_SOURCE_DIR}/brandtheme.cpp
    ${CMAKE_SOURCE_DIR}/brandtheme.h
    ${CMAKE_SOURCE_DIR}/brandthemedata.h
    ${CMAKE_SOURCE_DIR}/brandcolormath.h
)
set_target_properties(tst_brandtheme PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_brandtheme PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_brandtheme PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Svg
)
add_test(NAME tst_brandtheme COMMAND tst_brandtheme)
set_tests_properties(tst_brandtheme PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 6: Write a stub `qt-app/brandtheme.cpp`** containing ONLY the Task 1 implementations (`fallbackPalette`, the four JSON functions) plus declared-but-unimplemented-elsewhere functions left OUT (they are declared in the header; do not define them yet — the test target only links what it calls... **it links the whole .cpp**, so provide minimal placeholder definitions for the Task 2/3 functions that return fallback/false so the target links; Tasks 2–3 replace them with real code and their own tests):

```cpp
#include "brandtheme.h"

#include <QJsonValue>
#include <QSettings>
#include "brandcolormath.h"
#include "theme.h"

using BrandColorMath::mix;

namespace BrandTheme {

BrandPalette fallbackPalette()
{
    const QColor white(Qt::white);
    BrandPalette p;
    // Brand roles — verbatim theme.h constants (the PRD's fallback rule).
    p.adminPrimary      = QColor(WitsTheme::Color::AdminPrimary);
    p.adminPrimaryHover = QColor(WitsTheme::Color::AdminPrimaryHover);
    p.adminOnPrimary    = white;
    p.adminPrimarySoft  = mix(p.adminPrimary, white, 0.90);
    p.kioskPrimary      = QColor(WitsTheme::Color::KioskPrimary);
    p.kioskPrimaryHover = QColor(WitsTheme::Color::KioskPrimaryHover);
    p.kioskOnPrimary    = white; // legacy-faithful; fallback is exempt from MinContrast
    p.kioskPrimarySoft  = mix(p.kioskPrimary, white, 0.90);
    p.secondary         = QColor(WitsTheme::Color::Secondary);
    // Neutral roles
    p.sidebarBase   = QColor(WitsTheme::Color::SidebarBase);
    p.card          = QColor(WitsTheme::Color::Card);
    p.appBackground = QColor(WitsTheme::Color::AppBackground);
    p.border        = QColor(WitsTheme::Color::Border);
    p.text          = QColor(WitsTheme::Color::Text);
    p.mutedText     = QColor(WitsTheme::Color::MutedText);
    p.success       = QColor(WitsTheme::Color::Success);
    p.error         = QColor(WitsTheme::Color::Error);
    return p;
}

namespace {

// One place defines the JSON field order/names for both directions.
struct FieldMap { const char *key; QColor BrandPalette::*member; };
const FieldMap kPaletteFields[] = {
    {"admin_primary",       &BrandPalette::adminPrimary},
    {"admin_primary_hover", &BrandPalette::adminPrimaryHover},
    {"admin_on_primary",    &BrandPalette::adminOnPrimary},
    {"admin_primary_soft",  &BrandPalette::adminPrimarySoft},
    {"kiosk_primary",       &BrandPalette::kioskPrimary},
    {"kiosk_primary_hover", &BrandPalette::kioskPrimaryHover},
    {"kiosk_on_primary",    &BrandPalette::kioskOnPrimary},
    {"kiosk_primary_soft",  &BrandPalette::kioskPrimarySoft},
    {"secondary",           &BrandPalette::secondary},
    {"sidebar_base",        &BrandPalette::sidebarBase},
    {"card",                &BrandPalette::card},
    {"app_background",      &BrandPalette::appBackground},
    {"border",              &BrandPalette::border},
    {"text",                &BrandPalette::text},
    {"muted_text",          &BrandPalette::mutedText},
    {"success",             &BrandPalette::success},
    {"error",               &BrandPalette::error},
};

const QString kDateTimeFormat = QStringLiteral("yyyy-MM-dd HH:mm:ss");

} // namespace

QJsonObject paletteToJson(const BrandPalette &p)
{
    QJsonObject o;
    for (const FieldMap &f : kPaletteFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    return o;
}

BrandPalette paletteFromJson(const QJsonObject &o)
{
    BrandPalette p = fallbackPalette();
    for (const FieldMap &f : kPaletteFields) {
        const QColor c(o.value(QLatin1String(f.key)).toString());
        if (c.isValid())
            p.*(f.member) = c;
    }
    return p;
}

QJsonObject configToJson(const BrandingConfig &c)
{
    QJsonObject o;
    o.insert(QStringLiteral("theme_mode"),
             c.mode == ThemeMode::Manual ? QStringLiteral("manual")
                                         : QStringLiteral("auto"));
    o.insert(QStringLiteral("palette"), paletteToJson(c.palette));
    o.insert(QStringLiteral("logo_hash"), c.logoHash);
    o.insert(QStringLiteral("updated_at"),
             c.updatedAt.isValid() ? c.updatedAt.toString(kDateTimeFormat)
                                   : QString());
    return o;
}

BrandingConfig configFromJson(const QJsonObject &o)
{
    BrandingConfig c;
    c.mode = o.value(QStringLiteral("theme_mode")).toString().trimmed().toLower()
                     == QLatin1String("manual")
                 ? ThemeMode::Manual
                 : ThemeMode::Auto;
    c.palette = paletteFromJson(o.value(QStringLiteral("palette")).toObject());
    c.logoHash = o.value(QStringLiteral("logo_hash")).toString();
    c.updatedAt = QDateTime::fromString(
        o.value(QStringLiteral("updated_at")).toString(), kDateTimeFormat);
    return c;
}

// --- Placeholders replaced by Task 2 (logo pipeline) ---
bool validateLogoFile(const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return false;
}
BrandPalette extractPalette(const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return fallbackPalette();
}

// --- Placeholders replaced by Task 3 (cache + hooks) ---
void saveCachedConfig(QSettings &, const BrandingConfig &) {}
BrandingConfig loadCachedConfig(QSettings &)
{
    BrandingConfig c;
    c.palette = fallbackPalette();
    return c;
}
bool isNewer(const BrandingConfig &, const BrandingConfig &) { return false; }
bool regenerateFromLogo(BrandingConfig &, const QString &, QString *errorMsg)
{
    if (errorMsg) *errorMsg = QStringLiteral("not implemented");
    return false;
}

const BrandPalette &current()
{
    static BrandPalette s_current = fallbackPalette();
    return s_current;
}
void setCurrent(const BrandPalette &) {}

} // namespace BrandTheme
```

- [ ] **Step 7: Run the new test to verify it fails first, then passes.** From the worktree root (PowerShell, PATH prepended per Global Constraints):

```
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build --target tst_brandtheme
ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure
```

Red first (build error before the sources exist / assertion failures if stubs wrong), then all 9 slots PASS.

- [ ] **Step 8: Run the FULL suite** — `ctest --test-dir qt-app/build --output-on-failure` → 11 targets pass (10 baseline + tst_brandtheme).

- [ ] **Step 9: Commit** — subject: `feat(brandtheme): add color math, palette types, fallback palette and JSON round-trip`

---

### Task 2: Logo validation + deterministic, contrast-enforced palette extraction

**Files:**
- Modify: `qt-app/brandtheme.cpp` (replace the two Task 2 placeholders; add internal helpers)
- Modify: `qt-app/tests/tst_brandtheme.cpp` (add slots + helpers)

**Interfaces:**
- Consumes: Task 1 types/math.
- Produces: working `BrandTheme::validateLogoFile` and `BrandTheme::extractPalette` with the exact contract in `brandtheme.h`.

**Extraction algorithm (normative — implement exactly):**
1. Decode via internal `renderLogo(path, *err) -> QImage` (64×64 ARGB32):
   - not found → err `Logo file not found: <path>`, null image
   - extension (lowercased suffix) not in `{svg,png,jpg,jpeg,bmp,webp,ico}` → err `Unsupported logo format ".<ext>" — use SVG, PNG, JPG or BMP.`
   - `svg`: `QSvgRenderer r(path)`; `!r.isValid()` → err `Invalid or corrupt SVG file: <path>`; else render onto a transparent-filled 64×64 ARGB32 QImage via QPainter.
   - `webp` when `!QImageReader::supportedImageFormats().contains("webp")` → err `WEBP logos are not supported on this system — use PNG or SVG instead.`
   - otherwise `QImageReader reader(path); reader.setAutoTransform(true); QImage img = reader.read();` null → err `Unable to decode logo (corrupt or unsupported): <path> (<reader.errorString()>)`; else `img.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32)`.
2. `validateLogoFile` = `!renderLogo(...).isNull()`.
3. Chromatic histogram: for each pixel with `qAlpha >= 128`, compute HSV; skip if `saturationF < 0.15` or `valueF < 0.15` *(Correction 2026-07-06: the original `valueF > 0.98` near-white ceiling also rejected fully-saturated pure hues (V = 1.0); near-white is already excluded by the saturation floor, so the ceiling was removed)*; bucket key = `((r>>4)<<8) | ((g>>4)<<4) | (b>>4)`; count per key.
4. Empty histogram (greyscale/blank logo) → return `fallbackPalette()`, clear err.
5. Primary seed = bucket with highest count; tie → lowest key (iterate sorted keys). Seed color = bucket center: `component = nibble*16 + 8`.
6. Secondary seed = highest-count bucket (same tie rule) whose hue differs from the primary seed's hue by ≥ 60° (circular distance, `qAbs` wrapped); if none exists, secondary = primary rotated +45°: `QColor::fromHsvF(fmod(h + 0.125, 1.0), s, v)`.
7. Build palette from `(primarySeed, secondarySeed)`:
   - `enforceOnWhite(c)`: while `contrastRatio(c, white) < MinContrast` and < 24 iterations, `c = shade(c, -0.08)`; return c.
   - `adminPrimary = enforceOnWhite(primarySeed)`; `adminOnPrimary = white`.
   - Kiosk (allows the design's dark-on-gold): if `contrastRatio(secondarySeed, white) >= MinContrast` → `kioskPrimary = secondarySeed`, `kioskOnPrimary = white`; else if `contrastRatio(secondarySeed, shade(secondarySeed, -0.60)) >= MinContrast` → `kioskPrimary = secondarySeed`, `kioskOnPrimary = shade(secondarySeed, -0.60)`; else `kioskPrimary = enforceOnWhite(secondarySeed)`, `kioskOnPrimary = white`.
   - Hovers = `shade(primary, -0.28)` each; softs = `mix(primary, white, 0.90)` each; `secondary = kioskPrimary`; neutrals copied from `fallbackPalette()`.

Named constants (anonymous namespace): `kSampleSize=64, kMinAlpha=128, kMinSaturation=0.15, kMinValue=0.15, kMinHueSeparationDeg=60.0, kHoverShade=-0.28, kSoftMixToWhite=0.90, kOnColorDeepShade=-0.60, kEnforceStep=-0.08, kEnforceMaxIterations=24`.

- [ ] **Step 1: Add failing tests** to `tst_brandtheme.cpp` — new slots + file-generating helpers (QTemporaryDir member `m_dir` created in `initTestCase`):

```cpp
// helpers (private section of the test class):
//   QString writePng(const QString &name, const QColor &fill);        // 64x64 solid QImage saved as PNG
//   QString writeTwoTonePng(const QString &name, const QColor &left, const QColor &right); // 32px halves
//   QString writeSvg(const QString &name, const QString &fillHex);    // rect SVG written as text
//   QString writeGarbage(const QString &name);                        // "this is not an image" bytes

void validPngProducesBrandedPalette();     // red PNG -> ok err empty; adminPrimary != fallback.adminPrimary; adminPrimary.isValid()
void validSvgProducesBrandedPalette();     // '#7E1A15' SVG -> same assertions
void corruptFileRejectedWithSpecificError();   // garbage ".png" -> validateLogoFile false; err contains "corrupt"; extractPalette returns fallback, err non-empty — and does not crash
void unsupportedExtensionRejected();       // garbage ".txt" -> err contains "Unsupported logo format"
void missingFileRejected();                // err contains "not found"
void webpNeverCrashes();                   // garbage ".webp": if plugin absent err contains "not supported"; else err contains "corrupt or unsupported"; both paths return false, no crash
void sameLogoTwiceIsDeterministic();       // extract twice from same red PNG -> palettes operator==
void differentLogosDifferentPalettes();    // red PNG vs blue PNG -> adminPrimary differs
void twoToneLogoMapsPrimaryAndSecondary(); // maroon|gold two-tone -> adminPrimary hue near maroon, kioskPrimary hue near gold, admin != kiosk
void generatedPalettesMeetMinContrast();   // for red, blue, pale-yellow (#F0E68C), dark-navy (#101840) PNGs: contrast(adminPrimary, adminOnPrimary) >= BrandTheme::MinContrast && contrast(kioskPrimary, kioskOnPrimary) >= BrandTheme::MinContrast
void greyscaleLogoFallsBack();             // solid #808080 PNG -> fallbackPalette(), err empty
```

Write each slot with complete assertions (the descriptions above are the required assertions; QVERIFY2 with the error text on failure). Two-tone hue check: `qAbs(adminPrimary.hsvHue() - QColor("#7E1A15").hsvHue()) <= 30` (and gold equivalent ≤ 30, hues wrapped).

- [ ] **Step 2: Run to verify the new slots fail** (placeholders return "not implemented"): `ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure` → new slots FAIL, Task 1 slots still pass.

- [ ] **Step 3: Implement** — replace the two placeholders in `brandtheme.cpp` with the normative algorithm (includes: `QFile`, `QFileInfo`, `QImage`, `QImageReader`, `QPainter`, `QSvgRenderer`, `QHash`, `<QtMath>`).

- [ ] **Step 4: Run tst_brandtheme until green**, then the FULL suite: `ctest --test-dir qt-app/build --output-on-failure` → 11/11.

- [ ] **Step 5: Commit** — subject: `feat(brandtheme): deterministic logo palette extraction with contrast enforcement`

---

### Task 3: QSettings cache, isNewer, Manual-mode hook, current palette

**Files:**
- Modify: `qt-app/brandtheme.cpp` (replace Task 3 placeholders)
- Modify: `qt-app/tests/tst_brandtheme.cpp` (add slots)

**Interfaces:**
- Consumes: Tasks 1–2.
- Produces: working `saveCachedConfig/loadCachedConfig/isNewer/regenerateFromLogo/current/setCurrent` per the header contract. Task 6 calls exactly these.

**Normative behavior:**
- Cache keys under `store.beginGroup("branding")`: `mode` (`"auto"`/`"manual"`), `palette` (compact JSON string of `paletteToJson`), `logoHash` (string), `updatedAt` (`toString(Qt::ISODate)`); `endGroup()` then `store.sync()` on save. Load mirrors with defaults: missing/empty → `ThemeMode::Auto`, `fallbackPalette()`, empty hash, invalid QDateTime.
- `isNewer(remote, local)`: `remote.updatedAt.isValid() && (!local.updatedAt.isValid() || remote.updatedAt > local.updatedAt)`.
- `regenerateFromLogo`: `if (config.mode == ThemeMode::Manual) { if (errorMsg) errorMsg->clear(); return false; }` — **this branch is the Manual-mode hook (PRD condition 8)**. Else `extractPalette`; if err non-empty → return false (config untouched). Else set `config.palette`, `config.logoHash` = SHA-256 hex of file bytes (`QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex()`), `config.updatedAt = QDateTime::currentDateTime()`, return true.
- `current()/setCurrent`: function-local static `BrandPalette` initialized to `fallbackPalette()`; setter assigns.

- [ ] **Step 1: Add failing tests** (temp INI store: `QSettings s(m_dir.path() + "/branding.ini", QSettings::IniFormat);`):

```cpp
void cacheRoundTrip();                 // save config (Manual, red-logo palette, 64-hex hash, valid updatedAt) -> load -> all fields equal
void cacheLoadNeverBranded();          // fresh ini -> Auto + fallbackPalette + empty hash + !updatedAt.isValid()
void isNewerComparisons();             // valid>older true; equal false; remote-invalid false; local-invalid+remote-valid true
void manualModeSkipsRegeneration();    // cfg.mode=Manual, palette=fallback; regenerateFromLogo(cfg, redPng, &err) == false; err.isEmpty(); cfg.palette unchanged; logoHash still empty
void autoModeRegenerates();            // cfg.mode=Auto -> true; palette != fallback; logoHash 64 lowercase hex; updatedAt valid
void currentDefaultsToFallbackAndSets(); // current()==fallback; setCurrent(extracted); current()==extracted; restore fallback at end
```

- [ ] **Step 2: Run to verify new slots fail.**
- [ ] **Step 3: Implement** (adds includes `QCryptographicHash`, `QFile`, `QSettings` already present).
- [ ] **Step 4: tst_brandtheme green, then FULL suite green (11/11).**
- [ ] **Step 5: Commit** — subject: `feat(brandtheme): QSettings cache, freshness compare and Manual-mode regen hook`

---

### Task 4: Backend — branding_config table + get/save endpoints

**Files:**
- Create: `deliverables/sql/branding_config.sql`
- Create: `deliverables/loams_api/get_branding.php`
- Create: `deliverables/loams_api/save_branding.php`

**Interfaces:**
- Consumes: existing `db.php` (`$conn` mysqli), `auth_helper.php` (`requireAdminAuth`).
- Produces: wire contract Task 5's parser consumes:
  - GET/POST `get_branding.php` → `{"status":"success","branding":null}` or `{"status":"success","branding":{"theme_mode":"auto","palette":{…},"logo_hash":"…","updated_at":"YYYY-MM-DD HH:MM:SS"}}`; `{"status":"error","message":…}` on failure.
  - POST `save_branding.php` form fields `admin_key`, `theme_mode`, `palette` (JSON string), `logo_hash` → `{"status":"success"}` / error envelope.
- **Zero edits to existing PHP files or existing tables** (PRD conditions 16–17).

- [ ] **Step 1: Write `deliverables/sql/branding_config.sql`** (complete file):

```sql
-- Branding engine persistence (Phase 1, PRD condition 5). One singleton row
-- (id = 1) holding the current branding state. New table only — no existing
-- table is altered.
CREATE TABLE IF NOT EXISTS branding_config (
  id           TINYINT UNSIGNED NOT NULL,
  theme_mode   VARCHAR(10)      NOT NULL DEFAULT 'auto',
  palette_json TEXT             NOT NULL,
  logo_hash    VARCHAR(64)      NOT NULL DEFAULT '',
  updated_at   TIMESTAMP        NOT NULL DEFAULT CURRENT_TIMESTAMP
                                ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

- [ ] **Step 2: Write `deliverables/loams_api/get_branding.php`** (complete file — mirrors `get_visitors.php`: public read, same envelope):

```php
<?php
header("Content-Type: application/json");
include "db.php";

$result = $conn->query("SELECT theme_mode, palette_json, logo_hash, updated_at
                        FROM branding_config WHERE id = 1");
if (!$result) {
    echo json_encode(["status" => "error", "message" => "Query failed: " . $conn->error]);
    exit;
}

$row = $result->fetch_assoc();
if (!$row) {
    echo json_encode(["status" => "success", "branding" => null]);
    exit;
}

$palette = json_decode($row["palette_json"], true);

echo json_encode([
    "status" => "success",
    "branding" => [
        "theme_mode" => $row["theme_mode"],
        "palette"    => is_array($palette) ? $palette : null,
        "logo_hash"  => $row["logo_hash"],
        "updated_at" => $row["updated_at"],
    ],
]);
?>
```

- [ ] **Step 3: Write `deliverables/loams_api/save_branding.php`** (complete file — admin-auth write with prepared statements, mirrors the `auth_helper.php` pattern):

```php
<?php
header("Content-Type: application/json");
include "db.php";
include "auth_helper.php";

requireAdminAuth($conn);

$theme_mode = isset($_POST['theme_mode']) ? strtolower(trim($_POST['theme_mode'])) : '';
$palette    = isset($_POST['palette']) ? $_POST['palette'] : '';
$logo_hash  = isset($_POST['logo_hash']) ? trim($_POST['logo_hash']) : '';

if (!in_array($theme_mode, ['auto', 'manual'], true)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "theme_mode must be 'auto' or 'manual'"]);
    exit;
}

$decoded = json_decode($palette, true);
if (!is_array($decoded)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "palette must be a JSON object"]);
    exit;
}

if ($logo_hash !== '' && !preg_match('/^[0-9a-f]{64}$/', $logo_hash)) {
    http_response_code(400);
    echo json_encode(["status" => "error", "message" => "logo_hash must be a 64-character SHA-256 hex string"]);
    exit;
}

$stmt = $conn->prepare("INSERT INTO branding_config (id, theme_mode, palette_json, logo_hash)
                        VALUES (1, ?, ?, ?)
                        ON DUPLICATE KEY UPDATE
                            theme_mode   = VALUES(theme_mode),
                            palette_json = VALUES(palette_json),
                            logo_hash    = VALUES(logo_hash)");
if (!$stmt) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "Prepare failed: " . $conn->error]);
    exit;
}

$stmt->bind_param("sss", $theme_mode, $palette, $logo_hash);
if (!$stmt->execute()) {
    http_response_code(500);
    echo json_encode(["status" => "error", "message" => "Save failed: " . $stmt->error]);
    exit;
}
$stmt->close();

echo json_encode(["status" => "success"]);
?>
```

- [ ] **Step 4: Lint both PHP files if PHP is available** (XAMPP: `C:\xampp\php\php.exe -l <file>`; if the binary is missing, note that lint was skipped and rely on Task 8's review). Expected: `No syntax errors detected`.

- [ ] **Step 5: Verify zero existing-file drift:** `git status --short deliverables/` shows only the three new files (`??`), no `M` entries.

- [ ] **Step 6: Commit** — subject: `feat(api): add branding_config table and get/save branding endpoints`

---

### Task 5: BrandingController (fetch/save + pure parser) + tests

**Files:**
- Create: `qt-app/brandingcontroller.h`
- Create: `qt-app/brandingcontroller.cpp`
- Create: `qt-app/tests/tst_brandingcontroller.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (append target)

**Interfaces:**
- Consumes: `ApiConfig::endpoint()`, `BrandTheme::configFromJson`/`configToJson`, `BrandingConfig`.
- Produces (Task 6 uses exactly these):
  - `explicit BrandingController(QNetworkAccessManager *nam, QObject *parent = nullptr);` (nam injected, not owned — matches VisitorController)
  - `void fetchRemoteConfig();` → emits `remoteConfigLoaded(const BrandingConfig &)` when the server has a config, nothing when `branding` is null, `fetchError(const QString &)` on network/parse errors.
  - `void saveBranding(const BrandingConfig &config, const QString &adminKey);` → POSTs `save_branding.php` (form-urlencoded: `admin_key`, `theme_mode`, `palette` compact JSON, `logo_hash`); emits `saveFinished(bool ok, const QString &message)`. **Code hook only in v1 — no caller UI; the admin key is a parameter because the client does not retain it after login.**
  - `static bool parseBrandingResponse(const QByteArray &json, BrandingConfig *out, bool *hasConfig, QString *errorMsg);`

- [ ] **Step 1: Write the header** (complete file):

```cpp
#ifndef BRANDINGCONTROLLER_H
#define BRANDINGCONTROLLER_H
#include <QByteArray>
#include <QObject>
#include <QString>
#include "brandthemedata.h"

class QNetworkAccessManager;

// Thin network controller for the branding backend (get_branding.php /
// save_branding.php), following the VisitorController pattern: injected
// QNetworkAccessManager, async signals, and a pure static parser that the
// unit test feeds synthetic payloads (no live network in tests).
class BrandingController : public QObject
{
    Q_OBJECT
public:
    explicit BrandingController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Async — result arrives via remoteConfigLoaded / fetchError. A server
    // with no branding row (branding: null) emits neither: there is simply
    // nothing to sync yet.
    void fetchRemoteConfig();

    // Async admin-auth write (code hook in v1 — no UI calls this yet; the
    // admin key is a parameter because the client does not retain it).
    void saveBranding(const BrandingConfig &config, const QString &adminKey);

    // Pure, unit-testable. Returns false + *errorMsg on malformed input or
    // status != success. On success *hasConfig says whether branding was
    // non-null and *out holds the decoded config (untouched when null).
    static bool parseBrandingResponse(const QByteArray &json,
                                      BrandingConfig *out,
                                      bool *hasConfig,
                                      QString *errorMsg);

signals:
    void remoteConfigLoaded(const BrandingConfig &config);
    void fetchError(const QString &message);
    void saveFinished(bool ok, const QString &message);

private:
    QNetworkAccessManager *m_nam; // injected, not owned
};

#endif // BRANDINGCONTROLLER_H
```

- [ ] **Step 2: Write failing tests** — `qt-app/tests/tst_brandingcontroller.cpp`, parser-only (matches how tst_visitorcontroller tests parseVisitorsResponse; no live HTTP):

```cpp
#include <QtTest>
#include <QByteArray>
#include "brandingcontroller.h"
#include "brandtheme.h"

class TestBrandingController : public QObject
{
    Q_OBJECT
private slots:
    void parseSuccessWithConfig();      // full payload -> true, hasConfig, mode/palette/hash/updatedAt decoded (updated_at "2026-07-06 08:00:00" parsed with yyyy-MM-dd HH:mm:ss)
    void parseSuccessNullBranding();    // {"status":"success","branding":null} -> true, hasConfig=false, out untouched
    void parseInvalidJson();            // "not json" -> false, errorMsg non-empty
    void parseStatusError();            // {"status":"error","message":"Query failed"} -> false, errorMsg contains "Query failed"
    void parseMalformedPaletteFallsBack(); // palette:"garbage" (non-object) -> true, hasConfig, palette == fallback
    void parseManualMode();             // theme_mode:"manual" -> ThemeMode::Manual
};
```

Write each slot fully, building payloads with raw string literals in the tst_visitorcontroller style; use `BrandTheme::fallbackPalette()` for palette comparisons. End with `QTEST_GUILESS_MAIN(TestBrandingController)` + moc include.

- [ ] **Step 3: Register the target** — append to `qt-app/tests/CMakeLists.txt`:

```cmake
# 12th — branding network controller: pure parser tests over synthetic
# payloads (no live HTTP). Links brandtheme.cpp for JSON/fallback, hence
# Gui+Svg alongside Network.
qt_add_executable(tst_brandingcontroller
    tst_brandingcontroller.cpp
    ${CMAKE_SOURCE_DIR}/brandingcontroller.cpp
    ${CMAKE_SOURCE_DIR}/brandingcontroller.h
    ${CMAKE_SOURCE_DIR}/brandtheme.cpp
    ${CMAKE_SOURCE_DIR}/brandtheme.h
    ${CMAKE_SOURCE_DIR}/brandthemedata.h
)
set_target_properties(tst_brandingcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_brandingcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_brandingcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Svg
)
add_test(NAME tst_brandingcontroller COMMAND tst_brandingcontroller)
set_tests_properties(tst_brandingcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 4: Run to verify failure**, then **Step 5: implement `brandingcontroller.cpp`**:

```cpp
#include "brandingcontroller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include "apiconfig.h"
#include "brandtheme.h"

BrandingController::BrandingController(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent)
    , m_nam(nam)
{}

void BrandingController::fetchRemoteConfig()
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("get_branding.php")));
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(reply->errorString());
            return;
        }
        BrandingConfig config;
        bool hasConfig = false;
        QString errorMsg;
        if (!parseBrandingResponse(reply->readAll(), &config, &hasConfig, &errorMsg)) {
            emit fetchError(errorMsg);
            return;
        }
        if (hasConfig)
            emit remoteConfigLoaded(config);
        // branding: null -> server not branded yet; nothing to sync.
    });
}

void BrandingController::saveBranding(const BrandingConfig &config, const QString &adminKey)
{
    QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("save_branding.php")));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("admin_key"), adminKey);
    postData.addQueryItem(QStringLiteral("theme_mode"),
                          config.mode == ThemeMode::Manual ? QStringLiteral("manual")
                                                           : QStringLiteral("auto"));
    postData.addQueryItem(QStringLiteral("palette"),
                          QString::fromUtf8(QJsonDocument(BrandTheme::paletteToJson(config.palette))
                                                .toJson(QJsonDocument::Compact)));
    postData.addQueryItem(QStringLiteral("logo_hash"), config.logoHash);

    QNetworkReply *reply =
        m_nam->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit saveFinished(false, reply->errorString());
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit saveFinished(false, QStringLiteral("Invalid server response."));
            return;
        }
        const QJsonObject obj = doc.object();
        const bool ok = obj.value(QStringLiteral("status")).toString()
                        == QLatin1String("success");
        emit saveFinished(ok, obj.value(QStringLiteral("message")).toString());
    });
}

bool BrandingController::parseBrandingResponse(const QByteArray &json,
                                               BrandingConfig *out,
                                               bool *hasConfig,
                                               QString *errorMsg)
{
    if (hasConfig) *hasConfig = false;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("Invalid branding response: not a JSON object.");
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        if (errorMsg) {
            *errorMsg = obj.value(QStringLiteral("message")).toString();
            if (errorMsg->isEmpty())
                *errorMsg = QStringLiteral("Branding request failed.");
        }
        return false;
    }
    const QJsonValue branding = obj.value(QStringLiteral("branding"));
    if (branding.isNull() || branding.isUndefined())
        return true; // server not branded yet
    if (out)
        *out = BrandTheme::configFromJson(branding.toObject());
    if (hasConfig) *hasConfig = true;
    return true;
}
```

(Needs `#include <QJsonValue>` too.)

- [ ] **Step 6: tst_brandingcontroller green, FULL suite green (12/12).**
- [ ] **Step 7: Commit** — subject: `feat(branding): add BrandingController for backend fetch/save with pure parser`

---

### Task 6: App integration — CMake, cache-first startup sync, logo-import hook

**Files:**
- Modify: `qt-app/CMakeLists.txt` (add new sources to the WITS target)
- Modify: `qt-app/mainwindow.h` (+2 lines: forward decl + member)
- Modify: `qt-app/mainwindow.cpp` (additive block in ctor)
- Modify: `qt-app/adminwindow.cpp` (one additional `connect` + includes)

**Interfaces:** Consumes Tasks 1–5 exactly as declared. No new tests (covered by unit tests; integration is additive wiring), but the FULL suite and both app builds must stay green.

**Hard rules:** Do NOT touch any existing statement — only insert new lines. No changes to `adminwindow.ui`, object names, or existing connects.

- [ ] **Step 1: CMake** — in `qt_add_executable(WITS ...)` add after the `reportrenderer.h reportrenderer.cpp` line:

```cmake
        brandcolormath.h
        brandthemedata.h
        brandtheme.h brandtheme.cpp
        brandingcontroller.h brandingcontroller.cpp
```

- [ ] **Step 2: `mainwindow.h`** — add `class BrandingController;` beside the existing forward declarations and a private member `BrandingController *m_brandingController = nullptr;`.

- [ ] **Step 3: `mainwindow.cpp` startup path (PRD condition 7)** — add includes `#include "brandtheme.h"` and `#include "brandingcontroller.h"`; insert at the END of the `MainWindow` constructor (after the existing QSettings block and connects, before the closing brace):

```cpp
    // Branding: cache-first, then background sync. The cached palette is
    // applied immediately (no network wait); the backend is fetched in the
    // background and re-applied + re-cached only if strictly newer.
    {
        QSettings brandingStore(QLatin1String("MyCompany"), QLatin1String("MyApp"));
        BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
    }
    m_brandingController = new BrandingController(networkManager, this);
    connect(m_brandingController, &BrandingController::remoteConfigLoaded, this,
            [](const BrandingConfig &remote) {
        QSettings store(QLatin1String("MyCompany"), QLatin1String("MyApp"));
        if (BrandTheme::isNewer(remote, BrandTheme::loadCachedConfig(store))) {
            BrandTheme::setCurrent(remote.palette);
            BrandTheme::saveCachedConfig(store, remote);
        }
    });
    m_brandingController->fetchRemoteConfig();
```

(`networkManager` is the existing MainWindow member used by `handleLogin`; verify its exact member name in `mainwindow.h` and use it.)

- [ ] **Step 4: `adminwindow.cpp` logo-import hook** — add includes `#include "brandtheme.h"`; directly AFTER the existing `connect(m_settingsController, &SettingsController::logoChanged, ...)` connect (around line 1171), add a second, additive connect:

```cpp
    // Branding engine: a newly imported logo regenerates the palette in
    // Auto mode (Manual mode skips regeneration — the v1 code hook).
    connect(m_settingsController, &SettingsController::logoChanged, this,
            [](const QString &destinationPath) {
        QSettings store(QLatin1String("MyCompany"), QLatin1String("MyApp"));
        BrandingConfig config = BrandTheme::loadCachedConfig(store);
        QString errorMsg;
        if (BrandTheme::regenerateFromLogo(config, destinationPath, &errorMsg)) {
            BrandTheme::setCurrent(config.palette);
            BrandTheme::saveCachedConfig(store, config);
        } else if (!errorMsg.isEmpty()) {
            qWarning() << "Branding regeneration skipped:" << errorMsg;
        }
    });
```

(Existing connects to `logoChanged` are untouched; multiple connects on one signal are additive. `QSettings` and `QDebug` includes already exist in adminwindow.cpp — verify, add if missing.)

- [ ] **Step 5: Build the app + run everything:** `cmake --build qt-app/build` (whole build, WITS target included) then `ctest --test-dir qt-app/build --output-on-failure` → 12/12.
- [ ] **Step 6: Commit** — subject: `feat(branding): cache-first startup sync and logo-import regeneration hook`

---

### Task 7: Rewrite tst_theme.cpp — static hex to behavioral assertions

**Files:**
- Modify: `qt-app/tests/tst_theme.cpp` (full rewrite — the ONLY existing test file Phase 1 may change)
- Modify: `qt-app/tests/CMakeLists.txt` — no changes needed (tst_theme links Widgets; `brandcolormath.h` is header-only and `${CMAKE_SOURCE_DIR}` is already an include dir). Verify only.

**Interfaces:** Consumes `WitsTheme` + `BrandColorMath::contrastRatio`.

- [ ] **Step 1: Rewrite** `tst_theme.cpp` (complete file):

```cpp
#include <QtTest>
#include <QPalette>
#include <QColor>
#include "brandcolormath.h"
#include "theme.h"

#ifndef WITS_QSS_PATH
#define WITS_QSS_PATH "resources/wits.qss"
#endif

// Behavioral theme assertions: the palette must satisfy the properties the
// fixed light theme exists to guarantee (dark legible text on light
// surfaces), without pinning any specific hex value — so a rebrand cannot
// break this suite unless it actually breaks legibility.
class TestTheme : public QObject
{
    Q_OBJECT
private slots:
    void baseSurfacesAreLight();
    void textIsLegibleOnItsSurfaces();      // contrast >= 4.5 (WCAG AA text)
    void highlightIsLegible();              // contrast >= 3.0 (UI component)
    void placeholderIsMutedButVisible();
    void disabledTextIsMutedButVisible();
    void namedColorsAreValid();
    void adminAndKioskRolesStayDistinct();
    void hoverVariantsAreDarker();
    void loadStyleSheetReturnsNonEmpty();
    void loadStyleSheetMissingFileIsEmpty();
};

void TestTheme::baseSurfacesAreLight()
{
    const QPalette p = WitsTheme::lightPalette();
    QVERIFY(p.color(QPalette::Base).lightness() > 128);
    QVERIFY(p.color(QPalette::Window).lightness() > 128);
    QVERIFY(p.color(QPalette::Button).lightness() > 128);
}

void TestTheme::textIsLegibleOnItsSurfaces()
{
    const QPalette p = WitsTheme::lightPalette();
    using BrandColorMath::contrastRatio;
    QVERIFY(contrastRatio(p.color(QPalette::Text), p.color(QPalette::Base)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::WindowText), p.color(QPalette::Window)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::ButtonText), p.color(QPalette::Button)) >= 4.5);
    QVERIFY(contrastRatio(p.color(QPalette::ToolTipText), p.color(QPalette::ToolTipBase)) >= 4.5);
}

void TestTheme::highlightIsLegible()
{
    const QPalette p = WitsTheme::lightPalette();
    QVERIFY(BrandColorMath::contrastRatio(p.color(QPalette::HighlightedText),
                                          p.color(QPalette::Highlight)) >= 3.0);
}

void TestTheme::placeholderIsMutedButVisible()
{
    const QPalette p = WitsTheme::lightPalette();
    const QColor placeholder = p.color(QPalette::PlaceholderText);
    const QColor text = p.color(QPalette::Text);
    const QColor base = p.color(QPalette::Base);
    QVERIFY(placeholder != text);                       // visually distinct from real input
    QVERIFY(placeholder.lightness() > text.lightness()); // muted, not emphasized
    QVERIFY(BrandColorMath::contrastRatio(placeholder, base) >= 1.5); // still visible
}

void TestTheme::disabledTextIsMutedButVisible()
{
    const QPalette p = WitsTheme::lightPalette();
    const QColor disabled = p.color(QPalette::Disabled, QPalette::Text);
    const QColor normal = p.color(QPalette::Text);
    QVERIFY(disabled.lightness() > normal.lightness());
    QVERIFY(BrandColorMath::contrastRatio(disabled, p.color(QPalette::Base)) >= 1.5);
}

void TestTheme::namedColorsAreValid()
{
    const QStringList all = {
        WitsTheme::Color::SidebarBase, WitsTheme::Color::Card,
        WitsTheme::Color::AppBackground, WitsTheme::Color::Border,
        WitsTheme::Color::Text, WitsTheme::Color::MutedText,
        WitsTheme::Color::KioskPrimary, WitsTheme::Color::KioskPrimaryHover,
        WitsTheme::Color::AdminPrimary, WitsTheme::Color::AdminPrimaryHover,
        WitsTheme::Color::Secondary, WitsTheme::Color::Success,
        WitsTheme::Color::Error,
    };
    for (const QString &name : all)
        QVERIFY2(QColor(name).isValid(), qPrintable(name));
}

void TestTheme::adminAndKioskRolesStayDistinct()
{
    QVERIFY(QColor(WitsTheme::Color::AdminPrimary) != QColor(WitsTheme::Color::KioskPrimary));
    QVERIFY(QColor(WitsTheme::Color::AdminPrimaryHover) != QColor(WitsTheme::Color::KioskPrimaryHover));
}

void TestTheme::hoverVariantsAreDarker()
{
    using BrandColorMath::relativeLuminance;
    QVERIFY(relativeLuminance(QColor(WitsTheme::Color::AdminPrimaryHover))
            < relativeLuminance(QColor(WitsTheme::Color::AdminPrimary)));
    QVERIFY(relativeLuminance(QColor(WitsTheme::Color::KioskPrimaryHover))
            < relativeLuminance(QColor(WitsTheme::Color::KioskPrimary)));
}

void TestTheme::loadStyleSheetReturnsNonEmpty()
{
    const QString qss = WitsTheme::loadStyleSheet(QStringLiteral(WITS_QSS_PATH));
    QVERIFY2(!qss.isEmpty(), "wits.qss should load with content");
    QVERIFY2(qss.contains("QPushButton"), "wits.qss should contain button rules");
}

void TestTheme::loadStyleSheetMissingFileIsEmpty()
{
    QCOMPARE(WitsTheme::loadStyleSheet(QStringLiteral("/no/such/file.qss")), QString());
}

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
```

- [ ] **Step 2: Verify no hardcoded hex:** `grep -E '#[0-9A-Fa-f]{6}' qt-app/tests/tst_theme.cpp` → zero hits.
- [ ] **Step 3: Run tst_theme + FULL suite → 12/12 green.**
- [ ] **Step 4: Commit** — subject: `test(theme): replace hardcoded-hex assertions with behavioral legibility checks`

---

### Task 8: Phase 1 verification & proof assembly

**Files:**
- Create: `docs/superpowers/reports/2026-07-06-phase1-proofs.md` (build/test outputs + per-condition evidence)

No production code changes. Every command runs from the worktree root with the toolchain PATH prepended.

- [ ] **Step 1: Debug build from clean** — `cmake -S qt-app -B qt-app/build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"` then `cmake --build qt-app/build-debug` → exit 0; capture full output.
- [ ] **Step 2: Release build from clean** — same with `-B qt-app/build-release -DCMAKE_BUILD_TYPE=Release` → exit 0; capture full output.
- [ ] **Step 3: Warning delta** — grep both build logs for `warning`; compare against the same grep over a baseline build of master (configure master into `qt-app/build-baseline` the same way). Only pre-existing warnings (QXlsx CMake dev warning etc.) may appear. Enumerate any others → must be zero.
- [ ] **Step 4: Full test run** — `ctest --test-dir qt-app/build-debug --output-on-failure` → 12/12; capture full output (proof for conditions 2, 3, 4, 6).
- [ ] **Step 5: Unmodified-tests proof** — `git diff master --stat -- qt-app/tests/` shows: `tst_theme.cpp` modified (allowed), `tst_brandtheme.cpp`/`tst_brandingcontroller.cpp` added, `CMakeLists.txt` additions only, and NO other `tst_*.cpp` touched.
- [ ] **Step 6: Endpoint-contract proof (condition 16)** — `git diff master --name-status -- deliverables/loams_api/` → only `A` (added) lines for the two new PHP files; zero `M`.
- [ ] **Step 7: Schema proof (condition 17)** — `git diff master --name-status -- deliverables/sql/` → only `A deliverables/sql/branding_config.sql`; grep the branch for `ALTER TABLE` → zero hits. (Live `SHOW CREATE TABLE` before/after is a deploy-time check; record the equivalent repo-side proof plus the exact statement to run at deploy.)
- [ ] **Step 8: Startup-path walkthrough (condition 7)** — quote the mainwindow.cpp block with line numbers: cache applied synchronously in the ctor; fetch is async; reapply guarded by `isNewer`.
- [ ] **Step 9: Manual-hook excerpt (condition 8)** — quote `regenerateFromLogo`'s `ThemeMode::Manual` early-return from brandtheme.cpp; `grep -rn "Manual" qt-app/*.ui` → zero hits (no UI).
- [ ] **Step 10: Write the proofs report** collecting Steps 1–9 under headings `Condition 1` … `Condition 10` (mapping to the goal's ten proof items), and commit — subject: `docs(phase1): record build/test/contract proofs for the branding engine`

---

## Self-Review Notes

- Spec coverage: goal conditions 1–2 (Task 8 Steps 1–5), 3 (Task 7), 4 (Tasks 1–2), 5 (Task 4), 6 (Task 2 tests), 7 (Task 6 Step 3 + Task 8 Step 8), 8 (Task 3 + Task 8 Step 9), 9 (Task 4 Step 5 + Task 8 Step 6), 10 (Task 8 Step 7). Design-system derivation rules folded into Global Constraints.
- Type consistency: `BrandPalette`/`BrandingConfig`/`ThemeMode` defined once (Task 1) and consumed by name everywhere; controller signatures in Task 5 match Task 6's calls; `kDateTimeFormat` (`yyyy-MM-dd HH:mm:ss`) matches MySQL TIMESTAMP text and the parser tests.
- Placeholders: Task 1 Step 6 deliberately ships inert placeholder bodies for Task 2/3 functions so the target links; each is labeled with the task that replaces it and each replacement has its own red step. Task 2 Step 1 and Task 5 Step 2 name the exact assertions per slot rather than full listings — the assertion list is the spec; implementers write the bodies against it.
