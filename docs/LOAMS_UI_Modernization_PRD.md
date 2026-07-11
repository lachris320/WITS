# LOAMS/WITS UI Modernization & Dynamic Branding — PRD

> Reconstructed 2026-07-06 from the project owner's goal specification (the
> original PRD file was referenced but not present in the repo). Content is
> the owner's spec, organized; nothing added or removed in substance.

## Vision

Modernize the LOAMS/WITS desktop app (Qt 6 Widgets, C++17, CMake) into one
consistent, commercial-grade design system AND add a logo-derived dynamic
branding engine + a Today/This Week Visitor Log toggle — with **ZERO
regressions** to existing functionality. Stay on Qt Widgets; do NOT migrate
to QML.

## Design system reference (attached by owner)

Folder: `../Library System UI Modernization/` (sibling of the repo, outside
git) — `Admin Dashboard.dc.html`, `Library Kiosk v2.dc.html`,
`Kiosk Style Options.dc.html`. **Use this design system for UI.** Key
tokens (both screens share one token set):

- `--brand: #7E1A15` (maroon), `--brand-deep = shade(brand, -0.28)`,
  `--brand-soft = mix(brand, #FFFFFF, 0.90)`
- `--gold: #E8B10E` (accent), `--gold-soft = mix(gold, #FFFFFF, 0.88)`
- Neutrals: page `#FBF8F3`, card `#FFFFFF` + `2px solid #EFE5D4` border,
  table-header `#F7F1E6`, row divider `#F3ECDD`, hover `#FAF5EA`,
  text `#3A2C22`, muted `#8B7A62`, faint `#B0A08A`,
  sidebar fg `#FFF6E8`, sidebar muted `#EFC9A8`,
  danger `#A33B34` on `#FDF4F3` border `#F3D9D6`
- Fonts: 'Source Serif 4' (headings), 'Public Sans' (body); radii 8–18px
- Derivation functions (design's own JS, adopt for BrandTheme):
  `shade(hex, amt)` = per-channel `c * (1 + amt)` clamped;
  `mix(a, b, t)` = linear per-channel interpolation
- On-gold buttons use `--brand-deep` text (dark-on-accent), not white —
  the contrast rule must allow a dark on-color for light accents
- The admin "Visit Logs" screen shows the Today / This Week segmented
  control (P3) and the sidebar/cards/tables the P2 restyle targets

Phase 1 consumes only the derivation rules + role model; the visual
restyle (P2) applies the neutrals/typography.

## Repo context (verified)

- Qt app: `qt-app/` (`theme.h` = `WitsTheme` namespace + `Color::*` constants
  incl. distinct `AdminPrimary`/`KioskPrimary`; `resources/wits.qss` =
  central stylesheet)
- PHP backend: `deliverables/loams_api/` (`db.php`, `auth_helper.php`,
  `get_visitors.php`, `admin_login.php`, ...)
- Exports already exist: PDF via QPrinter/QPdfWriter (adminwindow.cpp),
  Excel via vendored QXlsx (`qt-app/libs/QXlsx/`)
- Visitor filter already exists: `DateFilterType` enum
  `{All, SpecificDay, Month, DateRange}` in `visitordata.h`, driven by
  `visitorDateFilterBox` in `adminwindow.ui`, wired through
  `VisitorController::fetchVisitors`/`collectVisitorFilter`/`exportToExcel`
- Existing tests: `tst_theme.cpp`, `tst_responsive_ui.cpp`,
  `tst_visitorcontroller.cpp`, `tst_reportrenderer.cpp`

## Approach (additive, low-risk)

- **New theming layer** (`brandtheme.h/.cpp`) on top of `theme.h`; `theme.h`
  `Color::*` become the fallback palette; preserve Admin vs Kiosk color roles
  (map logo primary/secondary onto each, don't collapse).
- **Visitor toggle:** EXTEND `DateFilterType` with `Today` + `ThisWeek`
  (mirror `monthRange()` logic), add a segmented control above
  `visitorTable` ALONGSIDE `visitorDateFilterBox` (don't remove it), reuse
  the existing fetch/export path — no parallel logic, no backend change.
- **Branding persistence:** hybrid. New MySQL table (`branding_config`) +
  new PHP endpoint(s) in `deliverables/loams_api/` following existing
  auth/db pattern. QSettings = local cache. Startup: apply cache
  immediately → background-sync backend → re-apply if newer.
- **Branded exports:** PDF/Excel pull colors from the theme engine instead
  of hardcoded values; data/calculations unchanged.

## Hard constraints (must not break)

- No change to existing business logic, workflows, API/JSON contracts,
  existing DB schema, auth, attendance, validation, signal/slot
  connections, or widget object names.
- Existing test suite is the regression baseline. Only `tst_theme.cpp` may
  change (static → behavioral assertions); any other test change must be
  justified in writing.
- **OUT OF SCOPE v1:** manual theme editor UI, multiple/saved theme
  profiles, dark mode, theme animations, web/mobile clients +
  cross-platform sync, advanced designer (gradients/fonts/component
  styling), multi-tenant admin. Manual mode = code hooks only (a
  `theme_mode == Manual` branch that skips auto-regen), NO UI.
- Logo formats: SVG/PNG preferred, JPG/BMP supported, WEBP best-effort
  (may lack qwebp plugin → treat as unsupported, never crash), ICO
  optional. Reject corrupt/unsupported files with a clear error.

## Success conditions — prove each with a command/test/diff/artifact

1. [P1] Debug AND Release builds succeed (exit 0, no new warnings vs
   baseline). Proof: full build output for both.
2. [P1] Existing tests pass unmodified except `tst_theme.cpp`. Proof: full
   run output of tst_responsive_ui, tst_visitorcontroller,
   tst_reportrenderer + any others.
3. [P1] `tst_theme.cpp` rewritten to behavior-based assertions (no
   hardcoded hex) and passes. Proof: diff + passing output.
4. [P1] New theme-engine tests (e.g. `tst_brandtheme.cpp`) pass: valid
   palette from a logo; deterministic (same logo → same output); different
   logo → different palette; generated colors meet a defined min contrast
   ratio. Proof: test file + output.
5. [P1] `branding_config` table + new PHP endpoint(s) exist under
   `deliverables/loams_api/` following existing auth/db pattern. Proof:
   CREATE TABLE stmt + new PHP file(s).
6. [P1] Logo upload validation: valid SVG + valid PNG succeed, corrupted
   file rejected with a specific error, no crash. Proof: test asserting
   all three outcomes.
7. [P1] Startup is cache-first then background-sync-then-reapply-if-newer.
   Proof: code walkthrough/diff of the startup path.
8. [P1] Manual mode = code hook only: a `theme_mode == Manual` branch that
   skips auto-regen, no UI. Proof: source excerpt (code review only).
9. [P3] `DateFilterType` extended with Today/ThisWeek + correct date-range
   logic. Proof: unit test asserting exact start/end dates for a fixed
   current date + enum diff.
10. [P3] Segmented Today/This Week control in `adminwindow.ui`, above
    `visitorTable`, `visitorDateFilterBox` still present. Proof:
    adminwindow.ui diff.
11. [P3] New control reuses `collectVisitorFilter()`/fetch path — no
    duplicate logic. Proof: adminwindow.cpp diff.
12. [P3] Export helpers handle new filter values. Proof: unit test on
    `defaultExportFileName()`/`datePartForFilter()` with Today+ThisWeek
    asserting correct, non-crashing output.
13. [P2] Charts source colors from the theme engine; numeric data
    untouched. Proof: diff of chart color assignments + before/after
    export showing identical data, different colors.
14. [P2] PDF export inherits branding. Proof: generate PDF under two
    themes; extracted colors differ, table/content data identical.
15. [P2] Excel export inherits branding where QXlsx formatting allows.
    Proof: XLSX under two themes; cell data identical, fill/format colors
    differ.
16. [P1] No existing PHP endpoint contract changed. Proof:
    request/response diff per exercised endpoint (login, get_visitors,
    reports, registration, settings) = zero diffs except new branding
    endpoint.
17. [P1] No existing MySQL schema altered. Proof: SHOW CREATE TABLE per
    pre-existing table, before/after = no change; only new table added.
18. [P2] Import/export workflows behave identically. Proof: existing
    import/export test coverage and/or a sample CSV/XLSX import produces
    identical records.
19. [P2] UI stays responsive across modernized screens. Proof:
    tst_responsive_ui passes unmodified.
20. [P2] No stray hardcoded colors outside the central theme system.
    Proof: grep `#[0-9A-Fa-f]{6}` across .cpp/.ui outside theme.h,
    brandtheme.*, wits.qss; remaining hits enumerated + justified.
21. [HUMAN] Overall visual-quality/consistency judgment is subjective —
    NOT an automated pass/fail; requires human walkthrough after 1–20 are
    green.

## Seed data (must exist before verifying)

Admin + librarian accounts with valid creds; 3–5 logos with distinct
palettes (≥1 SVG, ≥1 PNG); visitor log with visits today, earlier this
week, and >1 week old (to prove exclusion); active + historical
attendance; 100+ students across departments/programs/years; sample CSV +
XLSX import files; populated school settings (hours, existing logo,
report prefs).

## Phasing

Work in phases: **P1** (theme engine + backend + tests + no-regression
proofs) → **P2** (branded exports/charts + UI restyle + color audit) →
**P3** (Visitor Log toggle). Stop and report at the end of each phase.

**Current directive: execute PHASE 1 only** (success conditions 1–8, 16,
17 above; P1-tagged items). Stop and report at end of Phase 1.
