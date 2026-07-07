# LOAMS 2.0 — Qt Quick Modernization Design Spec

Date: 2026-07-07
Status: Approved direction (brainstormed with owner); Phase 0 deliverable defined below
Supersedes: the Qt Widgets modernization effort (docs/LOAMS_UI_Modernization_PRD.md P2/P3
phases). The Phase 1 branding engine from that effort is **merged and retained** as core
infrastructure (master f47fbcc).

## 1. Goal

Transform LOAMS/WITS from a Qt Widgets desktop app into **LOAMS 2.0** — a modern Qt Quick
(QML) desktop application with the polish of contemporary desktop software (Linear, Notion,
Raycast class), while preserving the proven C++ business logic and the PHP/MySQL backend.
This is a redesign of the presentation layer and interaction model, not a 1:1 widget port.

## 2. Locked decisions (do not relitigate)

| Decision | Choice |
|---|---|
| 2.0 scope | Migrate **existing modules only** (kiosk, visitors, students, reports, settings, auth). Inventory / Borrow-Return / AI Assistant are OUT of 2.0 — the architecture must leave clean seams for them later. |
| Phase 1 branding engine | **Merged to master** (f47fbcc). brandtheme/brandingcontroller/brandcolormath + branding_config endpoints are reusable core infrastructure. Only its ~40 lines of Widgets wiring are throwaway. |
| App topology | **One executable, two surfaces** (kiosk + admin) with separate QML scene trees over shared C++ services. |
| Theme model | Design-system tokens as base; **logo-derived brand palette** (engine reused) overrides brand roles; **light AND dark** variants ship in 2.0. |
| Backend | **Targeted hardening**: PHP/MySQL stack and working contracts stay; real problems may be fixed (parameterized queries, auth tightening) behind contract tests; new endpoints additive. |
| Migration strategy | **A: Parallel rebuild on a shared core.** New Qt Quick app built alongside the Widgets app in the same repo/CMake; Widgets app remains the rollback until parity, then is deleted. |
| Business logic language | C++ (core library). QML is presentation, interaction, and state binding only. |
| Process | **Phase 0 = comprehensive modernization proposal document; implementation only after owner approval**, then phase-by-phase per roadmap. |

## 3. Sources of truth

- **Technical:** this repository. Key existing assets: extracted UI-free controllers
  (`settingscontroller`, `visitorcontroller`, `importcontroller`, `studentcontroller`,
  `reportcontroller` + stateless `reportrenderer`), the brand engine
  (`brandtheme.h/.cpp`, `brandcolormath.h`, `brandingcontroller`), `apiconfig.h`, RFID
  input plumbing (`rfidscandetector`, `rfidkeyboardfilter`), 12 green ctest targets,
  PHP endpoints under `deliverables/loams_api/`, schema under `deliverables/sql/`.
- **Visual:** the Claude Design references in
  `../Library System UI Modernization/` (`Admin Dashboard.dc.html`,
  `Library Kiosk v2.dc.html`, `Kiosk Style Options.dc.html`) — tokens, layout, motion,
  and component styling are normative; they are light-theme only (dark is derived, §6).

## 4. Architecture — shared core + Qt Quick shell

```
qt-app/
  core/            NEW: static library "witscore" — all business logic
                   (moved, not rewritten: the five controllers, reportrenderer,
                    brand engine, apiconfig, RFID classes, data structs)
  quick/           NEW: LOAMS 2.0 Qt Quick app
    main.cpp
    viewmodels/    QObject ViewModels (MVVM) — one per screen/module
    models/        QAbstractListModel wrappers (students, visitors, log rows)
    qml/
      AppShell.qml           window, surface switching, navigation host
      kiosk/                 login, live feed, guest
      admin/                 dashboard, database, reporting, search, logs, settings
      components/            design-system library (LButton, LCard, LTable,
                             LSegmented, LSideNav, LToast, LDialog, ...)
      theme/                 Theme singleton bound to the C++ brand engine
    tests/                   Qt Quick Test targets (tst_qml_*)
  tests/           existing 12 unit-test targets — permanent regression baseline
  (legacy Widgets sources keep building the WITS app until parity; deleted in Phase 6)
```

Rules:
- `witscore`'s business-logic core (controllers, brand engine, RFID state machine,
  `apiconfig`) has zero QML/Widgets dependencies. `reportrenderer` is the one
  exception: it links `Qt::Charts` + `Qt::Widgets` + `QXlsx` because `QChart`
  derives from `QGraphicsWidget` in Qt 6 — both apps accept this transitive
  dependency on the reports/export path during the parallel-rebuild window
  (see Risk #2).
- **ViewModels are the only QML-facing C++.** They own controllers, expose
  Q_PROPERTY/signals, and translate record lists into models. QML never calls
  controllers directly.
- A C++ `Navigator` singleton owns surface/page/modal state so flow is testable and
  keyboard shortcuts are centralized.
- The `RfidScanDetector` state machine (already Widgets-free) is reused verbatim.
  Its current install layer (`RfidKeyboardFilter`) is Widgets-coupled — installed
  on a `QApplication` and gated on `QApplication::activeWindow()`/`focusWidget()` —
  so that layer is **re-authored**, not reused as-is, against `QQuickWindow` focus
  semantics; the Phase 1 spike (Risk #1) is where this gets proven out.
- Future modules (inventory, borrow/return, AI) plug in as new
  viewmodel + qml/module + core-service triples; nothing in 2.0 may assume the
  admin sidebar is a closed set.

## 5. Design system

- Tokens from the Claude Design files: brand `#7E1A15`, accent `#E8B10E`, page
  `#FBF8F3`, card `#FFFFFF` with `#EFE5D4` borders, text `#3A2C22`, muted `#8B7A62`;
  radii 8–18 px; `Source Serif 4` headings + `Public Sans` body bundled as app fonts.
- Derivations use the shipped `brandcolormath.h` rules (`shade(-0.28)` deep/hover,
  `mix(white, 0.90)` soft), matching the design's own JS.
- A QML `Theme` singleton (backed by `ThemeViewModel`) is the single source of every
  visual property (`Theme.surface`, `Theme.brand`, `Theme.onBrand`,
  `Theme.motion.*`, spacing/radius/typography scales). The PRD's "no stray hardcoded
  colors" grep audit remains enforceable.
- Component library (`qml/components/`) implements the design's controls once;
  screens compose components and never restyle primitives locally.

## 6. Theming

- **Brand engine reuse:** `BrandTheme::current()` supplies brand roles; logo import in
  admin settings triggers `regenerateFromLogo` → live re-theme via property bindings
  (no restart). Persistence stays on `branding_config` (backend) + QSettings (cache),
  cache-first startup exactly as shipped.
- **Light + dark:** every token role carries light/dark values. Dark surfaces are
  derived (no design reference exists) and brand roles are re-contrast-checked against
  dark surfaces using the engine's WCAG machinery (`MinContrast = 3.0` for UI roles,
  4.5 for text). Theme mode (light/dark/system) is a local QSettings key.
- Manual theme mode remains a code hook (unchanged from Phase 1); no theme-editor UI
  in 2.0.

## 7. Surfaces

**Kiosk** (from `Library Kiosk v2.dc.html`): maroon gradient brand panel (logo, pulsing
scan affordance, gold LOG IN CTA, clock + library hours), live attendance feed with
row-entry motion, "now signed in" hero card, stat tiles (visitors today / this hour),
guest flow behind the existing settings toggle. RFID scan behaves exactly as today.

**Admin** (from `Admin Dashboard.dc.html`): persistent left sidebar (Dashboard,
Database, Reporting, Search, Visit Logs, Settings), page header with clock; dashboard
card grid (visitors today/week, registered students, peak hour, hourly bars, department
bars) fed by existing report endpoints; tables in the design's row style; segmented
controls — including the Today/This-Week visitor-log toggle inherited from the old P3
scope, which lands natively here.

**Motion:** 150–400 ms eased transitions from the design files (pageIn/rowIn/barGrow);
all durations via `Theme.motion.*` with a global reduce switch.

**Keyboard & accessibility:** full keyboard navigation on admin (sidebar cycling, table
focus, Ctrl+K quick search), `Accessible.*` roles on all interactive components,
contrast guaranteed by the engine's math.

## 8. Verification model

- The **12 existing unit-test targets stay green in every phase** (core move is
  `git mv` + CMake path updates, not a rewrite — each target keeps compiling its
  controller `.cpp` directly rather than linking a monolithic `witscore`, so its
  dependency surface stays as lean as today; e.g. `tst_reportcontroller` still
  omits `reportrenderer.cpp`).
- New layers: ViewModel unit tests (plain QtTest, synthetic payloads, no live network —
  house rule), Qt Quick Test for components/screens (offscreen), per-module parity
  checklists (legacy behaviors enumerated, checked off against the new screen).
- Backend hardening is **contract-tests-first**: capture current request/response
  fixtures per endpoint, then harden against them (zero contract drift).
- Per-phase gates: Debug+Release builds clean, full ctest, `/claude-review`, human
  visual walkthrough (premium look is a human judgment).

## 9. Roadmap

| Phase | Deliverable | Gate |
|---|---|---|
| **0** | **Comprehensive modernization proposal** (20 sections per owner brief: repo/debt/design/UX assessments, migration strategy, architecture, folder structure, ViewModels, component library, design/theme/navigation/animation systems, backend reuse+hardening plan, screen-by-screen redesigns, risks, roadmap). No implementation. | **Owner approval — hard stop** |
| 1 | `witscore` extraction (both apps build, 12/12 green); AppShell + Theme singleton (light/dark + brand engine) + core components; **RFID-in-QML spike** (detector state machine reused, filter/install layer re-authored against `QQuickWindow`) | build/tests/review |
| 2 | Kiosk surface parity (student/RFID/admin/guest login, feed, hero, stats) | parity checklist + gates |
| 3 | Admin part 1: dashboard, visit logs (Today/This-Week), search | parity checklist + gates |
| 4 | Admin part 2: database + import, reports (reportrenderer reuse), settings incl. live logo re-theming | parity checklist + gates |
| 5 | Dark-mode polish, a11y pass | full gates + human walkthrough |
| 6 | Backend targeted hardening (contract fixtures across all ~30 `deliverables/loams_api/` endpoints, then parameterize/harden; remove debug-only endpoints — `phpinfo.php`, `testupload.php` — and resolve `hash_admin.php`'s status; revisit the hardcoded `http://localhost` base URL in `apiconfig.h` now that auth endpoints are in scope); delete Widgets app; final whole-branch review | full gates + human walkthrough |

## 10. Risks

1. **RFID under QML focus** — event delivery differs from Widgets; de-risked by the
   Phase 1 spike before anything depends on it.
2. **Charts** — QtCharts QML styling fights the design; build the dashboard's simple
   bars as custom QML, keep QtCharts only inside `reportrenderer` exports (accepted
   trade-off: this keeps a `Qt::Widgets` link on the reports/export path even in the
   Qt Quick app, since `QChart` derives from `QGraphicsWidget`).
3. **Dark mode has no design reference** — derived via contrast math; budget a human
   review round.
4. **Deployment hardware performance** — validate Qt Quick rendering on the actual
   library PC in Phase 1; document software-rendering fallback.
5. **PHP hardening regressions** — contract fixtures before touching any endpoint.
6. **Two apps in one repo** — `-DBUILD_LEGACY_WIDGETS=ON` (default) keeps the rollback
   alive until Phase 6 removes it.

## 11. Out of scope for 2.0

Inventory, Borrow/Return, AI Assistant, multi-tenant admin, web/mobile clients,
theme-editor UI, backend redesign beyond targeted hardening.
