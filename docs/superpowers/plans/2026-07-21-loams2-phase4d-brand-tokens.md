# LOAMS 2.0 Phase 4d — Brand Tokens Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the surface-scoped brand palette (logo primary → all Admin, logo secondary → all Kiosk) with a role-based one where both surfaces use both logo colours split by element role, auto-derived from the school logo.

**Architecture:** Two reviewable PRs. **PR 1 (Tasks 1–7)** is invisible infrastructure: rename the C++ `BrandPalette` value type + `ThemeViewModel` + `Theme.qml` tokens (old names kept as aliases), then rewrite the derivation engine to the role model with a split-contrast policy and a bad-logo quality gate. No rendered change against an existing cached config; derivation output changes on the next logo upload. **PR 2 (Tasks 8–12)** is the visible change: extend the Settings status component, wire the fallback notice, de-hardcode the brand-coloured drop-shadows, sweep the ~61 QML call sites to the new role tokens, then delete the API aliases.

**Tech Stack:** Qt 6.11 / C++17, QML (Qt Quick), CMake + Ninja + MinGW. QtTest + Qt Quick Test under CTest. Colour math is header-only C++ (`brandcolormath.h`).

**Spec:** `docs/superpowers/specs/2026-07-21-loams2-phase4d-brand-tokens-design.md` (APPROVED). Section refs below (§N) point into it.

## Global Constraints

- **Theme.qml is the SINGLE source of every visual token — ZERO raw hex outside it.** Opacity variants use `Qt.alpha(Theme.<token>, a)`, never a literal colour.
- **MVVM:** ViewModels (`qt-app/quick/viewmodels/`) are the only QML-facing C++. QML never calls a `core`/`witscore` controller directly.
- **Contrast policy (owner-locked):** 4.5:1 for any token pair carrying text; 3:1 for purely graphical pairs.
- **Warm neutrals stay FIXED** — `card`, `appBackground`, `tableHeaderBg`, `rowHairline`, `mutedTextCaption`, `border`, `errorSoft`, `errorBorder`, `scrim`, `success`, `error` are never logo-derived.
- **JSON read-compat aliases are PERMANENT** — the old snake_case keys accepted by `paletteFromJson` outlive PR 2 indefinitely; deleting them would drop every un-resaved config to fallback.
- **Build dir:** the repo default is `qt-app/build` (`cmake -S qt-app -B qt-app/build`, then `cmake --build qt-app/build`); tests via `ctest --test-dir qt-app/build --output-on-failure`. If executing in a deep worktree that overflows Windows MAX_PATH, build into a short dir (e.g. `C:/b/loams-4d`) instead and substitute that path in every command below.
- **Qt tools are not on PATH.** Prefix build/test commands with: `export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/CMake_64/bin:$PATH"`.
- **Commit via the `commit` skill.** Never raw `git add`/`git commit` in normal flow; the commit steps below show the intended grouping/message.
- **New C++ token field names** (used verbatim throughout): `brandBase, brandDeep, brandSoft, brandOn, brandOnMuted, brandText, accentBase, accentDeep, accentSoft, accentOn, accentText`, plus the unchanged neutrals `sidebarBase, card, appBackground, border, text, mutedText, success, error`.
- **New JSON keys:** `brand_base, brand_deep, brand_soft, brand_on, brand_on_muted, brand_text, accent_base, accent_deep, accent_soft, accent_on, accent_text`. **Old read-compat keys** (permanent): `admin_primary→brandBase, admin_primary_hover→brandDeep, admin_on_primary→brandOn, admin_primary_soft→brandSoft, kiosk_primary→accentBase, kiosk_primary_hover→accentDeep, kiosk_on_primary→accentOn, kiosk_primary_soft→accentSoft, secondary→accentBase`. (`brand_on_muted`, `brand_text`, `accent_text` are NEW — no old key.)

---

# PR 1 — Infrastructure (invisible; two logical commits become Tasks 1–7)

## File map (PR 1)

- `qt-app/core/brandthemedata.h` — `BrandPalette` struct fields renamed; `BrandingConfig` gains `didFallBack`.
- `qt-app/core/brandtheme.h` — split-contrast constants; `RegenResult` enum; `buildPalette` decl.
- `qt-app/core/brandtheme.cpp` — `kPaletteFields` (rename + old-key read aliases), `paletteToJson` (dual-write), `paletteFromJson` (read-compat precedence), `fallbackPalette`, `enforceContrast`, `buildPalette`, quality gate, `regenerateFromLogo` sets `didFallBack`.
- `qt-app/quick/viewmodels/ThemeViewModel.h/.cpp` — new role `Q_PROPERTY`/accessors + old-name forwarding aliases; `regenerateFromImportedLogo` returns `RegenResult`.
- `qt-app/quick/qml/theme/Theme.qml` — new `brand.*`/`accent.*` role tokens + old-name alias bindings.
- `qt-app/tests/tst_brandtheme.cpp` — JSON round-trip incl. old-key read-compat; obsolete surface-distinctness tests replaced with role invariants; adversarial-seed test.

---

### Task 1: Rename `BrandPalette` fields + JSON read-compat + dual-write

**Files:**
- Modify: `qt-app/core/brandthemedata.h:19-63` (struct fields + `operator==`)
- Modify: `qt-app/core/brandtheme.cpp:26-49` (`fallbackPalette`), `:56-97` (`kPaletteFields`, `paletteToJson`, `paletteFromJson`), and every `p.adminPrimary`-style field reference in the build step `:321-360`
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.h` (accessors read the renamed fields — full rename is Task 2, but the fields they read change here; keep it compiling by updating the accessor bodies in this task)
- Test: `qt-app/tests/tst_brandtheme.cpp` (JSON tests)

**Interfaces:**
- Produces: `BrandPalette` with fields `brandBase, brandDeep, brandSoft, brandOn, brandOnMuted, brandText, accentBase, accentDeep, accentSoft, accentOn, accentText` + unchanged neutrals. `paletteFromJson` accepts BOTH new and old keys (new wins). `paletteToJson` emits BOTH key sets with equal values.

- [ ] **Step 1: Write the failing JSON read-compat test.** Add to `tst_brandtheme.cpp`'s slots (near `:33`):

```cpp
    void paletteFromJsonReadsOldKeys();
    void paletteFromJsonMixedKeysNewWins();
    void paletteToJsonDualWritesBothKeySets();
```

and the bodies:

```cpp
void TestBrandTheme::paletteFromJsonReadsOldKeys()
{
    // A config written by ANY pre-4d build holds only the OLD snake_case keys.
    // Read-compat must land them in the new role fields, not silently fall back.
    QJsonObject o;
    o.insert("admin_primary", "#123456");        // -> brandBase
    o.insert("kiosk_primary", "#abcdef");        // -> accentBase
    o.insert("admin_on_primary", "#ffffff");     // -> brandOn
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    QCOMPARE(p.brandBase,  QColor("#123456"));
    QCOMPARE(p.accentBase, QColor("#abcdef"));
    QCOMPARE(p.brandOn,    QColor("#ffffff"));
}

void TestBrandTheme::paletteFromJsonMixedKeysNewWins()
{
    // When both keys for the same field are present, the NEW key is authoritative.
    QJsonObject o;
    o.insert("admin_primary", "#111111");  // old alias for brandBase
    o.insert("brand_base",    "#222222");  // new key for brandBase
    const BrandPalette p = BrandTheme::paletteFromJson(o);
    QCOMPARE(p.brandBase, QColor("#222222"));
}

void TestBrandTheme::paletteToJsonDualWritesBothKeySets()
{
    BrandPalette p = BrandTheme::fallbackPalette();
    p.brandBase = QColor("#123456");
    const QJsonObject o = BrandTheme::paletteToJson(p);
    QCOMPARE(o.value("brand_base").toString(),   QString("#123456")); // new key
    QCOMPARE(o.value("admin_primary").toString(), QString("#123456")); // old key, equal value
}
```

- [ ] **Step 2: Run, expect FAIL** (fields/keys don't exist yet):

```
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:/c/Qt/Tools/Ninja:/c/Qt/Tools/CMake_64/bin:$PATH"
cmake --build qt-app/build 2>&1 | tail -5
```
Expected: compile error — `brandBase` etc. are not members of `BrandPalette`.

- [ ] **Step 3: Rename the struct fields.** In `brandthemedata.h`, replace the brand-role block (`:22-30`) with:

```cpp
    // Brand roles (logo-derived in Auto mode). Renamed to role names in 4d;
    // both surfaces draw from both families by element role.
    QColor brandBase;      // was adminPrimary
    QColor brandDeep;      // was adminPrimaryHover
    QColor brandOn;        // was adminOnPrimary
    QColor brandSoft;      // was adminPrimarySoft
    QColor brandOnMuted;   // NEW — muted text on the brand sidebar
    QColor brandText;      // NEW — brand colour used AS text on a light card
    QColor accentBase;     // was kioskPrimary
    QColor accentDeep;     // was kioskPrimaryHover
    QColor accentOn;       // was kioskOnPrimary
    QColor accentSoft;     // was kioskPrimarySoft
    QColor accentText;     // NEW — accent colour used AS text on a light card
```

Update `operator==` (`:42-61`) to compare all eleven brand/accent fields (the three new ones included) plus the unchanged neutrals.

- [ ] **Step 4: Update `fallbackPalette`, `kPaletteFields`, `paletteToJson`, `paletteFromJson`.** In `brandtheme.cpp`:

`fallbackPalette()` (`:26-49`) — set the new fields from the existing `WitsTheme::Color::*` constants; the two NEW derived-token fields get sensible fallback seeds so the fallback palette is complete:

```cpp
    p.brandBase      = QColor(WitsTheme::Color::AdminPrimary);
    p.brandDeep      = QColor(WitsTheme::Color::AdminPrimaryHover);
    p.brandOn        = white;
    p.brandSoft      = mix(p.brandBase, white, 0.90);
    p.brandOnMuted   = QColor("#EFC9A8"); // legacy onBrandMuted literal, now a palette field
    p.brandText      = p.brandBase;       // brand-as-text on light; fallback == base
    p.accentBase     = QColor(WitsTheme::Color::KioskPrimary);
    p.accentDeep     = QColor(WitsTheme::Color::KioskPrimaryHover);
    p.accentOn       = white;
    p.accentSoft     = mix(p.accentBase, white, 0.88);
    p.accentText     = QColor("#8a6a08"); // legacy accent-as-text; fallback constant
```

`kPaletteFields` (`:56-74`) — replace with the NEW keys mapped to the NEW members, and add a SECOND table of OLD read-compat keys:

```cpp
const FieldMap kPaletteFields[] = {
    {"brand_base",      &BrandPalette::brandBase},
    {"brand_deep",      &BrandPalette::brandDeep},
    {"brand_on",        &BrandPalette::brandOn},
    {"brand_soft",      &BrandPalette::brandSoft},
    {"brand_on_muted",  &BrandPalette::brandOnMuted},
    {"brand_text",      &BrandPalette::brandText},
    {"accent_base",     &BrandPalette::accentBase},
    {"accent_deep",     &BrandPalette::accentDeep},
    {"accent_on",       &BrandPalette::accentOn},
    {"accent_soft",     &BrandPalette::accentSoft},
    {"accent_text",     &BrandPalette::accentText},
    {"sidebar_base",    &BrandPalette::sidebarBase},
    {"card",            &BrandPalette::card},
    {"app_background",  &BrandPalette::appBackground},
    {"border",          &BrandPalette::border},
    {"text",            &BrandPalette::text},
    {"muted_text",      &BrandPalette::mutedText},
    {"success",         &BrandPalette::success},
    {"error",           &BrandPalette::error},
};
// PERMANENT read-compat: pre-4d configs hold only these old keys. NEVER delete.
const FieldMap kOldPaletteAliasFields[] = {
    {"admin_primary",       &BrandPalette::brandBase},
    {"admin_primary_hover", &BrandPalette::brandDeep},
    {"admin_on_primary",    &BrandPalette::brandOn},
    {"admin_primary_soft",  &BrandPalette::brandSoft},
    {"kiosk_primary",       &BrandPalette::accentBase},
    {"kiosk_primary_hover", &BrandPalette::accentDeep},
    {"kiosk_on_primary",    &BrandPalette::accentOn},
    {"kiosk_primary_soft",  &BrandPalette::accentSoft},
    {"secondary",           &BrandPalette::accentBase},
};
```

`paletteToJson` (`:80-86`) — dual-write: emit the new keys, then the old alias keys with equal values:

```cpp
QJsonObject paletteToJson(const BrandPalette &p)
{
    QJsonObject o;
    for (const FieldMap &f : kPaletteFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    // Dual-write old keys so an older build reading this config still finds them.
    for (const FieldMap &f : kOldPaletteAliasFields)
        o.insert(QLatin1String(f.key), (p.*(f.member)).name());
    return o;
}
```

`paletteFromJson` (`:88-97`) — apply OLD aliases first, then NEW keys, so new wins the last-writer-wins overwrite:

```cpp
BrandPalette paletteFromJson(const QJsonObject &o)
{
    BrandPalette p = fallbackPalette();
    // Old aliases first...
    for (const FieldMap &f : kOldPaletteAliasFields) {
        const QColor c(o.value(QLatin1String(f.key)).toString());
        if (c.isValid()) p.*(f.member) = c;
    }
    // ...then new keys, which therefore win when both are present.
    for (const FieldMap &f : kPaletteFields) {
        const QColor c(o.value(QLatin1String(f.key)).toString());
        if (c.isValid()) p.*(f.member) = c;
    }
    return p;
}
```

- [ ] **Step 5: Keep the build compiling.** Update every remaining `p.adminPrimary`/`p.kioskPrimary`/`p.secondary`-style reference in `brandtheme.cpp`'s build step (`:321-360`) and in `ThemeViewModel.h`'s accessor bodies to the new field names (temporary — the build step is fully rewritten in Task 6; here just make it compile by mapping `adminPrimary→brandBase`, `kioskPrimary→accentBase`, `secondary→accentBase`, and set the three new fields to reasonable placeholders like `p.brandOnMuted = QColor("#EFC9A8"); p.brandText = p.brandBase; p.accentText = QColor("#8a6a08");`).

- [ ] **Step 6: Run the JSON tests, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure 2>&1 | tail -12
```
Expected: the three new tests pass; `paletteJsonRoundTrip` still passes (renamed keys round-trip). If `paletteJsonRoundTrip` (`:182`) hardcodes old key strings, update it to the new keys now.

- [ ] **Step 7: Commit.**

```
git add qt-app/core/brandthemedata.h qt-app/core/brandtheme.cpp qt-app/quick/viewmodels/ThemeViewModel.h qt-app/tests/tst_brandtheme.cpp
git commit -m "refactor(brandtheme): rename BrandPalette fields to roles; permanent JSON read-compat + dual-write"
```

---

### Task 2: Rename `ThemeViewModel` accessors + keep old-name Q_PROPERTY aliases

**Files:**
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.h:18-56`
- Test: none new (compile + existing QuickTests are the gate; QML aliases proven in Task 3)

**Interfaces:**
- Produces: `ThemeViewModel` exposes NEW `Q_PROPERTY`s `brandBase, brandDeep, brandSoft, brandOn, brandOnMuted, brandText, accentBase, accentDeep, accentSoft, accentOn, accentText` (each `READ` a new accessor `NOTIFY changed`), AND keeps the OLD `Q_PROPERTY`s (`adminPrimary`, …, `secondary`) as forwarding aliases.

- [ ] **Step 1: Add the new accessors + Q_PROPERTYs.** In `ThemeViewModel.h`, add eleven new `Q_PROPERTY` lines mirroring the existing block, and new accessor bodies reading the renamed struct fields:

```cpp
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
```

```cpp
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
```

- [ ] **Step 2: Convert the OLD accessors into forwarding aliases** (keep the OLD `Q_PROPERTY` lines from `:19-27` unchanged; only the bodies forward):

```cpp
    // DEPRECATED API aliases — removed in PR 2. Forward to the new accessors so
    // there is one source of truth (no second copy of any colour).
    QColor adminPrimary() const      { return brandBase(); }
    QColor adminPrimaryHover() const { return brandDeep(); }
    QColor adminOnPrimary() const    { return brandOn(); }
    QColor adminPrimarySoft() const  { return brandSoft(); }
    QColor kioskPrimary() const      { return accentBase(); }
    QColor kioskPrimaryHover() const { return accentDeep(); }
    QColor kioskOnPrimary() const    { return accentOn(); }
    QColor kioskPrimarySoft() const  { return accentSoft(); }
    QColor secondary() const         { return accentBase(); }
```

- [ ] **Step 3: Build + run full suite, expect PASS** (nothing consumes the new props yet; old ones behave identically):

```
cmake --build qt-app/build && ctest --test-dir qt-app/build --output-on-failure 2>&1 | tail -6
```
Expected: `100% tests passed`.

- [ ] **Step 4: Commit.**

```
git add qt-app/quick/viewmodels/ThemeViewModel.h
git commit -m "refactor(theme-vm): expose role Q_PROPERTYs; old names forward as deprecated aliases"
```

---

### Task 3: Add role tokens to `Theme.qml`; keep old tokens as alias bindings

**Files:**
- Modify: `qt-app/quick/qml/theme/Theme.qml:12-41`
- Test: `qt-app/quick/tests/tst_qml_theme.qml` (a binding-resolves test)

**Interfaces:**
- Produces: `Theme.brand.base/deep/soft/on/onMuted/text` and `Theme.accent.base/deep/soft/on/text` resolve to the new `ThemeViewModel` props. The OLD `Theme.brand.admin/adminHover/adminSoft/kiosk/kioskHover/kioskSoft/onPrimary/onKiosk` and `Theme.secondary` remain as alias bindings.

- [ ] **Step 1: Write the failing token test.** In `tst_qml_theme.qml`, add a `TestCase` function asserting the new tokens exist and the alias equals the new token:

```qml
        function test_roleTokensExistAndAliasesMatch() {
            verify(Theme.brand.base !== undefined);
            verify(Theme.accent.base !== undefined);
            // The deprecated alias must resolve to the same colour as the new token.
            compare(Theme.brand.admin.toString(), Theme.brand.base.toString());
            compare(Theme.secondary.toString(),   Theme.accent.base.toString());
        }
```

- [ ] **Step 2: Run, expect FAIL** (`Theme.brand.base` undefined). Theme.qml is module QML — rebuild first:

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_qml_theme --output-on-failure 2>&1 | tail -8
```
Expected: FAIL — `brand.base` is undefined.

- [ ] **Step 3: Add the role groups + alias bindings.** Replace the `brand` group and `secondary` line (`Theme.qml:12-23`) with:

```qml
    readonly property QtObject brand: QtObject {
        // New role tokens (4d).
        readonly property color base:     root._vm.brandBase
        readonly property color deep:     root._vm.brandDeep
        readonly property color soft:     root._vm.brandSoft
        readonly property color on:       root._vm.brandOn
        readonly property color onMuted:  root._vm.brandOnMuted
        readonly property color text:     root._vm.brandText
        // DEPRECATED aliases — removed in PR 2. Pure bindings to the new tokens.
        readonly property color admin:      base
        readonly property color adminHover: deep
        readonly property color adminSoft:  soft
        readonly property color kiosk:      root.accent.base
        readonly property color kioskHover: root.accent.deep
        readonly property color kioskSoft:  root.accent.soft
        readonly property color onPrimary:  on
        readonly property color onKiosk:    root.accent.on
    }
    readonly property QtObject accent: QtObject {
        readonly property color base: root._vm.accentBase
        readonly property color deep: root._vm.accentDeep
        readonly property color soft: root._vm.accentSoft
        readonly property color on:   root._vm.accentOn
        readonly property color text: root._vm.accentText
    }
    // DEPRECATED alias — removed in PR 2.
    readonly property color secondary: accent.base
```

> Note: `secondarySoft` and `onBrandMuted` frozen literals (`:34,41`) are NOT touched in PR 1 — they stay literals until Task 11 rewires them to `accent.soft` / `brand.onMuted`.

- [ ] **Step 4: Run, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_qml_theme --output-on-failure 2>&1 | tail -6
```
Expected: PASS.

- [ ] **Step 5: Commit.**

```
git add qt-app/quick/qml/theme/Theme.qml qt-app/quick/tests/tst_qml_theme.qml
git commit -m "feat(theme): add role-based brand/accent tokens; keep old tokens as aliases"
```

---

### Task 4: Extract the `buildPalette(primarySeed, secondarySeed)` seam

**Files:**
- Modify: `qt-app/core/brandtheme.h` (declare `buildPalette`), `qt-app/core/brandtheme.cpp:321-360` (extract)
- Test: `qt-app/tests/tst_brandtheme.cpp`

**Interfaces:**
- Produces: `BrandPalette BrandTheme::buildPalette(const QColor &primarySeed, const QColor &secondarySeed)` — the pure build step, callable from tests without a logo image. `extractPalette` calls it after seed extraction.

- [ ] **Step 1: Write the failing seam test.**

```cpp
    void buildPaletteIsCallableFromSeeds();   // add to slots
```

```cpp
void TestBrandTheme::buildPaletteIsCallableFromSeeds()
{
    // Maroon primary, gold secondary — the reference case.
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    QVERIFY(p.brandBase.isValid());
    QVERIFY(p.accentBase.isValid());
    QVERIFY(p.brandBase != p.accentBase);
}
```

- [ ] **Step 2: Run, expect FAIL** (`buildPalette` undeclared).

- [ ] **Step 3: Declare + extract.** In `brandtheme.h`, add `BrandPalette buildPalette(const QColor &primarySeed, const QColor &secondarySeed);` to the `BrandTheme` namespace. In `brandtheme.cpp`, move the current build body (`:321-360`, the `p.adminPrimary = enforceOnWhite(...)` … through the neutral copy) into a new `buildPalette(primarySeed, secondarySeed)` function returning `p`, and have `extractPalette` call `return buildPalette(primarySeed, secondarySeed);` after computing the seeds (`:296-319`). Keep the *current* (still surface-shaped) transforms in this task — this is a pure refactor; the role rewrite is Task 6. Map the temporary field names from Task 1 Step 5.

- [ ] **Step 4: Run full brand-theme suite, expect PASS** (behaviour unchanged by the extraction).

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure 2>&1 | tail -8
```

- [ ] **Step 5: Commit.**

```
git add qt-app/core/brandtheme.h qt-app/core/brandtheme.cpp qt-app/tests/tst_brandtheme.cpp
git commit -m "refactor(brandtheme): extract pure buildPalette(seeds) seam for testability"
```

---

### Task 5: Parameterise contrast enforcement (target + accent lighten)

**Files:**
- Modify: `qt-app/core/brandtheme.h:22` (or a new constant block), `qt-app/core/brandtheme.cpp:242-253` (`enforceOnWhite`)
- Test: `qt-app/tests/tst_brandtheme.cpp`

**Interfaces:**
- Produces: `QColor enforceContrast(const QColor &c, const QColor &against, double target)` — darkens `c` in `kEnforceStep` increments until `contrastRatio(c, against) >= target`, capped at `kEnforceMaxIterations`. And `QColor raiseToContrast(const QColor &c, const QColor &against, double target)` — LIGHTENS `c` (raises HSV value) until `contrastRatio(c, against) >= target`. `enforceOnWhite(c)` becomes `enforceContrast(c, white, MinContrast)` for back-compat.

- [ ] **Step 1: Write failing tests.**

```cpp
    void enforceContrastReachesTarget();
    void raiseToContrastLightensDarkAccent();
```

```cpp
void TestBrandTheme::enforceContrastReachesTarget()
{
    const QColor white(Qt::white);
    // A mid-grey darkened until it clears 4.5 vs white.
    const QColor out = BrandTheme::enforceContrast(QColor("#999999"), white, 4.5);
    QVERIFY(BrandColorMath::contrastRatio(out, white) >= 4.5);
}

void TestBrandTheme::raiseToContrastLightensDarkAccent()
{
    // A dark navy accent on a dark maroon brand must be LIGHTENED to be visible.
    const QColor brand("#4A0E0B");   // dark maroon
    const QColor out = BrandTheme::raiseToContrast(QColor("#1A2340"), brand, 3.0); // dark navy
    QVERIFY(BrandColorMath::contrastRatio(out, brand) >= 3.0);
    QVERIFY(out.valueF() > QColor("#1A2340").valueF()); // it got lighter, not darker
}
```

- [ ] **Step 2: Run, expect FAIL** (functions undeclared).

- [ ] **Step 3: Implement.** In `brandtheme.h`, declare both functions. In `brandtheme.cpp`, generalise `enforceOnWhite`:

```cpp
QColor enforceContrast(const QColor &c, const QColor &against, double target)
{
    QColor result = c;
    int iterations = 0;
    while (contrastRatio(result, against) < target && iterations < kEnforceMaxIterations) {
        result = shade(result, kEnforceStep);   // darken toward the target
        ++iterations;
    }
    return result;
}

QColor raiseToContrast(const QColor &c, const QColor &against, double target)
{
    float h, s, v, a;
    c.getHsvF(&h, &s, &v, &a);
    int iterations = 0;
    QColor result = c;
    while (contrastRatio(result, against) < target && iterations < kEnforceMaxIterations) {
        v = qMin(1.0f, v + 0.06f);               // raise value (lighten), preserve hue/sat
        result = QColor::fromHsvF(h, s, v, a);
        ++iterations;
    }
    return result;
}

QColor enforceOnWhite(const QColor &c) { return enforceContrast(c, QColor(Qt::white), MinContrast); }
```

- [ ] **Step 4: Run, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure 2>&1 | tail -8
```

- [ ] **Step 5: Commit.**

```
git add qt-app/core/brandtheme.h qt-app/core/brandtheme.cpp qt-app/tests/tst_brandtheme.cpp
git commit -m "feat(brandtheme): parameterise contrast enforcement (target + accent lighten)"
```

---

### Task 6: Role-based derivation in `buildPalette` + replace obsolete tests with role invariants

**Files:**
- Modify: `qt-app/core/brandtheme.cpp` (`buildPalette` body), `qt-app/core/brandtheme.h` (`kAccentDeepShade` etc. if needed)
- Test: `qt-app/tests/tst_brandtheme.cpp:178,326,329-349` (replace) + new invariants

**Interfaces:**
- Produces: `buildPalette` fills all eleven brand/accent fields per §4 of the spec: `brandBase` = `enforceContrast(primarySeed, white, 4.5)`; `brandDeep` = `shade(brandBase, -0.28)`; `brandSoft` = `mix(brandBase, white, 0.90)`; `brandOn` = white; `brandOnMuted` = `mix(brandOn, brandBase, 0.25)`; `brandText` = `enforceContrast(brandBase, card, 4.5)`; `accentBase` = `raiseToContrast(secondarySeed, brandBase, 3.0)`; `accentDeep` = `shade(accentBase, -0.28)`; `accentSoft` = `mix(accentBase, white, 0.88)`; `accentOn` = `brandDeep` if ≥4.5 vs `accentBase` else `shade(accentBase, -0.60)` else best of black/white; `accentText` = `shade(accentBase, -0.40)` then `enforceContrast(..., card, 4.5)` (also verified vs `accentSoft`).

- [ ] **Step 1: Replace the obsolete surface-distinctness tests.** Delete `fallbackKeepsAdminAndKioskDistinct` (`:178`) and rename/retarget `twoToneLogoMapsPrimaryAndSecondary` (`:326`) and `generatedPalettesMeetMinContrast` (`:329-349`). Add the role invariants:

```cpp
void TestBrandTheme::rolePaletteMeetsSplitContrastFloors()
{
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    using BrandColorMath::contrastRatio;
    QVERIFY(contrastRatio(p.brandOn,   p.brandBase)  >= 4.5); // text on brand fill
    QVERIFY(contrastRatio(p.accentOn,  p.accentBase) >= 4.5); // text on accent fill
    QVERIFY(contrastRatio(p.accentBase, p.brandBase) >= 3.0); // graphical highlight on dark
    QVERIFY(contrastRatio(p.accentText, p.card)      >= 4.5); // accent-as-text on paper
    QVERIFY(contrastRatio(p.accentText, p.accentSoft)>= 4.5); // accent-as-text on its tint
    QVERIFY(contrastRatio(p.brandText,  p.card)      >= 4.5); // brand-as-text on paper
    QVERIFY(contrastRatio(p.brandOnMuted, p.brandBase) >= 4.5); // muted nav label
}

void TestBrandTheme::brandAndAccentAreDistinctHues()
{
    const BrandPalette p = BrandTheme::buildPalette(QColor("#7E1A15"), QColor("#E8B10E"));
    const double dh = std::abs(p.brandBase.hsvHueF()*360.0 - p.accentBase.hsvHueF()*360.0);
    QVERIFY(qMin(dh, 360.0 - dh) >= 30.0);   // visibly different colours
}
```

- [ ] **Step 2: Run, expect FAIL** (invariants unmet — build still does the old surface transforms from Task 4).

- [ ] **Step 3: Rewrite `buildPalette`'s body** to the role transforms above (the `p.card` used by `brandText`/`accentText` is the fixed neutral; copy neutrals from `fallbackPalette()` BEFORE deriving `brandText`/`accentText` so `p.card` is populated). Apply the exact §4 transforms; `accentOn` three-branch:

```cpp
    // ... after neutrals are copied so p.card is valid ...
    p.brandBase    = enforceContrast(primarySeed, white, 4.5);
    p.brandDeep    = shade(p.brandBase, kHoverShade);
    p.brandSoft    = mix(p.brandBase, white, kSoftMixToWhite);
    p.brandOn      = white;
    p.brandOnMuted = mix(p.brandOn, p.brandBase, 0.25);
    if (contrastRatio(p.brandOnMuted, p.brandBase) < 4.5)      // clamp if the mix fails
        p.brandOnMuted = raiseToContrast(p.brandOnMuted, p.brandBase, 4.5);  // LIGHTEN: a light label on a dark fill gains contrast by moving toward white, not by darkening
    p.brandText    = enforceContrast(p.brandBase, p.card, 4.5);

    p.accentBase   = raiseToContrast(secondarySeed, p.brandBase, 3.0);
    p.accentDeep   = shade(p.accentBase, kHoverShade);
    p.accentSoft   = mix(p.accentBase, white, 0.88);
    if (contrastRatio(p.brandDeep, p.accentBase) >= 4.5)
        p.accentOn = p.brandDeep;
    else if (contrastRatio(shade(p.accentBase, kOnColorDeepShade), p.accentBase) >= 4.5)
        p.accentOn = shade(p.accentBase, kOnColorDeepShade);
    else
        p.accentOn = (contrastRatio(QColor(Qt::white), p.accentBase) >= contrastRatio(QColor(Qt::black), p.accentBase))
                     ? QColor(Qt::white) : QColor(Qt::black);
    p.accentText   = enforceContrast(shade(p.accentBase, -0.40), p.card, 4.5);
    if (contrastRatio(p.accentText, p.accentSoft) < 4.5)
        p.accentText = enforceContrast(p.accentText, p.accentSoft, 4.5);
```

- [ ] **Step 4: Run, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build -R tst_brandtheme --output-on-failure 2>&1 | tail -10
```
Expected: role invariants pass. If a floor is unreachable for the reference maroon/gold seeds, the derivation is wrong — do not weaken the test.

- [ ] **Step 5: Commit.**

```
git add qt-app/core/brandtheme.cpp qt-app/core/brandtheme.h qt-app/tests/tst_brandtheme.cpp
git commit -m "feat(brandtheme): role-based split-contrast derivation; replace surface-distinctness tests with role invariants"
```

---

### Task 7: Quality gate + `didFallBack` + `RegenResult` + adversarial-seed test

**Files:**
- Modify: `qt-app/core/brandthemedata.h` (`BrandingConfig.didFallBack`), `qt-app/core/brandtheme.h` (`RegenResult`, `paletteIsUsable` decl), `qt-app/core/brandtheme.cpp` (gate in `extractPalette`, `regenerateFromLogo` sets flag), `qt-app/quick/viewmodels/ThemeViewModel.h/.cpp` (`regenerateFromImportedLogo` → `RegenResult`)
- Test: `qt-app/tests/tst_brandtheme.cpp`

**Interfaces:**
- Produces: `enum class RegenResult { Ok, FellBack, Failed }`. `BrandingConfig.didFallBack` (bool). `bool paletteIsUsable(const BrandPalette&, const QColor &primarySeed, const QColor &secondarySeed)`. `ThemeViewModel::regenerateFromImportedLogo` returns `RegenResult` (was `bool`).

- [ ] **Step 1: Write the failing adversarial-seed test.**

```cpp
    void badSeedsEitherMeetInvariantsOrFallBack();  // add to slots
```

```cpp
void TestBrandTheme::badSeedsEitherMeetInvariantsOrFallBack()
{
    struct Case { QColor a, b; };
    const QVector<Case> cases = {
        {QColor("#FDFDFD"), QColor("#FCFCFC")},  // near-white / near-white
        {QColor("#050505"), QColor("#0A1030")},  // black / navy (both dark)
        {QColor("#FF00FF"), QColor("#00FF00")},  // neon / neon
        {QColor("#888888"), QColor("#8A8A8A")},  // greyscale-ish, near-mono
    };
    const BrandPalette fb = BrandTheme::fallbackPalette();
    using BrandColorMath::contrastRatio;
    for (const Case &c : cases) {
        const BrandPalette p = BrandTheme::buildPalette(c.a, c.b);
        const bool usable = BrandTheme::paletteIsUsable(p, c.a, c.b);
        if (!usable) continue;  // gate would fall back — acceptable
        // If declared usable, it MUST meet the floors.
        QVERIFY(contrastRatio(p.brandOn,    p.brandBase)  >= 4.5);
        QVERIFY(contrastRatio(p.accentBase, p.brandBase)  >= 3.0);
        QVERIFY(p.accentBase.hsvSaturationF() >= 0.15);   // not washed out
        QVERIFY(p.accentBase.valueF() <= 0.97);           // not near-white
    }
}
```

- [ ] **Step 2: Run, expect FAIL** (`paletteIsUsable` undeclared).

- [ ] **Step 3: Implement the gate + flag + enum.**
  - `brandthemedata.h`: add `bool didFallBack = false;` to `BrandingConfig`.
  - `brandtheme.h`: `enum class RegenResult { Ok, FellBack, Failed };` and `bool paletteIsUsable(const BrandPalette &p, const QColor &primarySeed, const QColor &secondarySeed);`
  - `brandtheme.cpp`: implement `paletteIsUsable` checking (a) `hueDistanceDeg` between seeds ≥ floor, (b) `primarySeed.hsvSaturationF() ≥` floor, (c) post-clamp `p.accentBase.hsvSaturationF() ≥ 0.15 && p.accentBase.valueF() ≤ 0.97`, (d) the required 4.5/3.0 floors hold via `contrastRatio`, **(e) `contrastRatio(p.brandOnMuted, p.brandBase) ≥ 4.5`**. Check (e) is load-bearing: Task 6's `brandOnMuted` recovery lightens toward white, but for ~half of clamp-firing brand seeds (vivid violet/blue/magenta hues) even a fully-lightened muted label caps below 4.5 against a mid-luminance `brandBase` — no other gate check detects an illegible muted nav label, so without (e) such a palette ships. In `extractPalette`, after `buildPalette`, `if (!paletteIsUsable(p, primarySeed, secondarySeed)) return fallbackPalette();`. In `regenerateFromLogo`, set `config.didFallBack = (the palette is the fallback)` — compute it from the usable check so the flag reflects reality.
  - `ThemeViewModel.cpp`: change `regenerateFromImportedLogo` to return `RegenResult` — `Failed` if the logo can't be read, `FellBack` if `config.didFallBack`, else `Ok`. `ThemeViewModel.h`: change the signature and register the enum for QML with `Q_ENUM(RegenResult)` (declare the enum on the class or expose the C++ enum via `qmlRegisterUncreatableMetaObject`/`Q_ENUM`; simplest: re-declare `enum class RegenResult` on `ThemeViewModel` with `Q_ENUM` and map).

> QML call site `SettingsScreen.qml:530` currently ignores the return — it keeps compiling and stays invisible. Consuming `FellBack` is Task 9.

- [ ] **Step 4: Run, expect PASS** (adversarial test + existing `greyscaleLogoFallsBack` still green).

```
cmake --build qt-app/build && ctest --test-dir qt-app/build --output-on-failure 2>&1 | tail -8
```

- [ ] **Step 5: Commit.**

```
git add qt-app/core/brandthemedata.h qt-app/core/brandtheme.h qt-app/core/brandtheme.cpp qt-app/quick/viewmodels/ThemeViewModel.h qt-app/quick/viewmodels/ThemeViewModel.cpp qt-app/tests/tst_brandtheme.cpp
git commit -m "feat(brandtheme): bad-logo quality gate + didFallBack + RegenResult return"
```

**End of PR 1.** Open PR 1 with the `create-pr` skill; body must say *"no visible change until the next logo upload,"* never "nothing changes." Land it before starting PR 2.

---

# PR 2 — QML sweep + visible change (Tasks 8–12)

## File map (PR 2)

- `qt-app/quick/qml/admin/SettingsScreen.qml` — `SectionStatus` neutral state; `schoolStatus` placement; consume `RegenResult.FellBack`.
- `qt-app/quick/viewmodels/SettingsViewModel.h/.cpp` — `lastFallbackLogoHash` persisted state + a `Q_INVOKABLE` to read/record it.
- `qt-app/quick/qml/theme/Theme.qml` — `elevation.*` de-hardcode; delete alias bindings (Task 12).
- All `qt-app/quick/qml/**` + `qt-app/quick/tests/**` consuming `Theme.brand.*`/`Theme.secondary*` — role sweep.
- `qt-app/quick/viewmodels/ThemeViewModel.h` — delete old-name aliases (Task 12).
- A new grep-guard test (Task 12).

---

### Task 8: Extend `SectionStatus` with a neutral / info state

**Files:**
- Modify: `qt-app/quick/qml/admin/SettingsScreen.qml:82-91` (the `SectionStatus` component)
- Test: `qt-app/quick/tests/tst_qml_admin.qml`

**Interfaces:**
- Produces: `SectionStatus` gains an `isNeutral` bool (default false). When `isNeutral`, it renders in a neutral token (`Theme.mutedText`), overriding the error/success choice.

- [ ] **Step 1: Write the failing test.** In `tst_qml_admin.qml`'s SettingsScreen TestCase:

```qml
        function test_sectionStatusHasNeutralInfoState() {
            var s = findChild(settings, "schoolStatus");
            verify(s !== null);
            s.isNeutral = true; s.text = "info";
            compare(s.color.toString(), Theme.mutedText.toString());
        }
```

(`schoolStatus` is added in Task 9; if running Task 8 in isolation, assert against an existing `SectionStatus` instance and add an `isNeutral` toggle there.)

- [ ] **Step 2: Run, expect FAIL** (`isNeutral` undefined / `schoolStatus` missing).

- [ ] **Step 3: Implement.** In the `SectionStatus` component:

```qml
    component SectionStatus: Text {
        property bool isError: false
        property bool isNeutral: false
        Layout.fillWidth: true
        textFormat: Text.PlainText
        visible: text.length > 0
        color: isNeutral ? Theme.mutedText : (isError ? Theme.error : Theme.success)
        wrapMode: Text.WordWrap
        font.family: Theme.typography.sans
        font.pixelSize: Theme.typography.body
    }
```

- [ ] **Step 4: Run, expect PASS** (after Task 9 adds `schoolStatus`, or against an existing instance).

- [ ] **Step 5: Commit.**

```
git add qt-app/quick/qml/admin/SettingsScreen.qml qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(settings): add neutral/info state to SectionStatus"
```

---

### Task 9: Wire the bad-logo fallback notice (fire-once) on the School card

**Files:**
- Modify: `qt-app/quick/viewmodels/SettingsViewModel.h/.cpp` (`lastFallbackLogoHash`), `qt-app/quick/qml/admin/SettingsScreen.qml` (School card `schoolStatus` + import handler)
- Test: `qt-app/quick/tests/tst_qml_admin.qml` (stub-VM), `qt-app/quick/tests/tst_settingsviewmodel.cpp` (hash persistence)

**Interfaces:**
- Consumes: `RegenResult` (Task 7), `SectionStatus.isNeutral` (Task 8).
- Produces: `SettingsViewModel` gains `Q_INVOKABLE QString lastFallbackLogoHash() const` + `Q_INVOKABLE void recordFallbackLogoHash(const QString&)` persisting via the settings store; and a `Q_INVOKABLE QString logoHashFor(const QString &path) const` that SHA-256s the file so QML can compare. `SettingsScreen` School card has `SectionStatus { objectName: "schoolStatus" }`.

- [ ] **Step 1: Write the failing hash-persistence test** in `tst_settingsviewmodel.cpp`:

```cpp
void TestSettingsViewModel::fallbackLogoHashPersists()
{
    SettingsViewModel vm;
    QVERIFY(vm.lastFallbackLogoHash().isEmpty());
    vm.recordFallbackLogoHash(QStringLiteral("abc123"));
    QCOMPARE(vm.lastFallbackLogoHash(), QStringLiteral("abc123"));
    SettingsViewModel vm2;   // reconstructed — reads persisted value
    QCOMPARE(vm2.lastFallbackLogoHash(), QStringLiteral("abc123"));
}
```

- [ ] **Step 2: Run, expect FAIL** (methods undefined).

- [ ] **Step 3: Implement the VM state.** In `SettingsViewModel`, add the three `Q_INVOKABLE`s. `lastFallbackLogoHash`/`recordFallbackLogoHash` read/write a `branding/lastFallbackLogoHash` key via `AppSettings` (the isolated settings scope from 4c). `logoHashFor` reuses the SHA-256 the engine already computes — expose `BrandTheme`'s logo-hash helper or recompute with `QCryptographicHash::hash(file.readAll(), Sha256).toHex()`.

- [ ] **Step 4: Add the School-card notice + import handler.** In `SettingsScreen.qml`, add `SectionStatus { objectName: "schoolStatus" }` to the School-identity card's column, and change the logo-import `onAccepted` (around `:530`) to consume the tri-state:

```qml
                var result = Theme._vm.regenerateFromImportedLogo(vm.logoPath);
                if (result === ThemeViewModel.FellBack) {
                    var h = vm.logoHashFor(vm.logoPath);
                    if (h !== vm.lastFallbackLogoHash()) {           // fire-once
                        schoolStatus.isNeutral = true;
                        schoolStatus.text = qsTr("This logo couldn't produce a usable colour palette. LOAMS is using the default theme instead.");
                        vm.recordFallbackLogoHash(h);
                    }
                } else {
                    schoolStatus.text = "";   // a good logo clears any prior notice
                }
```

- [ ] **Step 5: Write a stub-VM QuickTest** proving the notice shows once for a `FellBack` and not on re-open (mirror the 4c stub pattern; the stub returns `ThemeViewModel.FellBack` and a fixed hash).

- [ ] **Step 6: Run, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build --output-on-failure 2>&1 | tail -8
```

- [ ] **Step 7: Commit.**

```
git add qt-app/quick/viewmodels/SettingsViewModel.h qt-app/quick/viewmodels/SettingsViewModel.cpp qt-app/quick/qml/admin/SettingsScreen.qml qt-app/quick/tests/tst_settingsviewmodel.cpp qt-app/quick/tests/tst_qml_admin.qml
git commit -m "feat(settings): fire-once bad-logo fallback notice on the School card"
```

---

### Task 10: De-hardcode the brand-coloured elevation shadows

**Files:**
- Modify: `qt-app/quick/qml/theme/Theme.qml:78-84` (`elevation`), and the consuming shadow component(s)
- Test: `qt-app/quick/tests/tst_qml_theme.qml`

**Interfaces:**
- Produces: the three brand-tinted elevation tokens (`hover`, `heroFill`, `ctaGold`) carry a LIVE colour from `Theme.brand.deep` / `Theme.accent.base` via `Qt.alpha(...)`, not a frozen `rgba()` literal. `modal` stays neutral (black).

- [ ] **Step 1: Decide the representation (spec §4.1 sub-decision).** The elevation values are opaque CSS-shadow strings today. Prefer restructuring each into fields the shadow consumer assembles: `{ x, y, blur, color }`. Grep the consumer: `grep -rn "elevation\." qt-app/quick/qml` to find every read site and how the string is parsed. If the consumer count is small, restructure; if broad, split ONLY the colour out (option b in §4.1) and leave geometry as data. Record which route in the commit.

- [ ] **Step 2: Write the failing test** asserting the shadow colour tracks the brand:

```qml
        function test_elevationShadowsTrackBrand() {
            // hover/heroFill shadow colour derives from brand.deep, not a maroon literal.
            verify(Theme.elevation.hover.color !== undefined);
            compare(Theme.elevation.hover.color.toString(),
                    Qt.alpha(Theme.brand.deep, 0.10).toString());
            compare(Theme.elevation.ctaGold.color.toString(),
                    Qt.alpha(Theme.accent.base, 0.30).toString());
        }
```

(If option b is chosen, assert at the call site instead.)

- [ ] **Step 3: Run, expect FAIL.**

- [ ] **Step 4: Implement.** Restructure the three brand-tinted tokens, e.g.:

```qml
    readonly property QtObject elevation: QtObject {
        readonly property var resting:  ({ x: 0,  y: 0,  blur: 0,  color: "transparent" })
        readonly property var hover:    ({ x: 0,  y: 12, blur: 26, color: Qt.alpha(root.brand.deep, 0.10) })
        readonly property var heroFill: ({ x: 0,  y: 12, blur: 30, color: Qt.alpha(root.brand.deep, 0.25) })
        readonly property var ctaGold:  ({ x: 0,  y: 5,  blur: 14, color: Qt.alpha(root.accent.base, 0.30) })
        readonly property var modal:    ({ x: 0,  y: 24, blur: 48, color: Qt.rgba(0,0,0,0.30) })
    }
```

Update the shadow consumer(s) to read `.x/.y/.blur/.color`.

- [ ] **Step 5: Run, expect PASS.**

- [ ] **Step 6: Commit.**

```
git add qt-app/quick/qml/theme/Theme.qml qt-app/quick/tests/tst_qml_theme.qml
git commit -m "feat(theme): derive brand-tinted elevation shadows from tokens via Qt.alpha"
```

---

### Task 11: Sweep the ~61 QML call sites to role tokens + derive the two neutral-adjacent literals

**Files:**
- Modify: the 16 QML files using `Theme.brand.*` (47 sites) and 7 using `Theme.secondary*` (14 sites); `qt-app/quick/qml/theme/Theme.qml:34,41` (`secondarySoft`, `onBrandMuted`)
- Test: existing QuickTests are the regression gate

**Interfaces:**
- Consumes: `Theme.brand.base/deep/soft/on/onMuted/text`, `Theme.accent.base/deep/soft/on/text` (Task 3).

- [ ] **Step 1: Enumerate the sites.**

```
grep -rn "Theme\.brand\." qt-app/quick/qml | wc -l   # expect ~47
grep -rn "Theme\.secondary" qt-app/quick/qml | wc -l # expect ~14
```

- [ ] **Step 2: Migrate role-by-role, per the mockups (§6.2).** For each site choose the family by the element's ROLE, not by its old surface:
  - Sidebar gradient / primary buttons / ID+stat text / focus ring / avatar chip / default chart bar → `brand.*` (`admin*`→`brand.base/deep/soft`, `onPrimary`→`brand.on`).
  - **Admin search CTA, active nav item, chart PEAK bar, freshest/newest row highlight, gold ring** → `accent.*` (`kiosk*`→`accent.base/deep/soft`, `onKiosk`→`accent.on`, `secondary`→`accent.base`, `secondarySoft`→`accent.soft`).
  - Inactive nav label → `brand.onMuted`.
  - Gold text on a light card → `accent.text` (never `accent.base`).
  Do this in small commits (e.g. per file or per screen) so a mis-assignment is bisectable.

- [ ] **Step 3: Derive the two frozen literals.** In `Theme.qml`, delete the `secondarySoft` and `onBrandMuted` hex literals (`:34,41`) and repoint their consumers to `Theme.accent.soft` / `Theme.brand.onMuted`.

- [ ] **Step 4: Build + run full suite after each screen; expect green.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build --output-on-failure 2>&1 | tail -6
```

- [ ] **Step 5: Commit (grouped by screen/concern via the commit skill).** Example final:

```
git commit -m "feat(theme): sweep QML call sites to role tokens; derive secondarySoft/onBrandMuted"
```

---

### Task 12: Remove the API aliases + grep-guard test + finish test updates

**Files:**
- Modify: `qt-app/quick/qml/theme/Theme.qml` (delete alias bindings), `qt-app/quick/viewmodels/ThemeViewModel.h` (delete old Q_PROPERTYs + forwarders), any remaining `qt-app/quick/tests/**` sites (e.g. `tst_qml_components.qml` `.secondary`)
- Test: a new grep-guard test

**Interfaces:**
- Produces: zero old-token occurrences anywhere under `qt-app/quick/qml/` **and** `qt-app/quick/tests/`. **JSON read-compat aliases in `paletteFromJson` are NOT touched — they stay forever.**

- [ ] **Step 1: Write the failing grep-guard test.** Add a QtTest (or a CMake `add_test` shell check) asserting no old token names remain:

```cpp
void TestThemeGuard::noDeprecatedTokenNamesRemain()
{
    // Scan the QML + QuickTest trees for any old token name. Read-compat JSON
    // keys in C++ are exempt (they are permanent, see spec §6.4).
    const QStringList banned = {
        "brand.admin", "brand.adminHover", "brand.adminSoft",
        "brand.kiosk", "brand.kioskHover", "brand.kioskSoft",
        "brand.onPrimary", "brand.onKiosk", "Theme.secondary",
        "secondarySoft", "onBrandMuted"
    };
    const QStringList roots = { SRC_QML_DIR, SRC_QML_TEST_DIR }; // passed via target_compile_definitions
    for (const QString &root : roots)
        for (const QString &name : banned)
            QVERIFY2(!dirContains(root, name), qPrintable(root + " still references " + name));
}
```

(Provide `dirContains` as a small recursive-grep helper; pass `SRC_QML_DIR`/`SRC_QML_TEST_DIR` via `target_compile_definitions` in the test's CMake registration.)

- [ ] **Step 2: Run, expect FAIL** (aliases still referenced or still defined).

- [ ] **Step 3: Delete the aliases.**
  - `Theme.qml`: remove the deprecated alias bindings (`brand.admin/adminHover/adminSoft/kiosk/kioskHover/kioskSoft/onPrimary/onKiosk`, `Theme.secondary`).
  - `ThemeViewModel.h`: remove the old `Q_PROPERTY` lines and the forwarding accessors (`adminPrimary`, …, `secondary`).
  - Migrate any lingering old-token references in `qt-app/quick/tests/**`.

- [ ] **Step 4: Run full suite + grep-guard, expect PASS.**

```
cmake --build qt-app/build && ctest --test-dir qt-app/build --output-on-failure 2>&1 | tail -8
```
Expected: `100% tests passed`, grep-guard green.

- [ ] **Step 5: Commit.**

```
git add qt-app/quick/qml/theme/Theme.qml qt-app/quick/viewmodels/ThemeViewModel.h qt-app/quick/tests
git commit -m "refactor(theme): remove deprecated API aliases; add grep-guard (JSON read-compat stays)"
```

- [ ] **Step 6: GUI walkthrough (spec §8.6) — human, required.** Build Debug + Release clean, no new warnings. Run `WITSQuick`: upload a two-tone logo → confirm Admin AND Kiosk pick up brand + accent by role (sidebar/CTA/active-nav/peak-bar); eyeball `brand.onMuted` (inactive nav label) for warmth (§8.6 — switch to a warm-biased mix if it reads cool). Upload a greyscale/monochrome logo → confirm the `schoolStatus` fallback notice fires ONCE and the theme reverts to default. Record the outcome in the PR body.

**End of PR 2.** Open with `create-pr`; this is the visible change. Merge is the owner's call.

---

## Self-review (against the spec)

- **§3 token model** → Tasks 1–3 (struct fields, VM props, QML tokens) + Task 6 (derivation fills them). ✓
- **§4 derivation + split contrast** → Task 5 (enforce target + accent lighten) + Task 6 (role transforms). The exact §4 transforms are pasted in Task 6 Step 3. ✓
- **§4.1 neutrals fixed + elevation shadows** → neutrals unchanged (copied from `fallbackPalette`); elevation de-hardcoded in Task 10. ✓
- **§5 quality gate + fire-once notice** → Task 7 (gate + `didFallBack` + `RegenResult`) + Tasks 8–9 (`SectionStatus` neutral state, `schoolStatus`, fire-once hash). ✓
- **§6 migration (2 PRs, 3 landings)** → PR 1 = Tasks 1–7 (commit-grouped as rename+aliases / derivation); PR 2 = Tasks 8–12. JSON read-compat permanent (Task 1, never deleted in Task 12). ✓
- **§7 token-stability** → out of scope (spec §9); no task. ✓
- **§8 testing** → JSON round-trip incl. old-key (Task 1), role invariants replacing surface-distinctness (Task 6), adversarial seeds (Task 7), grep-guard incl. tests dir (Task 12), GUI walkthrough incl. onMuted warmth (Task 12 Step 6). ✓
- **Type consistency:** field names, `RegenResult`, `paletteIsUsable`, `enforceContrast`/`raiseToContrast`, `lastFallbackLogoHash`/`recordFallbackLogoHash`/`logoHashFor` are used identically across the tasks that define and consume them. ✓
- **Open plan-level detail (spec Round-2 Low):** Task 9 Step 3 resolves how `SettingsViewModel` gets the current logo hash — `logoHashFor(path)` recomputes the SHA-256 — so `lastFallbackLogoHash` is never compared against an empty value. ✓
