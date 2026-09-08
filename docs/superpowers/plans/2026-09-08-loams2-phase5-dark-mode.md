# Phase 5 — Admin Dark Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give LOAMS 2.0 a Light / Dark / System theme choice scoped to the **admin** surfaces (the kiosk stays light), reusing the existing `BrandTheme` WCAG machinery so logo-derived brand colors stay legible on dark.

**Architecture:** A new pure engine function `BrandTheme::darkPalette(light)` derives a dark palette (fixed hand-tuned dark neutrals + brand/accent roles re-contrast-checked for a dark surface). `ThemeViewModel` gains a persisted `mode` (`Light`/`Dark`/`System`), a `resolvedDark` bool (mode + OS `colorScheme`), and dark color accessors backed by a cached dark palette. `Theme.qml` computes `isDark = admin-surface-active && resolvedDark` and switches every neutral token, the six design-literal tokens, and the on-surface brand/accent roles between light and dark. Admin Settings gains a segmented picker that writes through `Theme._vm.setMode(...)`.

**Tech Stack:** Qt 6.11.1, C++17, QML (Qt Quick), CMake + Ninja (MinGW). Tests: Qt Test (`QtTest`) + Qt Quick Test, run under CTest.

## Global Constraints

- **Qt kit is NOT on PATH.** Every `Run:` step assumes this PowerShell prelude has been run once in the shell:
  ```
  $env:Path = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:Path
  ```
- **Build into a SHORT external dir** (the default `qt-app/build` overflows Windows `MAX_PATH` on the QML module autogen). Use `C:/b/loams-5`.
  - Configure once: `cmake -S qt-app -B C:/b/loams-5 -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64`
  - Build: `cmake --build C:/b/loams-5`
  - Run one test target: `ctest --test-dir C:/b/loams-5 -R <name> --output-on-failure`
- **No CMake edits are required** — every new test appends to an existing target (`tst_brandtheme`, `tst_themeviewmodel`, `tst_qml_theme`, `tst_qml_admin`).
- **Zero raw hex outside `Theme.qml` and the brand engine.** Dark literals live in `Theme.qml` or `brandtheme.cpp`; opacity variants use `Qt.alpha(...)` / `Qt.rgba(...)`, never a literal color elsewhere.
- **MVVM:** QML never calls `BrandTheme`/`ApiConfig` directly. Mode changes go through `Theme._vm.setMode(...)` (the singleton's instance — a second `ThemeViewModel` would update global state nothing is bound to).
- **Default mode = `System`.** First run with no persisted value resolves to System (follows the OS).
- **Dark neutral hex values are PROVISIONAL** — anchored to reference option 1b and tuned in a human-review round after this plan lands. Tests pin *contrast/behaviour*, never specific hexes.
- Full suite must stay green: `ctest --test-dir C:/b/loams-5 --output-on-failure -j1` (serial — two QML targets flake only under full parallel load).

---

## File Structure

- `qt-app/core/brandtheme.h` / `brandtheme.cpp` — **modify**: add `darkPalette(const BrandPalette&)` + fixed dark-neutral constants. Light path untouched.
- `qt-app/tests/tst_brandtheme.cpp` — **modify**: 4 engine tests for `darkPalette`.
- `qt-app/quick/viewmodels/ThemeViewModel.h` / `ThemeViewModel.cpp` — **modify**: `mode`/`resolvedDark`/system seam + dark accessors + dark cache.
- `qt-app/quick/tests/tst_themeviewmodel.cpp` — **modify**: mode/resolvedDark/system + dark-accessor tests.
- `qt-app/quick/qml/theme/Theme.qml` — **modify**: real `isDark`/`mode`, token switching, `sidebarSurface`.
- `qt-app/quick/qml/components/LSideNav.qml` — **modify**: sidebar fill reads `Theme.sidebarSurface`.
- `qt-app/quick/tests/tst_qml_theme.qml` — **modify**: surface-scope + token-flip QuickTests.
- `qt-app/quick/qml/admin/SettingsScreen.qml` — **modify**: Appearance card with the Light/Dark/System picker.
- `qt-app/quick/tests/tst_qml_admin.qml` — **modify**: picker-wiring QuickTest.

---

## Task 1: Engine — `BrandTheme::darkPalette()`

Pure, deterministic dark-palette derivation. No I/O. Light path unchanged.

**Files:**
- Modify: `qt-app/core/brandtheme.h` (declare after `buildPalette`, ~line 52)
- Modify: `qt-app/core/brandtheme.cpp` (constants near the fallback constants; function after `buildPalette`, ~line 387)
- Test: `qt-app/tests/tst_brandtheme.cpp` (append slots to `TestBrandTheme`)

**Interfaces:**
- Consumes: `BrandPalette` (brandthemedata.h), `raiseToContrast`, `mix`, `kTextContrast` (=4.5, file-scope-visible in brandtheme.cpp).
- Produces: `BrandPalette BrandTheme::darkPalette(const BrandPalette &light);` — neutrals become fixed dark values; `brandBase/Deep/On`, `accentBase/Deep/On` carried over unchanged; `brandText`, `accentText`, `brandSoft`, `accentSoft`, `brandOnMuted` re-derived for a dark surface.

- [ ] **Step 1: Write the failing tests**

Append these slot declarations inside `TestBrandTheme`'s `private slots:` block in `qt-app/tests/tst_brandtheme.cpp` (after the existing declarations):

```cpp
    // Dark surfaces (Phase 5)
    void darkPaletteNeutralsAreDark();
    void darkPaletteTextRolesLegibleOnDark();
    void darkPaletteCarriesBrandFills();
    void darkPaletteIsDeterministic();
```

Append these implementations just before the `QTEST_MAIN(TestBrandTheme)` line:

```cpp
void TestBrandTheme::darkPaletteNeutralsAreDark()
{
    const BrandPalette d = BrandTheme::darkPalette(BrandTheme::fallbackPalette());
    QVERIFY(d.appBackground.lightness() < 128);
    QVERIFY(d.card.lightness() < 128);
    QVERIFY(d.sidebarBase.lightness() < 128);
    QVERIFY(d.text.lightness() > 128);   // near-white body text on the dark ground
}

void TestBrandTheme::darkPaletteTextRolesLegibleOnDark()
{
    using BrandColorMath::contrastRatio;
    const BrandPalette d = BrandTheme::darkPalette(BrandTheme::fallbackPalette());
    QVERIFY(contrastRatio(d.text, d.card) >= 4.5);
    QVERIFY(contrastRatio(d.text, d.appBackground) >= 4.5);
    QVERIFY(contrastRatio(d.brandText, d.card) >= 4.5);
    QVERIFY(contrastRatio(d.accentText, d.card) >= 4.5);
    QVERIFY(contrastRatio(d.mutedText, d.card) >= 3.0);
    QVERIFY(contrastRatio(d.brandOnMuted, d.sidebarBase) >= 4.5);
}

void TestBrandTheme::darkPaletteCarriesBrandFills()
{
    const BrandPalette light = BrandTheme::fallbackPalette();
    const BrandPalette d = BrandTheme::darkPalette(light);
    QCOMPARE(d.brandBase, light.brandBase);
    QCOMPARE(d.accentBase, light.accentBase);
    QCOMPARE(d.brandOn, light.brandOn);
}

void TestBrandTheme::darkPaletteIsDeterministic()
{
    const BrandPalette light = BrandTheme::fallbackPalette();
    QVERIFY(BrandTheme::darkPalette(light) == BrandTheme::darkPalette(light));
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_brandtheme --output-on-failure`
Expected: **build FAILS** with `'darkPalette' is not a member of 'BrandTheme'`.

- [ ] **Step 3: Declare the function in `brandtheme.h`**

In `qt-app/core/brandtheme.h`, immediately after the `buildPalette(...)` declaration (~line 52), add:

```cpp
// --- Dark surfaces (Phase 5) ---
// Derives a DARK palette from an already-built LIGHT palette. Neutral roles
// become a fixed hand-tuned dark set (anchored to reference option 1b); the
// brand/accent FILL roles (brandBase/Deep/On, accentBase/Deep/On) carry over
// unchanged, while the on-LIGHT-surface roles (brandText/accentText, the soft
// tints brandSoft/accentSoft, and the muted nav label brandOnMuted) are
// re-derived for a dark surface and re-contrast-checked with the same WCAG
// machinery. Pure and deterministic: a light palette always yields the same dark.
BrandPalette darkPalette(const BrandPalette &light);
```

- [ ] **Step 4: Add the dark-neutral constants in `brandtheme.cpp`**

In `qt-app/core/brandtheme.cpp`, add a new anonymous-namespace block just after the fallback-constants block (after line ~40, the `} // namespace` that closes `kDefaultAccentText`):

```cpp
namespace {
// Fixed hand-tuned DARK neutral surfaces (Phase 5), anchored to reference dark
// option 1b (deep-navy ground, near-white text, gold accent). NOT logo-derived
// — the dark ground is a curated set, tuned in the human-review round. None of
// these hexes leak past this engine / Theme.qml. PROVISIONAL values.
const char *const kDarkAppBackground = "#0C1524"; // deep navy ground
const char *const kDarkCard          = "#15213A"; // raised navy card
const char *const kDarkSidebarBase   = "#0A1220"; // darker slate — the admin nav mass
const char *const kDarkBorder        = "#26324B"; // subtle hairline on dark
const char *const kDarkText          = "#E8EDF5"; // near-white body text
const char *const kDarkMutedText     = "#94A3B8"; // slate-400 muted text
const char *const kDarkSuccess       = "#34D399"; // emerald-400 (lifted for dark)
const char *const kDarkError         = "#F87171"; // red-400 (lifted for dark)
// Soft tints on dark are mostly-card with a hint of the brand hue — the dark
// analogue of kSoftMixToWhite, but toward the dark card instead of white.
const double kDarkSoftMixToCard = 0.82;
// Fallback for on-dark TEXT when raising VALUE alone can't reach the text
// floor (a saturated dark brand): mix the fill this far toward the near-white
// text token, yielding a light tint of the brand hue that clears 4.5:1.
const double kDarkTextMixToLight = 0.65;
} // namespace
```

> `contrastRatio` and `mix` are already `using`-imported at the top of `brandtheme.cpp` (lines 20-21), so `darkPalette`'s `onDarkText` lambda can call them directly.

- [ ] **Step 5: Implement `darkPalette()` in `brandtheme.cpp`**

Add the function immediately after `buildPalette(...)` returns (after its closing brace, ~line 387), still inside `namespace BrandTheme`:

```cpp
// Derives a dark palette from a light one (see brandtheme.h). Placed after
// buildPalette so raiseToContrast/mix and kTextContrast are all file-visible.
BrandPalette darkPalette(const BrandPalette &light)
{
    BrandPalette d = light; // carry the brand/accent FILL roles unchanged

    // Fixed dark neutral ground.
    d.appBackground = QColor(kDarkAppBackground);
    d.card          = QColor(kDarkCard);
    d.sidebarBase   = QColor(kDarkSidebarBase);
    d.border        = QColor(kDarkBorder);
    d.text          = QColor(kDarkText);
    d.mutedText     = QColor(kDarkMutedText);
    d.success       = QColor(kDarkSuccess);
    d.error         = QColor(kDarkError);

    // On-dark brand/accent text. raiseToContrast raises HSV VALUE only
    // (hue+sat preserved, brandtheme.cpp:324-337), so a saturated dark brand
    // (navy #1E3A8A, maroon #7E1A15) value-maxes to a still-low-luminance
    // colour that CANNOT reach the 4.5 text floor on a dark card. When raising
    // value alone falls short, mix the fill toward the near-white text token
    // first — a light tint of the brand hue whose luminance always clears the
    // floor — then top up. A light fill (e.g. gold accent) passes the first
    // branch unchanged, keeping its hue. Deterministic (mix + raiseToContrast).
    auto onDarkText = [&](const QColor &fill) {
        QColor c = raiseToContrast(fill, d.card, kTextContrast);
        if (contrastRatio(c, d.card) < kTextContrast)
            c = raiseToContrast(mix(fill, d.text, kDarkTextMixToLight), d.card, kTextContrast);
        return c;
    };
    d.brandText  = onDarkText(light.brandBase);
    d.accentText = onDarkText(light.accentBase);

    // Soft fills become dark tints (mostly card, a hint of hue) so a soft-fill
    // block reads as a dark surface, not a near-white block on a dark card.
    d.brandSoft  = mix(light.brandBase,  d.card, kDarkSoftMixToCard);
    d.accentSoft = mix(light.accentBase, d.card, kDarkSoftMixToCard);

    // The muted nav label must stay legible on the dark slate sidebar.
    d.brandOnMuted = raiseToContrast(light.brandOnMuted, d.sidebarBase, kTextContrast);

    return d;
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_brandtheme --output-on-failure`
Expected: **PASS**, all `tst_brandtheme` cases (existing + 4 new).

- [ ] **Step 7: Commit**

Use the `commit` skill with a message like:
`feat(theme): add BrandTheme::darkPalette dark-surface derivation`
(body: fixed dark neutrals anchored to reference 1b; brand/accent text+soft re-contrast-checked against dark via raiseToContrast/mix; light path untouched.)

---

## Task 2: ThemeViewModel — mode, resolvedDark, system seam, persistence

Behavioral mode logic. No dark color accessors yet (Task 3).

**Files:**
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.h`
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.cpp`
- Test: `qt-app/quick/tests/tst_themeviewmodel.cpp`

**Interfaces:**
- Consumes: `AppSettings` (core/appsettings.h), `QGuiApplication::styleHints()`, `QStyleHints::colorSchemeChanged`, `Qt::ColorScheme`.
- Produces:
  - `QString mode() const;` / `void setMode(const QString &mode);` (persists `theme/mode`)
  - `bool resolvedDark() const;` — `(mode=="Dark") || (mode=="System" && systemDark)`
  - `void applySystemColorScheme(Qt::ColorScheme scheme);` — the test seam; the ctor connects the OS signal to it
  - signals `modeChanged()`, `resolvedDarkChanged()`

- [ ] **Step 1: Write the failing tests**

In `qt-app/quick/tests/tst_themeviewmodel.cpp`, add these includes at the top (after the existing includes):

```cpp
#include <QGuiApplication>
#include <QStyleHints>
```

Add these slot declarations to `TestThemeViewModel`'s `private slots:` block:

```cpp
    void init();   // reset persisted mode to a known default before each test
    void modePersistsAcrossInstances();
    void resolvedDarkTruthTable();
    void systemModeFollowsColorScheme();
    void setModeEmitsSignals();
```

Add these implementations before `QTEST_MAIN(TestThemeViewModel)`:

```cpp
void TestThemeViewModel::init()
{
    // AppSettings is process-isolated in tests, but shared across test
    // functions in this process; reset to the default so each test is
    // independent of persisted leftovers from another.
    ThemeViewModel v;
    v.setMode("System");
}

void TestThemeViewModel::modePersistsAcrossInstances()
{
    { ThemeViewModel vm; vm.setMode("Dark"); }
    ThemeViewModel vm2;
    QCOMPARE(vm2.mode(), QStringLiteral("Dark"));
}

void TestThemeViewModel::resolvedDarkTruthTable()
{
    ThemeViewModel vm;
    vm.setMode("Light");
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QVERIFY(!vm.resolvedDark());                       // Light overrides system

    vm.setMode("Dark");
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QVERIFY(vm.resolvedDark());                        // Dark overrides system

    vm.setMode("System");
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QVERIFY(vm.resolvedDark());                        // System follows dark OS
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QVERIFY(!vm.resolvedDark());                       // System follows light OS
    vm.applySystemColorScheme(Qt::ColorScheme::Unknown);
    QVERIFY(!vm.resolvedDark());                       // Unknown treated as light
}

void TestThemeViewModel::systemModeFollowsColorScheme()
{
    ThemeViewModel vm;
    vm.setMode("System");
    vm.applySystemColorScheme(Qt::ColorScheme::Light);
    QSignalSpy spy(&vm, &ThemeViewModel::resolvedDarkChanged);
    vm.applySystemColorScheme(Qt::ColorScheme::Dark);
    QCOMPARE(spy.count(), 1);
    QVERIFY(vm.resolvedDark());
}

void TestThemeViewModel::setModeEmitsSignals()
{
    ThemeViewModel vm;
    vm.setMode("Light");
    QSignalSpy modeSpy(&vm, &ThemeViewModel::modeChanged);
    QSignalSpy darkSpy(&vm, &ThemeViewModel::resolvedDarkChanged);
    vm.setMode("Dark");
    QCOMPARE(modeSpy.count(), 1);   // the picker binding re-evaluates
    QCOMPARE(darkSpy.count(), 1);   // isDark re-evaluates; drives the token flip
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_themeviewmodel --output-on-failure`
Expected: **build FAILS** (`mode`, `setMode`, `resolvedDark`, `applySystemColorScheme`, `modeChanged`, `resolvedDarkChanged` undefined).

- [ ] **Step 3: Declare the API in `ThemeViewModel.h`**

Add the two `Q_PROPERTY` lines just after the neutral-role property block (after the `error` property, ~line 40):

```cpp
    // Theme mode (Phase 5). mode is Light|Dark|System; resolvedDark folds mode
    // with the OS colorScheme (System-only). Surface-scoping to admin lives in
    // Theme.qml, not here.
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool resolvedDark READ resolvedDark NOTIFY resolvedDarkChanged)
```

Add the includes near the top (after `#include <QColor>`):

```cpp
#include <QString>
#include <qnamespace.h>   // Qt::ColorScheme
```

In the `public:` section, after `regenerateFromImportedLogo(...)`, add:

```cpp
    QString mode() const { return m_mode; }
    void setMode(const QString &mode);   // persists theme/mode, recomputes resolvedDark
    bool resolvedDark() const;

    // System-appearance seam (testable): the ctor connects
    // QStyleHints::colorSchemeChanged to this; tests call it directly so the
    // System branch never depends on the host OS appearance.
    void applySystemColorScheme(Qt::ColorScheme scheme);
```

In `signals:`, after `void changed();`, add:

```cpp
    void modeChanged();
    void resolvedDarkChanged();
```

In `private:`, after `BrandingConfig m_config;`, add:

```cpp
    void loadMode();
    static bool schemeIsDark(Qt::ColorScheme s);
    QString m_mode = QStringLiteral("System");
    Qt::ColorScheme m_systemScheme = Qt::ColorScheme::Unknown;
```

- [ ] **Step 4: Implement in `ThemeViewModel.cpp`**

Add includes at the top (after the existing `#include "brandtheme.h"`):

```cpp
#include <QGuiApplication>
#include <QStyleHints>
#include "appsettings.h"
```

Add a file-scope anonymous namespace after the includes:

```cpp
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
```

Replace the constructor body with (keep the existing explanatory comment lines):

```cpp
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
```

Add these method definitions (anywhere after the constructor):

```cpp
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
```

- [ ] **Step 5: Run to verify pass**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_themeviewmodel --output-on-failure`
Expected: **PASS** (existing + 4 new cases).

- [ ] **Step 6: Commit**

Use the `commit` skill:
`feat(theme): ThemeViewModel gains persisted Light/Dark/System mode`
(body: mode persisted in AppSettings theme/mode; resolvedDark folds mode with QStyleHints colorScheme; applySystemColorScheme is the OS-follow seam the ctor connects and tests drive directly.)

---

## Task 3: ThemeViewModel — dark color accessors + dark cache

Expose the dark palette to QML; keep it in sync with `BrandTheme::current()`.

**Files:**
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.h`
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.cpp`
- Test: `qt-app/quick/tests/tst_themeviewmodel.cpp`

**Interfaces:**
- Consumes: `BrandTheme::darkPalette` (Task 1), `BrandTheme::current()`.
- Produces: read-only `QColor` accessors `cardDark`, `appBackgroundDark`, `borderDark`, `textDark`, `mutedTextDark`, `successDark`, `errorDark`, `sidebarBaseDark`, `brandTextDark`, `brandSoftDark`, `brandOnMutedDark`, `accentTextDark`, `accentSoftDark` (all `NOTIFY changed`), backed by `m_darkCache`. `refresh()` and `regenerateFromImportedLogo()` rebuild the cache.

- [ ] **Step 1: Write the failing tests**

Add slot declarations to `TestThemeViewModel`:

```cpp
    void darkAccessorsMatchDarkPalette();
    void darkCacheRebuildsAfterSetCurrentAndRefresh();
```

Add implementations before `QTEST_MAIN`:

```cpp
void TestThemeViewModel::darkAccessorsMatchDarkPalette()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    const BrandPalette d = BrandTheme::darkPalette(BrandTheme::current());
    QCOMPARE(vm.cardDark(), d.card);
    QCOMPARE(vm.textDark(), d.text);
    QCOMPARE(vm.brandTextDark(), d.brandText);
    QCOMPARE(vm.accentTextDark(), d.accentText);
    QCOMPARE(vm.sidebarBaseDark(), d.sidebarBase);
    QVERIFY(vm.property("cardDark").isValid());   // Q_PROPERTY registered for QML
}

void TestThemeViewModel::darkCacheRebuildsAfterSetCurrentAndRefresh()
{
    BrandTheme::setCurrent(BrandTheme::fallbackPalette());
    ThemeViewModel vm;
    BrandPalette custom = BrandTheme::fallbackPalette();
    custom.brandBase = QColor(0x7E, 0x1A, 0x15);   // maroon
    BrandTheme::setCurrent(custom);
    vm.refresh();
    QCOMPARE(vm.brandTextDark(), BrandTheme::darkPalette(custom).brandText);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_themeviewmodel --output-on-failure`
Expected: **build FAILS** (`cardDark` etc. undefined).

- [ ] **Step 3: Declare the dark properties + accessors in `ThemeViewModel.h`**

After the light neutral-role `Q_PROPERTY` block (after `error`, before the `mode` property added in Task 2), add:

```cpp
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
```

In `public:`, after the light accessors (after `error()`), add:

```cpp
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
```

In `private:`, after the `m_systemScheme` member (Task 2), add:

```cpp
    void rebuildDarkCache();     // m_darkCache = darkPalette(current())
    BrandPalette m_darkCache;    // derived dark palette; kept in sync with BrandTheme::current()
```

- [ ] **Step 4: Implement the cache in `ThemeViewModel.cpp`**

Add the helper:

```cpp
void ThemeViewModel::rebuildDarkCache()
{
    m_darkCache = BrandTheme::darkPalette(BrandTheme::current());
}
```

In the constructor, add `rebuildDarkCache();` as the last line of the body (after the styleHints connect).

Replace `refresh()` with:

```cpp
void ThemeViewModel::refresh()
{
    // Re-notify QML after an external BrandTheme::setCurrent. The light getters
    // already read the engine live; the dark cache must be rebuilt so its
    // getters track the new palette too.
    rebuildDarkCache();
    emit changed();
}
```

In `regenerateFromImportedLogo(...)`, after `BrandTheme::setCurrent(m_config.palette);` and before `emit changed();`, add:

```cpp
    rebuildDarkCache();   // keep the dark palette in step with the new brand
```

- [ ] **Step 5: Run to verify pass**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_themeviewmodel --output-on-failure`
Expected: **PASS**.

- [ ] **Step 6: Commit**

Use the `commit` skill:
`feat(theme): expose dark palette to QML via ThemeViewModel *Dark accessors`
(body: m_darkCache = darkPalette(current()), rebuilt in ctor/refresh/regenerate so the dark roles track live re-theme; 13 read-only Q_PROPERTY twins for the light roles Theme.qml switches on.)

---

## Task 4: Theme.qml — isDark / mode / token switching / sidebarSurface

Wire the real mode into every token; add the admin-nav surface token and make `LSideNav` consume it.

**Files:**
- Modify: `qt-app/quick/qml/theme/Theme.qml`
- Modify: `qt-app/quick/qml/components/LSideNav.qml` (line 28)
- Test: `qt-app/quick/tests/tst_qml_theme.qml`

**Interfaces:**
- Consumes: `Navigator.currentSurface` / `Navigator.Admin` (LOAMS singleton), `_vm.resolvedDark`, `_vm.mode`, and all `_vm.*Dark` accessors (Task 3).
- Produces: `Theme.isDark`, `Theme.mode`, `Theme.sidebarSurface`, plus every neutral/literal/brand/accent token now mode-aware.

- [ ] **Step 1: Write the failing QuickTests**

In `qt-app/quick/tests/tst_qml_theme.qml`, add a `cleanup()` function and five test functions inside the `TestCase` (after the existing `test_elevationShadowsTrackBrand`):

```qml
    // Phase 5: reset the process-global mode + surface after each test so a
    // dark/admin state never leaks into another test in this file.
    function cleanup() {
        Theme._vm.setMode("System");
        Navigator.showKiosk();
    }

    function test_kioskAlwaysLightRegardlessOfMode() {
        Theme._vm.setMode("Dark");
        Navigator.showKiosk();
        verify(!Theme.isDark);
    }

    function test_adminDarkWhenModeDark() {
        Theme._vm.setMode("Dark");
        Navigator.showAdmin();
        verify(Theme.isDark);
    }

    function test_adminLightWhenModeLight() {
        Theme._vm.setMode("Light");
        Navigator.showAdmin();
        verify(!Theme.isDark);
    }

    function test_neutralTokensFlipWithDark() {
        Theme._vm.setMode("Light");
        Navigator.showAdmin();
        verify(Theme.card.hslLightness > 0.5);          // light card
        var lightText = Theme.text.toString();
        Theme._vm.setMode("Dark");
        verify(Theme.card.hslLightness < 0.5);          // dark card
        verify(Theme.text.toString() !== lightText);    // text token swapped
    }

    function test_sidebarSurfaceIsBrandInLightSlateInDark() {
        Theme._vm.setMode("Light");
        Navigator.showAdmin();
        compare(Theme.sidebarSurface.toString(), Theme.brand.base.toString());
        Theme._vm.setMode("Dark");
        verify(Theme.sidebarSurface.hslLightness < 0.5);
        verify(Theme.sidebarSurface.toString() !== Theme.brand.base.toString());
    }
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_qml_theme --output-on-failure`
Expected: **FAIL** — `Theme.isDark` is the hardcoded `false` stub, so `test_adminDarkWhenModeDark` fails; `Theme.sidebarSurface` is undefined.

- [ ] **Step 3: Rewrite the neutral tokens in `Theme.qml`**

Replace the neutral-token block (lines 32-39) with:

```qml
    readonly property color card:          isDark ? root._vm.cardDark          : root._vm.card
    readonly property color appBackground: isDark ? root._vm.appBackgroundDark : root._vm.appBackground
    readonly property color border:        isDark ? root._vm.borderDark        : root._vm.border
    readonly property color text:          isDark ? root._vm.textDark          : root._vm.text
    readonly property color mutedText:     isDark ? root._vm.mutedTextDark      : root._vm.mutedText
    readonly property color success:       isDark ? root._vm.successDark        : root._vm.success
    readonly property color error:         isDark ? root._vm.errorDark          : root._vm.error
    readonly property color sidebarBase:   isDark ? root._vm.sidebarBaseDark    : root._vm.sidebarBase
```

- [ ] **Step 4: Switch the brand/accent on-surface roles**

Replace the `accent` QtObject (lines 14-20) with:

```qml
    readonly property QtObject accent: QtObject {
        readonly property color base: root._vm.accentBase
        readonly property color deep: root._vm.accentDeep
        readonly property color soft: root.isDark ? root._vm.accentSoftDark : root._vm.accentSoft
        readonly property color on:   root._vm.accentOn
        readonly property color text: root.isDark ? root._vm.accentTextDark : root._vm.accentText
    }
```

Replace the `brand` QtObject (lines 22-30) with:

```qml
    readonly property QtObject brand: QtObject {
        readonly property color base:     root._vm.brandBase
        readonly property color deep:     root._vm.brandDeep
        readonly property color soft:     root.isDark ? root._vm.brandSoftDark    : root._vm.brandSoft
        readonly property color on:       root._vm.brandOn
        readonly property color onMuted:  root.isDark ? root._vm.brandOnMutedDark : root._vm.brandOnMuted
        readonly property color text:     root.isDark ? root._vm.brandTextDark    : root._vm.brandText
    }
```

- [ ] **Step 5: Switch the design-literal tokens + add `sidebarSurface`**

Replace the extra-design-token block (lines 42-47) with:

```qml
    // Extra design tokens (no BrandPalette field — literals, §12.1). Dark twins
    // are PROVISIONAL (tuned in the human-review round); hex is allowed here.
    readonly property color mutedTextCaption: isDark ? "#7E8CA3" : "#B0A08A"
    readonly property color tableHeaderBg:    isDark ? "#16223B" : "#F7F1E6"
    readonly property color rowHairline:      isDark ? "#1F2C46" : "#F3ECDD"
    readonly property color errorSoft:        isDark ? "#2A1517" : "#FDF4F3"
    readonly property color errorBorder:      isDark ? "#5B2A2C" : "#F3D9D6"
    readonly property color scrim:            isDark ? Qt.rgba(0, 0, 0, 0.55)
                                                     : Qt.rgba(15/255, 23/255, 42/255, 0.45)

    // Admin nav surface: the brand fill in light, a dark slate in dark (Phase 5
    // review decision — the dark sidebar is a neutral slate, not the maroon
    // brand block). LSideNav consumes THIS, not brand.base directly.
    readonly property color sidebarSurface: isDark ? root._vm.sidebarBaseDark : root.brand.base
```

- [ ] **Step 6: Replace the `mode`/`isDark` stubs**

Replace lines 139-142 (the stub block) with:

```qml
    // Mode (§13.5 / Phase 5). isDark is surface-scoped: only the admin surface
    // goes dark — the kiosk (a public wall display) always renders light. The
    // AppShell Loader swaps kiosk/admin so they never coexist, which is what
    // makes admin-only enforceable by surface rather than duplicated tokens.
    readonly property string mode: root._vm.mode
    readonly property bool isDark: Navigator.currentSurface === Navigator.Admin
                                   && root._vm.resolvedDark
```

- [ ] **Step 7: Point `LSideNav` at the surface token**

In `qt-app/quick/qml/components/LSideNav.qml`, change line 28 from `color: Theme.brand.base` to:

```qml
    color: Theme.sidebarSurface
```

(In light mode `sidebarSurface === brand.base`, so the light sidebar is byte-identical; in dark mode it becomes the slate.)

- [ ] **Step 8: Run to verify pass**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_qml_theme --output-on-failure`
Expected: **PASS** (existing token tests + 5 new). Also run `ctest --test-dir C:/b/loams-5 -R "tst_qml_admin|tst_qml_kiosk|tst_notokenaliases" --output-on-failure` to confirm no regression (default Kiosk surface keeps those light).

- [ ] **Step 9: Commit**

Use the `commit` skill:
`feat(theme): make Theme tokens dark-aware, surface-scoped to admin`
(body: isDark = admin-active && resolvedDark; neutral/literal/brand-text/soft tokens switch on isDark; new sidebarSurface token = brand fill in light, dark slate in dark; LSideNav consumes it. Kiosk stays light because its surface never satisfies isDark.)

---

## Task 5: Admin Settings — Light/Dark/System picker

The user-facing control, writing through the singleton VM.

**Files:**
- Modify: `qt-app/quick/qml/admin/SettingsScreen.qml`
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Consumes: `LSegmented` (`options: [{value,label}]`, `currentValue`, `signal selectionChanged(var value)`), `Theme.mode`, `Theme._vm.setMode(...)`.
- Produces: an `objectName: "themeModePicker"` `LSegmented` whose `currentValue` tracks `Theme.mode` and whose selection drives `Theme._vm.setMode`.

- [ ] **Step 1: Write the failing QuickTest**

In `qt-app/quick/tests/tst_qml_admin.qml`, add (inside the `TestCase`) a host item, an inline component, and a test function. Also ensure `mode` is reset to `System` regardless of how the test exits: if the `TestCase` already defines a `cleanup()`, add the reset line there; if not, add the `cleanup()` shown below (a mid-test failure otherwise leaves `mode=Dark` for later tests in the same binary — benign, since admin tests sit on the default Kiosk surface where `isDark` is false, but reset anyway for isolation):

```qml
    Item { id: settingsHost; width: 420; height: 640 }

    // If the TestCase has no cleanup() yet, add this; otherwise fold the
    // setMode line into the existing cleanup().
    function cleanup() { Theme._vm.setMode("System"); }

    Component {
        id: settingsScreenComp
        SettingsScreen { anchors.fill: parent }
    }

    function test_themeModePickerReflectsAndDrivesMode() {
        Theme._vm.setMode("System");
        var s = createTemporaryObject(settingsScreenComp, settingsHost);
        verify(s);
        var picker = findChild(s, "themeModePicker");
        verify(picker);

        // Reflects Theme.mode…
        Theme._vm.setMode("Dark");
        compare(picker.currentValue, "Dark");

        // …and drives it: emitting the picker's selection runs onSelectionChanged.
        picker.selectionChanged("Light");
        compare(Theme.mode, "Light");

        Theme._vm.setMode("System");   // reset process-global mode
    }
```

- [ ] **Step 2: Run to verify failure**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_qml_admin --output-on-failure`
Expected: **FAIL** — `findChild(s, "themeModePicker")` returns null (no such child yet).

- [ ] **Step 3: Add the Appearance card to `SettingsScreen.qml`**

Insert a new `LCard` in the `ColumnLayout { id: content ... }`, immediately after the School-identity `LCard` (after its closing `}` at ~line 216, before the Administrator card):

```qml
            // --- Appearance (Phase 5) ---
            LCard {
                id: appearanceCard
                Layout.fillWidth: true
                padding: Theme.spacing.lg
                implicitHeight: appearanceColumn.implicitHeight + padding * 2

                ColumnLayout {
                    id: appearanceColumn
                    anchors.fill: parent
                    spacing: Theme.spacing.md

                    Text {
                        text: qsTr("Appearance")
                        color: Theme.text
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.cardTitle
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Dark mode applies to the admin app only; the kiosk always stays light.")
                        color: Theme.mutedText
                        wrapMode: Text.WordWrap
                        font.family: Theme.typography.sans
                        font.pixelSize: Theme.typography.body
                    }
                    LSegmented {
                        id: themeModePicker
                        objectName: "themeModePicker"
                        // Reflects the persisted mode. NOTE: a real user click
                        // runs `seg.currentValue = value` imperatively inside
                        // LSegmented (LSegmented.qml:42), which SEVERS this
                        // binding — but it self-assigns the same value setMode
                        // writes back, and nothing else changes `mode`
                        // programmatically today (System-scheme changes alter
                        // resolvedDark, not mode), so the picker stays in sync.
                        // If a future affordance sets mode programmatically
                        // (e.g. a "reset to System" button), the picker will
                        // need an explicit re-sync — the severed binding won't
                        // pick it up.
                        currentValue: Theme.mode
                        options: [
                            { value: "Light",  label: qsTr("Light") },
                            { value: "Dark",   label: qsTr("Dark") },
                            { value: "System", label: qsTr("System") }
                        ]
                        // Live re-theme MUST go through Theme._vm (the singleton's
                        // instance Theme binds its tokens to) — a VM-owned instance
                        // would update nothing the UI is bound to.
                        onSelectionChanged: Theme._vm.setMode(value)
                    }
                }
            }
```

- [ ] **Step 4: Run to verify pass**

Run: `cmake --build C:/b/loams-5` then `ctest --test-dir C:/b/loams-5 -R tst_qml_admin --output-on-failure`
Expected: **PASS**.

> **Fixture-height coupling:** the new Appearance card grows the `settings` fixture's content column, which `test_fixtureFitsWholeContentColumn` (tst_qml_admin.qml ~:1214) pins against a deliberately-chosen `1600` height (comment ~:241-246). The one-line-description card (~110px) should fit inside the existing slack, but if that guard reddens, bump the pinned fixture height to absorb the card — do NOT shrink the card.

- [ ] **Step 5: Full-suite regression check**

Run: `ctest --test-dir C:/b/loams-5 --output-on-failure -j1`
Expected: **all green** (serial — the two QML targets flake only under full parallel load).

- [ ] **Step 6: Commit**

Use the `commit` skill:
`feat(settings): add Light/Dark/System appearance picker to admin Settings`
(body: LSegmented bound to Theme.mode, writing Theme._vm.setMode; copy states dark is admin-only. Completes Phase 5 wiring end-to-end.)

---

## Post-plan: human-review round

Per the spec (§6, roadmap Risk 3) dark mode has no full design reference. After Task 5 lands and the build is green, launch `WITSQuick.exe` (from `C:/b/loams-5/quick/`, Qt bin on PATH), enter admin, set **Dark**, and walk Dashboard / Search / Visit Logs / Database / Reporting / Settings. Tune, in `Theme.qml` (literals) and `brandtheme.cpp` (`kDark*` constants) ONLY: the dark neutrals, the sidebar slate, and any brand/accent role that reads too hot or too dim. No structural change — the contract (roles, contrast floors, surface-scoping, persistence) is fixed. Re-run the full suite after any hex change.

---

## Self-Review

**Spec coverage:**
- §1 in-scope mode Light/Dark/System, persisted → Task 2. System follows OS live → Task 2 (`applySystemColorScheme` + `colorSchemeChanged`). Fixed dark neutrals anchored to 1b → Task 1. Brand roles re-contrast-checked → Task 1. Settings picker → Task 5.
- §1 out-of-scope: kiosk never dark → Task 4 (`isDark` surface term) + test `test_kioskAlwaysLightRegardlessOfMode`. No animated transitions → tokens flip via bindings (no animation added). ✓
- §3.1 component responsibilities: engine `darkPalette` (Task 1), VM mode/resolvedDark/accessors (Tasks 2-3), Theme.qml isDark/mode/tokens (Task 4), Settings picker via Theme._vm (Task 5). ✓
- §3.2 sidebar = dark-neutral slate, brand via accents → Task 4 `sidebarSurface` (slate in dark) + `LSideNav` consumer; selected/hover states keep `accent.base`. ✓
- §5 testing: engine contrast/determinism (Task 1), mode persistence + truth table + system-follow (Task 2), dark accessors (Task 3), surface-scope guard + token flip (Task 4), picker wiring (Task 5). ✓

**Placeholder scan:** none — every step has concrete code and exact commands.

**Type consistency:** `setMode(QString)`, `resolvedDark()`, `applySystemColorScheme(Qt::ColorScheme)`, `rebuildDarkCache()`, `m_darkCache`, `darkPalette(const BrandPalette&)`, the 13 `*Dark` accessor names, `Theme.sidebarSurface`, `objectName "themeModePicker"` — all referenced consistently across tasks and tests. `LSegmented` API (`options`/`currentValue`/`selectionChanged(var)`) matches `qt-app/quick/qml/components/LSegmented.qml`.
