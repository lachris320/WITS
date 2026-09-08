# LOAMS 2.0 — Phase 5: Admin Dark Mode (Design Spec)

**Date:** 2026-09-08
**Status:** Design — approved for planning
**Roadmap:** `docs/superpowers/specs/2026-07-07-loams2-qtquick-design.md` §10 (Light + dark), Risk 3 (dark has no design reference).

## 1. Goal

Add a **Light / Dark / System** theme choice to LOAMS 2.0, scoped to the **admin surfaces only**. The kiosk always renders light. Dark mode reuses the existing `BrandTheme` WCAG contrast machinery so logo-derived brand colors stay legible on dark surfaces.

### In scope
- A user-selectable mode: `Light`, `Dark`, `System` — persisted across launches.
- `System` follows the OS appearance live (`QStyleHints::colorScheme` / `colorSchemeChanged`, Qt 6.11).
- A **fixed hand-tuned dark neutral palette** (bg / card / text / border / muted / sidebar), anchored to the reference dark option **`1b`** (`#0C1524` navy family, cream `#E8EDF5` text, gold `#F0C64A` accent).
- Dark-surface **brand/accent** roles re-derived from the same logo seeds and re-contrast-checked against the dark surfaces.
- A picker in admin **Settings**.

### Out of scope (explicitly)
- **Kiosk dark mode.** The kiosk stays light in every mode. Kiosk and admin never render simultaneously (the `AppShell` `Loader` swaps one for the other), so "admin-only" is enforceable by surface, not by duplicating tokens.
- Per-widget dark overrides or a dark *logo* variant.
- Animated light↔dark transitions (tokens flip instantly; acceptable).

## 2. Key decisions (locked)

| Decision | Choice | Why |
|---|---|---|
| Scope | Admin surfaces only | Kiosk is a public wall display; its light brand identity is deliberate. |
| Dark neutrals | Fixed, hand-tuned, anchored to reference `1b` | No full dark design reference exists (roadmap Risk 3); a curated set beats math-only neutrals. |
| Brand roles in dark | Derived from existing seeds + re-contrast-checked | Keeps the logo-driven identity; avoids a second seed source. |
| Mode options | `Light` / `Dark` / `System` | Matches the roadmap ("mode is a local QSettings key") and OS-follow expectation. |
| Persistence | `QSettings` key `theme/mode` via the test-isolated `AppSettings` | Same persistence seam the rest of settings already use. |

## 3. Architecture

Three resolution layers combine into one effective boolean, `Theme.isDark`:

```
userMode (Light|Dark|System)  ─┐
system colorScheme (Light|Dark)─┤→ resolvedDark (bool)  ─┐
                                                          ├→ isDark
Navigator.currentSurface == Admin ───────────────────────┘
```

- `resolvedDark = (userMode == Dark) || (userMode == System && systemColorScheme == Dark)`
- `isDark = resolvedDark && Navigator.currentSurface === Navigator.Admin`

Because `isDark` is a QML binding over `Navigator.currentSurface` (a `QML_SINGLETON` with `currentSurfaceChanged`) and over the VM's `resolvedDark` (with NOTIFY), navigating kiosk↔admin or toggling the OS/mode flips every token automatically. The kiosk term makes kiosk always evaluate `isDark == false`.

### 3.1 Component responsibilities

**`BrandTheme` (core engine, C++)** — *pure, unit-testable, light path untouched.*
- Add `BrandPalette darkPalette(const BrandPalette &light)`:
  - Copies `brandBase` / `accentBase` (the brand fills stay the brand color).
  - Attaches a fixed `darkNeutrals()` struct: `appBackground`, `card`, `sidebarBase`, `text`, `mutedText`, `border`, `success`, `error` — the hand-tuned dark set.
  - Re-derives the *on-light* brand roles for a **dark** surface: `brandText` / `accentText` / `brandSoft` / `accentSoft` / `brandOnMuted` (and any role whose light value assumed a light background), then runs the existing `enforceContrast` / `raiseToContrast` against the dark `card` (text floor 4.5, UI floor 3.0). `raiseToContrast` already lightens toward legibility, which is the correct direction on dark.
- `current()` remains the **light** palette; the dark palette is derived on demand from it — no churn of the global on navigation.

**`ThemeViewModel` (QML-facing wrapper, C++)** — *holds both palettes, owns mode + resolution.*
- New `Q_PROPERTY mode` (string `"Light"|"Dark"|"System"`) with a setter that persists to `AppSettings` (`theme/mode`) and emits; loaded at construction.
- Observe `QGuiApplication::styleHints()->colorScheme()` + `colorSchemeChanged`; expose `resolvedDark` (bool, NOTIFY).
- Expose the **dark** accessors alongside the existing light ones: `cardDark`, `appBackgroundDark`, `textDark`, `mutedTextDark`, `borderDark`, `sidebarBaseDark`, `successDark`, `errorDark`, plus the dark brand roles (`brandTextDark`, `accentTextDark`, `brandSoftDark`, `accentSoftDark`, `brandOnMutedDark`). These read from the cached `darkPalette`.
- Live re-theme gotcha (from Phase 4c/4d): the mode setter that drives the singleton MUST be called on **`Theme._vm`** — the instance the `Theme` singleton created — not a second VM instance. Settings therefore calls `Theme._vm.setMode(...)`.

**`Theme.qml` (QML singleton)** — *decides `isDark`, switches tokens.*
- `readonly property bool isDark: Navigator.currentSurface === Navigator.Admin && _vm.resolvedDark` (replaces the stub at ~line 142).
- `readonly property string mode: _vm.mode` (for the Settings UI; replaces the `"Light"` stub at ~line 141).
- Every neutral token (`appBackground`, `card`, `sidebarBase`, `text`, `mutedText`, `border`, `success`, `error`) becomes `isDark ? _vm.<x>Dark : _vm.<x>`.
- The current **hardcoded light literals** (lines ~42-47: `mutedTextCaption`, `tableHeaderBg`, `rowHairline`, `errorSoft`, `errorBorder`, `scrim`) each gain a dark counterpart and switch on `isDark`. Dark counterparts live in `Theme.qml` (tokenized) or move into the engine's dark neutrals — no raw hex leaks outside `Theme.qml`/the engine.
- Dark brand roles: the `brand`/`accent` sub-objects' text/soft/onMuted members switch on `isDark` to the `*Dark` accessors.

**Admin Settings (QML)** — *the picker.*
- A `Light | Dark | System` selector (an `LSegmented`-style control, or an `LComboBox` if a segmented primitive doesn't exist yet) in `SettingsScreen`, bound to `Theme.mode`, writing via `Theme._vm.setMode(...)`.

### 3.2 Admin sidebar in dark

**Decision (locked, design review 2026-09-08):** the dark sidebar uses a **hand-tuned dark-neutral slate surface**, NOT the light-mode maroon brand fill.

> **Dark-mode sidebar:** Use a hand-tuned dark-neutral slate surface rather than the light-mode maroon brand fill. Brand/maroon remains available through semantic accent roles and selected/interactive states where contrast permits. The sidebar's dark-neutral hue is part of the human-review tuning pass.

Rationale: dark mode must actually reduce luminance rather than be a light UI with only the content area darkened; a large saturated maroon block would dominate the visual mass and flatten hierarchy; the sidebar is structural navigation, so a neutral surface lets content and selected-state accents carry the hierarchy while the brand stays recognizable through accents. Implementation: `sidebarBase` in dark comes from `darkNeutrals()` (deep slate); brand/maroon appears only via `brand`/`accent` accent roles on selected/interactive states, contrast-checked so sidebar text/icons stay ≥ their floors. Exact slate hue is a **human-review tuning item** (§6).

## 4. Data flow

1. Startup: `ThemeViewModel` loads `theme/mode` from `AppSettings`; reads system `colorScheme`; computes `resolvedDark`.
2. `Theme.isDark` binds `resolvedDark && admin-active`.
3. Admin surfaces read `Theme.*` tokens → dark values when `isDark`.
4. User picks a mode in Settings → `Theme._vm.setMode()` persists + emits → `resolvedDark` re-emits → tokens flip.
5. OS appearance changes while on `System` → `colorSchemeChanged` → `resolvedDark` re-emits → tokens flip.
6. Navigate to kiosk → `currentSurface == Kiosk` → `isDark` false → kiosk light.

## 5. Testing

**Engine (Qt Test — `tst_brandtheme` style):**
- `darkPalette()` is deterministic for a given light palette.
- Every dark brand text role meets 4.5:1 and every dark UI role meets 3.0:1 against the dark `card`/surfaces (the same invariant assertions the light palette already carries).
- `darkNeutrals()` are the fixed expected values.

**`ThemeViewModel` (Qt Test, isolated `AppSettings`):**
- `mode` round-trips through persistence (set → new VM instance reads it back).
- `resolvedDark` truth table over (`mode` × `systemColorScheme`).
- `System` follows a simulated `colorScheme` change (drive via a test hook / injected value; do not depend on the real OS).

**QuickTest (`tst_qml_*`, OFFSCREEN):**
- `Theme.isDark` is **false on the kiosk surface** for every mode (surface-scope guard).
- `Theme.isDark` is **true** only when `resolvedDark && admin`.
- Token values differ light vs dark for a representative neutral + a brand role.
- The Settings picker updates `Theme.mode`.

All via `wits_add_qttest()` (+ `OFFSCREEN` for the Quick/painting tests).

## 6. Risks & the human-review round

- **No full dark design reference (roadmap Risk 3).** The spec fixes the *contract* — roles, contrast floors, surface-scoping, persistence — not the final dark hex values. Budget **one human review round** on the running `WITSQuick.exe` admin to tune: the exact dark neutrals, the sidebar hue, and any brand role that reads too hot/dim on dark. Tuning changes literals inside `Theme.qml`/`darkNeutrals()` only; no structural change.
- **Zero-raw-hex rule** stays intact: dark literals live in `Theme.qml` or the engine's dark neutrals, tokenized; opacity variants via `Qt.alpha`.
- **System-follow testability:** `colorScheme` detection must be behind a seam so tests don't depend on the CI host's OS appearance.

## 7. Alternatives considered

- **Pure-QML dark palette** (no engine change): rejected — duplicates the C++ contrast math and puts the fixed dark neutrals in the wrong layer.
- **Global (non-surface-scoped) dark**: rejected — would darken the kiosk, which is out of scope by decision.
- **Swap `BrandTheme::current()` to a dark palette on navigation**: rejected — churns global state on every surface change and complicates the untouched light path; deriving dark on demand and selecting in `Theme.qml` is cleaner.

## 8. Deliverables

- `brandtheme.{h,cpp}`: `darkPalette()`, `darkNeutrals()` + engine tests.
- `brandthemedata.h`: dark-neutral fields on `BrandPalette` (or a `DarkNeutrals` sub-struct).
- `ThemeViewModel.{h,cpp}`: `mode`, `resolvedDark`, `colorScheme` seam, dark accessors + tests.
- `Theme.qml`: real `isDark`/`mode`, all neutral tokens + the 6 literals switch on `isDark`.
- `SettingsScreen.qml` (+ any VM): the Light/Dark/System picker.
- QuickTests for the surface-scope guard + token flip.
