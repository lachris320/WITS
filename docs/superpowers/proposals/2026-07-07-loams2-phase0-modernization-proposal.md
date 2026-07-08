# LOAMS 2.0 — Modernization Proposal (Phase 0)

**Date:** 2026-07-07
**Status:** Complete — awaiting owner review
**Supersedes / builds on:** `docs/superpowers/specs/2026-07-07-loams2-qtquick-design.md` (the locked-decision spec)

## How to read this document

The decisions in the spec's §2 ("Locked decisions — do not relitigate") are
settled: 2.0 scope (existing modules only), the retained Phase 1 branding
engine, the one-executable/two-surfaces topology, the design-system + light/dark
theme model, targeted backend hardening, migration Strategy A (parallel
rebuild on a shared core), C++-resident business logic, and the Phase 0 →
owner-approval → phased-implementation process. This document does not
re-argue any of them — it elaborates *within* them: diagnosing the current
repository in detail, then specifying the target architecture, UI systems,
backend plan, and rollout at implementation depth. Where evidence from the
codebase creates tension with a locked decision, that tension is called out
explicitly in a blockquote starting with **"⚠ Decision tension"** rather than
silently overridden or silently ignored.

Every factual claim about the current codebase in this document cites a real
`file:line` (or a filename, for whole-file claims) so a reviewer can verify it
directly against the repository.

---

## 1. Executive summary

**What LOAMS 2.0 is.** LOAMS 2.0 is a full modernization of the *presentation
layer* of WITS — the library monitoring system — migrating its user interface
from Qt Widgets to Qt Quick (QML) while keeping the existing PHP/MySQL backend
and the existing C++ business logic. It is a client rebuild, not a system
rewrite: the same two surfaces the app has today (a walk-up kiosk and an admin
console) are rebuilt against the maroon/gold design system in the Claude Design
references (Section 4), with a proper MVVM architecture, a live-brandable theme,
light/dark support, and the motion and accessibility the current Widgets UI
lacks. The scope is **the modules that exist today only** — Inventory,
Borrow/Return, and an AI Assistant are explicitly out of 2.0 and are treated as
forward-seams the architecture must leave room for, never build-now work
(Sections 5.10, 9).

**The locked decisions this document builds within (not relitigated).** Per the
design spec's §2: 2.0 migrates existing modules only; the Phase 1 branding
engine is retained and reused unchanged; the app stays one executable presenting
two surfaces; a design-token system with a single QML `Theme` singleton and a
derived dark mode; targeted (not wholesale) backend hardening; migration
**Strategy A** — a parallel rebuild on a shared core; business logic stays
resident in C++; and a Phase 0 → owner-approval → phased-implementation process.
Where codebase evidence creates tension with a locked decision, this document
flags it in a **⚠ Decision tension** callout rather than overriding it silently
(e.g. the zero-config default palette, raised in Section 4.3 and resolved in
Section 12.4).

**The strategy — Strategy A, parallel rebuild on a shared core.** The new Qt
Quick UI is built *beside* the existing Widgets app, both linking one shared
`witscore` C++ static library, until the Quick app reaches feature parity — at
which point the Widgets app is deleted (Section 6). The two rejected
alternatives (an in-place re-skin, and a big-bang rewrite) are documented as
already-decided in Section 6.1. What makes Strategy A viable is the central
finding of the repository assessment: the business logic is *already*
substantially UI-free and reusable (Section 2.8), so the core moves by `git mv`,
not rewrite, and the 12 existing ctest targets stay green through every phase
(Section 8).

**The four assessments, one line each.**

- **Repository architecture (Section 2):** the app is one flat CMake target with
  no library boundary, but its five domain controllers, the report renderer, the
  brand engine, and the RFID state machine are already Widgets-free and
  unit-tested — well-positioned for a `git mv` into `witscore`; the net-new work
  is the missing ViewModel seam between that logic and QML.
- **Technical debt (Section 3):** the debt that actually impedes the migration
  is small and cited — the flat CMake target (High) and the UI/logic coupling
  still inside the window classes (High); the rest (ad hoc `QSettings`,
  duplicated department fetches, a plaintext base URL, the three-way theming
  split) is Medium/Low and is absorbed by the migration rather than needing a
  pre-emptive fix.
- **Design (Section 4):** the gap between today's blue/green Widgets UI and the
  maroon/gold design references is structural, not merely chromatic — a grep for
  the design's tokens against `qt-app/` returns zero hits, native `QGroupBox`
  chrome has no equivalent in the design's card vocabulary, and nearly all the
  motion is net-new — which is why an in-place re-skin cannot meet the goal.
- **Product UX (Section 5):** each existing screen is walked and its friction
  named (the dual-purpose login field, the hard 9-row live feed, five/seven
  modal validation dialogs, the mandatory-department report gate, no
  quick-search); two design-target screens — the Admin dashboard and a student
  Visit Logs screen — do not exist yet and are called out as net-new rather than
  described as if shipped.

**The roadmap at a glance.** Phase 0 (this document) → Phase 1 (extract
`witscore`, stand up `AppShell`/`Theme`/the `L*` component library, run the
RFID-in-QML spike and the deployment-hardware render check) → Phase 2 (kiosk
surface parity) → Phase 3 (admin dashboard, visit logs, search) → Phase 4
(database/import, reports, settings) → Phase 5 (dark-mode + accessibility polish)
→ Phase 6 (targeted backend hardening, delete the Widgets app, final review).
Every phase after 0 clears the same four-check gate — clean Debug+Release builds,
full ctest green, `/claude-review`, and a human visual walkthrough — and the
Widgets app survives as the rollback until Phase 6 (Section 20). **Phase 0's gate
is different and absolute: owner approval. No implementation begins until this
proposal is approved — the Phase 0 hard stop.**

**Top three risks (Section 19).**

1. **RFID under QML focus (R1) — the headline risk.** The kiosk's RFID capture
   is an application-wide event filter gating on `QApplication`/`QWidget` focus
   APIs that have no `QQuickWindow` equivalent; the pure detector state machine
   is reused verbatim, but the install/focus layer is re-authored. Mitigated by a
   Phase 1 spike *before* any kiosk screen depends on it, and verified in Phase 2
   by a parity checklist that treats "RFID behaves exactly as today" as
   observable behavior — the single most important owner-check in the plan.
2. **Deployment-hardware rendering (R4).** Qt Quick's GPU-oriented scene graph
   may underperform the current Widgets app on the actual (possibly older)
   library PC; mitigated by validating rendering on the real machine in Phase 1,
   with a documented software-rendering fallback.
3. **PHP hardening regressions (R5).** The Phase-6 backend hardening could drift
   a request/response contract the frozen client depends on; mitigated by a
   contract-tests-first method (golden fixtures, synthetic data) that rejects any
   change altering a JSON shape, and confined to Phase 6 *after* the client
   migration.

**What we are asking you to approve.** Approval of **this Phase 0 proposal** —
the strategy (Strategy A), the target architecture (`witscore` + MVVM + QML), the
design-system / theme / navigation / animation specifications, the backend
reuse-and-hardening plan, the screen-by-screen redesigns, and the seven-phase
roadmap with its per-phase gates. This is a decision to proceed into Phase 1, not
to ship any code today: **Phase 0 produces this document and nothing else, and no
implementation begins until you approve it.** The specific judgment calls that
need your explicit acknowledgement at sign-off are itemized in Section 19 —
chiefly that the unbranded default look becomes maroon/gold (N1, Section 12.4)
and that the Visit Logs screen initially shows guest sign-ins until an additive
student-attendance endpoint lands (N5, Section 18.8).

## 2. Repository architecture assessment

### 2.1 Overview

WITS is a single-target Qt 6.11 / C++17 Widgets desktop application, built
with CMake+Ninja (MinGW). The build produces one executable, `WITS`, from a
flat, unstructured source list — there is no library split between business
logic and UI (`qt-app/CMakeLists.txt:31-53`, the `qt_add_executable(WITS
MANUAL_FINALIZATION ...)` call listing all ~40 top-level `.cpp`/`.h`/`.ui`
sources as one target). `qt-app/CMakeLists.txt:12-13` links the app against
`Widgets`, `Network`, `Charts`, `Test`, `UiTools`, `PrintSupport`, and `Svg` —
i.e. the executable's own build inseparably mixes a GUI toolkit dependency
into what is, for roughly half the source tree, pure business logic that
never touches a widget. `qt-app/CMakeLists.txt:63,71` also links the vendored
`QXlsx` library (`qt-app/libs/QXlsx/`, out of review scope per project
instructions) directly into `WITS`.

The runtime app has three top-level windows: `MainWindow` (the kiosk/login
surface, `qt-app/mainwindow.h`/`.cpp`, backed by `mainwindow.ui`), `adminWindow`
(the admin surface, `qt-app/adminwindow.h`/`.cpp`, backed by `adminwindow.ui`),
and `GuestWindow` (a modal dialog, `qt-app/guestwindow.h`/`.cpp`, backed by
`guestwindow.ui`). A fourth `.ui` file, `attachFilesDialog.ui`, backs a
small `AttachFilesDialog` (`qt-app/attachfilesdialog.h`/`.cpp`) used from the
admin database screen. `MainWindow` owns an `adminWindow*` member
(`mainwindow.h:42`) constructed in its own constructor (`mainwindow.cpp:52`),
so admin is not a separately launched process — it is a second `QMainWindow`
instantiated inside the kiosk process and shown on successful admin login
(`mainwindow.cpp:226`, `adminWin->show();`). This one-process, two-window
shape is exactly the topology the spec's §4/§7 shared-core design carries
forward as "one executable, two surfaces."

### 2.2 The controller layer — already UI-free

Five `QObject`-derived controllers exist, all constructed as children of
`adminWindow` with an injected, caller-owned `QNetworkAccessManager*` (the
consistent pattern across the app, e.g. `visitorcontroller.h:16`,
`studentcontroller.h:17`, `reportcontroller.h:20`,
`brandingcontroller.h:18`; the comment "injected, not owned — adminWindow
keeps ownership" appears verbatim in four of the five headers):

| Controller | Header | Constructed at | Owns |
|---|---|---|---|
| `SettingsController` | `qt-app/settingscontroller.h` | `adminwindow.cpp:132` (member-init list) | school/admin/library `QSettings` read+write, logo/poster import |
| `VisitorController` | `qt-app/visitorcontroller.h` | `adminwindow.cpp:148` | visitor fetch/filter, `QXlsx` export |
| `ImportController` | `qt-app/importcontroller.h` | `adminwindow.cpp:153` | CSV/Excel parsing, duplicate check, ZIP upload |
| `StudentController` | `qt-app/studentcontroller.h` | `adminwindow.cpp:166` | student search/bulk-update/delete, department/course lists |
| `ReportController` | `qt-app/reportcontroller.h` | `adminwindow.cpp:184` | report filter parsing, date-range math, department/year/course lists |

A sixth controller, `BrandingController` (`qt-app/brandingcontroller.h`), is
constructed in `MainWindow` rather than `adminWindow`
(`mainwindow.cpp:163`), because branding sync happens at kiosk startup, not
inside the admin surface.

All five `adminWindow`-owned controllers plus `BrandingController` and the
free-function brand engine (`qt-app/brandtheme.h`, backed by
`qt-app/brandtheme.cpp`) are genuinely free of Widgets/QMainWindow
dependencies at the `.cpp` level — a repository-wide search for
`QWidget`/`QMainWindow`/generated `ui_*.h` includes across
`visitorcontroller.cpp`, `importcontroller.cpp`, `studentcontroller.cpp`,
`reportcontroller.cpp`, `settingscontroller.cpp`, and `brandingcontroller.cpp`
returns zero hits. `ReportController` additionally exposes pure, static,
already-unit-tested logic that has been extracted out of `adminWindow`
entirely: `ReportController::getPalette` (`qt-app/reportcontroller.cpp:21-51`,
the four-branch palette table used by chart/report rendering) and
`ReportController::computeDateRange` (`qt-app/reportcontroller.h:34-39`, the
day/month/semester/custom date-range math). `adminWindow` calls the latter
rather than reimplementing it — `adminwindow.cpp:1532,1554,1572,1582` each
call `ReportController::computeDateRange(...)` from inside
`adminWindow::collectReportFilters` (`adminwindow.cpp:1498-1605`), and
`adminwindow.cpp:1650,1687,1719,1763` each call
`ReportController::getPalette(...)` rather than a local palette table. A
companion stateless renderer, `ReportRenderer` (`qt-app/reportrenderer.h`),
holds every chart/PDF/Excel-painting method as a `static` function of its
arguments — "No QSettings, no ui->, no member state" per its own header
comment (`qt-app/reportrenderer.h:17-20`) — and is held by `adminWindow` as a
plain value member (`adminwindow.h:102`, `ReportRenderer m_reportRenderer;`),
not a `QObject` at all.

This means the extraction work the spec's §4 "shared core" design assumes
already happened for reports specifically, and exists in a more general form
(controller-per-domain, injected `QNetworkAccessManager`, async signals, pure
static parsers) for visitors, import, students, and settings. The five
controllers plus `ReportRenderer` are precisely the six units the spec names
as reusable core technical assets (spec §3, "Key existing assets").

### 2.3 The brand engine — a seventh, free-function layer

The Phase 1 branding engine sits alongside the controllers as a distinct,
already-merged layer: `qt-app/brandcolormath.h` (header-only WCAG contrast
math — `relativeLuminance`, `contrastRatio`, `shade`, `mix`), `qt-app/
brandthemedata.h` (the `BrandPalette`/`BrandingConfig`/`ThemeMode` value
types), and `qt-app/brandtheme.h`/`.cpp` (the `BrandTheme` namespace:
`fallbackPalette()`, deterministic logo-based `extractPalette()`, JSON
round-trip, a `QSettings`-backed cache, `isNewer()` freshness comparison, and
the `regenerateFromLogo()` Manual-mode hook). `BrandTheme::current()`/
`setCurrent()` (`brandtheme.h:67-68`) hold a single process-wide palette via
a function-local static, read by the app wherever brand colors are needed.
The engine is deliberately not a `QObject` and has no QML/Widgets
dependency of its own — `BrandingController` (network-facing, `QObject`based)
is the only piece of the branding stack that talks to the backend, mirroring
the `VisitorController` pattern exactly (`brandingcontroller.h:11-13`, "Thin
network controller ... following the VisitorController pattern"). The
integration into the running app is additive, small, and already wired at
both ends of the parallel-rebuild's shared-core boundary: `mainwindow.cpp:
159-172` applies the cached palette synchronously at startup before any
network I/O, then re-applies it if the backend later reports a newer
config (this cache-first / background-sync / reapply-if-newer flow is the
exact mechanism the spec §6 requires "as shipped"); `adminwindow.cpp:1174-
1187` regenerates the palette when a new logo is imported, gated on
`ThemeMode::Auto` (the Manual-mode hook is a `return false` with a cleared
error message when `config.mode == ThemeMode::Manual` — see `brandtheme.cpp`
inside `regenerateFromLogo`, not reproduced here since Section 13 covers
theme mechanics in depth).

### 2.4 What is still coupled to the window classes

Despite the controller extraction, substantial logic remains embedded
directly inside `adminWindow`/`MainWindow` rather than in a controller or a
pure function:

- **`adminWindow::collectReportFilters`** (`adminwindow.cpp:1498-1605`) reads
  eight different widgets directly (`ui->filterDepartmentBox`,
  `ui->filterCourseBox`, `ui->paletteComboBox`, `ui->durationTypeBox`,
  `ui->dateEdit`, `ui->monthComboBox`/`ui->yearComboBox`,
  `ui->semesterComboBox`/`ui->semYearComboBox`, `ui->startDateEdit`/
  `ui->endDateEdit`), calls `QMessageBox::warning` inline seven times for
  validation failures, and only delegates the pure date-range arithmetic
  (not the widget reads, not the validation-failure UI) to
  `ReportController::computeDateRange`. The function is a textbook
  View+Controller mixture: it is *half* extracted.
- **`adminWindow::collectHeaderInfo`** (`adminwindow.cpp:1401-1412`)
  constructs a fresh `QSettings settings("MyCompany", "MyApp")` and reads
  seven keys directly, rather than calling `SettingsController::load()`
  (`settingscontroller.h:12`), which already knows how to read most of the
  same `school/`/`admin/`/`library/` keyspace (`settingscontroller.cpp:17-
  27`). The two code paths read overlapping keys through two different
  routes.
- **`adminWindow::loadDepartments`** (`adminwindow.cpp:1365-1399`) is a
  second, hand-rolled `QNetworkAccessManager::get` + inline JSON parse
  against `get_departments.php`, functionally overlapping with
  `ReportController::loadDepartments()`/`parseDepartments()`
  (`reportcontroller.h:24,42`) and `StudentController::loadDepartments()`
  (`studentcontroller.h:44`) — three independent department-list fetchers
  exist for what is one backend concept.
- **RFID input handling** is Widgets-coupled at its outermost layer only.
  `RfidScanDetector` (`qt-app/rfidscandetector.h`/`.cpp`) is a pure state
  machine — no Qt GUI includes, `feedKey(QChar, qint64) ->
  std::optional<QString>` (`rfidscandetector.h:20`) — genuinely reusable
  as-is under the spec's plan. Its installation layer, however,
  `RfidKeyboardFilter`, is deeply Widgets-specific:
  `mainwindow.cpp:95` installs it application-wide
  (`qApp->installEventFilter(rfidFilter)`), and
  `rfidkeyboardfilter.cpp:25` gates every key event on
  `QApplication::activeWindow() != m_loginWindow`, with a second gate at
  `rfidkeyboardfilter.cpp:33-36` against `QApplication::focusWidget()` to
  avoid double-counting a propagated key event. Both gates are
  `QApplication`/`QWidget` APIs with no `QQuickWindow` equivalent — this is
  precisely why the spec (§4, "Rules") calls this layer out for
  re-authoring rather than reuse, and why the Phase 1 roadmap entry
  dedicates a spike to it (spec §9, Phase 1; spec §10, Risk 1).
- **`MainWindow::refreshRightPanel`** (`mainwindow.cpp:366-403`) hand-builds
  five parallel `QList<QLabel*>` (name/course/year/department/time, 9 slots
  each = 45 named `.ui` widgets, e.g. `ui->nameLabel`, `ui->nameLabel_2` …
  `ui->nameLabel_13`) and indexes into them by position to render the
  "recent logins" feed — a UI-only concern, but one that hard-codes a fixed
  9-slot feed into named widget instances rather than a model/view list.
  This is a live example of exactly the case the spec's `Navigator`/
  ViewModel/`QAbstractListModel` design (§4) is meant to replace with a
  `VisitLogsViewModel`-style list model.

None of this is a defect relative to the app's stated goals to date — the
Widgets app was never meant to be MVVM — but it is the concrete inventory of
"still coupled" work the 2.0 migration must externalize into ViewModels
(Section 10) before the corresponding QML screen can be built.

### 2.5 Sidebar navigation is a fixed `QStackedWidget` switch

The admin surface's page navigation is a hand-wired
`QPushButton::clicked` → `ui->stackedWidget->setCurrentWidget(...)` +
`setActiveSidebar(...)` connection per sidebar button
(`adminwindow.cpp:292-311`, one `connect(...)` block per of the five
sidebar buttons: `generalBtn`, `databaseBtn`, `reportingBtn`,
`studentSearchBtn`, `visitorBtn`). There is no navigation abstraction —
adding a sixth admin page today means adding a sixth hand-wired
`connect(...)` block and a sixth `QStackedWidget` page in Designer. This is
the concrete "closed set" the spec's forward-compatibility requirement
(§4, "nothing in 2.0 may assume the admin sidebar is a closed set") is
written against — Section 9 addresses the target module-organization fix.

### 2.6 The theming layers (three, currently)

Three independent theming mechanisms currently coexist:

1. **`theme.h`** (`qt-app/theme.h`) — a header-only `WitsTheme` namespace:
   `lightPalette()` (a fixed, forced-light `QPalette`, `theme.h:29-61`, with
   an explanatory comment about Windows dark-mode text-legibility bugs) and
   the `Color::*` named-hex constants (`theme.h:65-79`, e.g.
   `AdminPrimary = "#2563EB"`, `KioskPrimary = "#10B981"`) plus
   `loadStyleSheet()` (`theme.h:83-90`).
2. **`resources/wits.qss`** (282 lines) — the central Qt stylesheet, loaded
   via `WitsTheme::loadStyleSheet()`, with its own repeated hex literals
   (e.g. `#1E293B` at `resources/wits.qss:7`, `#F1F5F9` at `:11`, `#E2E8F0`
   at `:17`) that
   happen to match `theme.h`'s constants today but are not generated from
   them — they are two independently hand-maintained copies of the same
   palette.
3. **The brand engine** (`brandtheme.h`/`.cpp`, Section 2.3) — layered
   *on top of* the other two: `BrandTheme::fallbackPalette()`
   (`brandtheme.cpp`) is defined to reproduce `WitsTheme::Color::*`
   verbatim so that an unbranded install looks identical to today, and the
   engine's brand roles (`adminPrimary`, `kioskPrimary`, etc.) only
   override `theme.h`'s roles when a logo has been imported.

The three-way split is intentional and additive by design (the brand engine
was built specifically not to touch `theme.h`/`wits.qss`), but it means
today's "theme" is really three sources of truth layered by convention, not
one. Section 13 specifies how the spec's single `Theme` QML singleton
collapses this into one property surface without changing the brand
engine's own contract.

### 2.7 Test baseline

`qt-app/tests/CMakeLists.txt` currently registers 12 ctest targets. Eleven
of them directly compile the class-under-test's `.cpp` alongside the test's
own `.cpp` as a same-target source list — no linking against `WITS` itself
and no shared test-support library:

| Target | `tests/CMakeLists.txt` lines | Extra sources compiled |
|---|---|---|
| `tst_rfidscandetector` | 1-12 | `rfidscandetector.cpp` |
| `tst_rfidkeyboardfilter` | 14-27 | `rfidkeyboardfilter.cpp` + `rfidscandetector.cpp` |
| `tst_apiconfig` | 29-38 | none (header-only `apiconfig.h`) |
| `tst_theme` | 40-50 | none (header-only `theme.h`; QSS path injected via `WITS_QSS_PATH`) |
| `tst_responsive_ui` | 52-66 | none (loads `.ui` files at runtime via `UiTools`, `WITS_UI_DIR`) |
| `tst_visitorcontroller` | 68-84 | `visitorcontroller.cpp` (+ `QXlsx` link) |
| `tst_importcontroller` | 86-102 | `importcontroller.cpp` (+ `QXlsx` link) |
| `tst_studentcontroller` | 104-119 | `studentcontroller.cpp` |
| `tst_reportcontroller` | 121-140 | `reportcontroller.cpp` (Gui, no Widgets/Charts) |
| `tst_reportrenderer` | 142-162 | `reportrenderer.cpp` (Gui+Charts+Widgets+QXlsx) |
| `tst_brandtheme` | 164-184 | `brandtheme.cpp` (Gui+Svg) |
| `tst_brandingcontroller` | 186-208 | `brandingcontroller.cpp` + `brandtheme.cpp` (Network+Gui+Svg) |

Two targets are genuinely dependency-free of the class under test's own
`.cpp`: `tst_apiconfig` (because `ApiConfig::endpoint`/`baseUrl` are
`inline` functions in the header, `apiconfig.h:18,27`) and `tst_theme` /
`tst_responsive_ui` (both compile no extra business-logic `.cpp` at all —
`tst_theme` exercises the header-only palette/constants directly, and
`tst_responsive_ui` loads the real `.ui` files at runtime rather than
compiling generated `ui_*.h` code). Notably, `tst_reportcontroller` links
only `reportcontroller.cpp`, not `reportrenderer.cpp`
(`tests/CMakeLists.txt:124-129`) — the two units the spec's §8 verification
model calls out as staying separately-testable
("`tst_reportcontroller` still omits `reportrenderer.cpp`") already do so
today, which is direct evidence the current test architecture is compatible
with the target `core/` library split without a rewrite.

There is no test target yet for `SettingsController` — it is the one
`adminWindow`-owned controller without a corresponding `tst_*` file, a gap
noted here for completeness (not a 2.0-scope blocker, since Section 3
addresses gaps, not this section's job of describing what exists).

### 2.8 What this means for MVVM reuse

**Well-positioned today:** the five domain controllers, `ReportRenderer`,
the brand engine, `RfidScanDetector`, `apiconfig.h`, and `theme.h`'s
constants are all Widgets-free C++ that a `witscore` library (Section 7)
can absorb by `git mv`, not rewrite — this is the literal claim the spec §8
verification model makes ("core move is `git mv` + CMake path updates, not
a rewrite") and the CMake/test evidence above supports it directly: every
one of these files already compiles standalone into a test target with no
`QMainWindow`/`ui_*.h` dependency.

**Not yet positioned:** the widget-reading, validation-messaging, and
navigation logic enumerated in Section 2.4-2.5 lives inside `adminWindow`/
`MainWindow` method bodies with no separating interface — there is currently
no `ReportsViewModel`-shaped seam between "read the widgets" and "call
`computeDateRange`." Building that seam (a ViewModel wrapping each
controller, replacing direct `ui->` reads with `Q_PROPERTY` bindings) is
the concrete, sizeable piece of net-new work the migration requires; it is
specified in Section 10.

## 3. Technical debt assessment

Each item is cited, rated **High/Med/Low** for how much it impedes the 2.0
migration specifically (not a general code-quality score), and marked
**[Phase 6]** where the spec already schedules its remediation there.

### 3.1 Flat CMake target blocks a shared core — **High**

`qt-app/CMakeLists.txt:31-53` builds one `qt_add_executable(WITS ...)` with
every source — business logic and UI — in one flat list, and
`qt-app/CMakeLists.txt:65-72` links `Widgets`/`Network`/`Charts`/
`PrintSupport`/`Svg`/`QXlsx` all into that one target. There is no
`witscore` (or equivalent) library boundary today, so nothing currently
guarantees or even checks that the business-logic files stay
Widgets-free — Section 2.2/2.7's "already UI-free" finding holds *by
convention*, not by build enforcement. This is the direct blocker to the
spec §4 architecture and is Section 7/8's central subject; not Phase-6
deferred — it is Phase 1 work per the spec's roadmap (spec §9, Phase 1:
"`witscore` extraction").

### 3.2 UI/logic coupling remaining in the window classes — **High**

Detailed with citations in Section 2.4: `collectReportFilters`
(`adminwindow.cpp:1498-1605`), `collectHeaderInfo`
(`adminwindow.cpp:1401-1412`), the duplicate `loadDepartments`
(`adminwindow.cpp:1365-1399`), `RfidKeyboardFilter`'s `QApplication`
coupling (`rfidkeyboardfilter.cpp:25,33-36`), and `refreshRightPanel`'s
45-widget fixed feed (`mainwindow.cpp:366-403`). High impact because every
QML screen needs a ViewModel seam where today there is a widget-reading
method body; this is the bulk of the "elaborate the target architecture"
work in Sections 9-10.

### 3.3 `QSettings("MyCompany","MyApp")` constructed ad hoc at 7 call sites — **Med**

The literal pair `QLatin1String("MyCompany")`/`QLatin1String("MyApp")` (or
its plain-string-literal form) is repeated at: `mainwindow.cpp:99`,
`mainwindow.cpp:160`, `mainwindow.cpp:166`, `mainwindow.cpp:408`,
`adminwindow.cpp:1178`, `adminwindow.cpp:1402`, plus the two centralized
uses inside `SettingsController::load()`/`save()`
(`settingscontroller.cpp:15,33`) — 8 total construction sites, only 2 of
which go through the one controller that owns the `school/`/`admin/`/
`library/` keyspace. `QSettings` here is used partly as a genuine local
cache (the branding config: `mainwindow.cpp:160-161`,
`brandtheme.cpp`'s `saveCachedConfig`/`loadCachedConfig`, which is the
correct, intentional use per spec §6) and partly reasoned about as if it
were the source of truth for school/admin/library settings, when
`SettingsController`/`SettingsData` (`qt-app/settingsdata.h`) is meant to
be that source. No factory function centralizes the
`QSettings(company, app)` construction itself, so a future rename of the
org/app strings would require touching 8 call sites by hand. Medium impact:
doesn't block the architecture, but each of these 6 non-controller call
sites is a place a future ViewModel will need to either call through
`SettingsController` instead, or the controller's surface needs to grow to
cover what `collectHeaderInfo` reads today.

### 3.4 Hardcoded plaintext base URL in `apiconfig.h` — **Med**

`qt-app/apiconfig.h:20` hardcodes `return
QStringLiteral("http://localhost/loams_api/");` as the single source of
truth for the backend base URL — plaintext HTTP, no TLS, and a
compile-time constant rather than a configurable value (no environment
variable, no config file, no `QSettings` override). This is a reasonable
default for a same-machine XAMPP dev/deployment topology (which the
project's own `security-hygiene.md` rule treats as illustrative rather
than a live secret), but it means every deployment is either literally
`localhost` or requires editing and rebuilding this header. The spec's
Phase 6 backend-hardening entry explicitly plans to "revisit the hardcoded
`http://localhost` base URL in `apiconfig.h` now that auth endpoints are in
scope" (spec §9, Phase 6) — **[Phase 6]**. Not urgent for 2.0's UI
migration itself (Section 16/17 covers backend reuse in depth), but worth
tracking here as a debt item discovered during the code assessment.

### 3.5 Leftover debug/diagnostic PHP endpoints — **Med**

Two files under `deliverables/loams_api/` are debug/diagnostic utilities,
not application endpoints: `phpinfo.php` (`deliverables/loams_api/
phpinfo.php:1-3`, a 3-line file whose entire body is `<?php phpinfo(); ?>`
— a classic information-disclosure endpoint that dumps the full PHP
runtime configuration, including loaded extensions and paths, to anyone
who requests it) and `testupload.php` (`deliverables/loams_api/
testupload.php:1-24`, echoes `upload_max_filesize`/`post_max_size`/
`max_file_uploads`/`memory_limit` ini values and creates/checks
writability of an `uploads/` directory — a diagnostic script, not part of
any real upload flow). Neither is referenced by the Qt client (no
`ApiConfig::endpoint("phpinfo.php")` or similar call exists in
`qt-app/`). A third file the spec anticipated checking,
`hash_admin.php`, **does not exist anywhere in this repository** — a
directory listing of `deliverables/loams_api/` (29 `.php` files total)
confirms no file by that name is present, so there is nothing to resolve;
this note itself is the resolution the spec's Phase 6 line item asked for.
The spec's Phase 6 roadmap entry already schedules removing the two debug
endpoints — **[Phase 6]** — "remove debug-only endpoints —
`phpinfo.php`, `testupload.php`" (spec §9, Phase 6).

### 3.6 Duplicated department-list fetching across three call sites — **Med**

Three independent code paths fetch the department list from
`get_departments.php`: `adminWindow::loadDepartments`
(`adminwindow.cpp:1365-1399`, hand-rolled `QNetworkAccessManager::get` +
inline parse), `ReportController::loadDepartments()`/`parseDepartments()`
(declared `reportcontroller.h:24,42`), and
`StudentController::loadDepartments()` (`studentcontroller.h:44`,
`StudentController::parseDepartments` also declared at
`studentcontroller.h:26`). All three ultimately hit the same backend
endpoint and expect the same `{"status":"success","departments":[...]}`
shape, but each has its own combo-box-population glue and (per
`reportcontroller.cpp`'s own comment on `parseDepartments`) at least one
subtle behavioral difference already exists between the report and
student variants regarding whether an "all" sentinel is filtered. This is
exactly the kind of duplication a single `StudentsViewModel`/shared
service call would eliminate; flagged here as a concrete case Section 9's
module organization should account for, not a bug to fix in Phase 0.

### 3.7 Three-way theming split (`theme.h` / `wits.qss` / brand engine) — **Low**

Detailed in Section 2.6. Rated Low impact specifically *for the 2.0
migration* (not as a general code-quality matter) because the QML `Theme`
singleton (Section 13) replaces all three with one property surface
regardless of how they're organized today — the migration does not need to
reconcile `theme.h` and `wits.qss` first, it needs to build the new
singleton once and retire the old three-way split at the point each QML
screen ships. No remediation is required before Phase 1 begins.

### 3.8 `QSettings` used as a local cache with only lightweight freshness logic — **Low**

The branding cache's freshness check (`BrandTheme::isNewer`, referenced
from `mainwindow.cpp:167`) is a simple `updatedAt` timestamp comparison
with no conflict resolution beyond "remote wins if strictly newer" — fine
for a single-admin, single-device branding config (today's actual use
case) but worth naming as an assumption baked into the cache design, should
a future multi-admin scenario ever apply (multi-tenant admin is explicitly
out of scope per spec §11, so this is documented, not actioned).

### 3.9 Chart rendering is transitively Widgets-coupled via `QChart` — **Low, and accepted by the spec**

`adminwindow.h:26-34` includes the full `QtCharts` header set
(`QChart`, `QChartView`, `QBarSet`, `QBarSeries`, `QBarCategoryAxis`,
`QPieSeries`, `QPieSlice`, `QLineSeries`, `QValueAxis`), and
`ReportRenderer`'s chart-image makers
(`reportrenderer.h:29-35`) depend on `QChart`, which in Qt 6 derives from
`QGraphicsWidget` — meaning `reportrenderer.cpp` (and by extension anything
that links it, including the `tst_reportrenderer` test target's
`Qt::Widgets` link at `tests/CMakeLists.txt:156`) carries a `Qt::Widgets`
dependency even though it paints into a plain `QImage` and never shows a
widget. This is not treated as debt to fix — the spec explicitly accepts
this trade-off (spec §4, "Rules": "`reportrenderer` is the one exception:
it links `Qt::Charts` + `Qt::Widgets` + `QXlsx`"; spec §10, Risk 2) and
Section 19 covers it as a named risk rather than a defect. Listed here only
so the technical-debt inventory is honest about every non-obvious
dependency uncovered while reading the code.

---

## 4. Design assessment

This section compares the current Qt Widgets UI (`qt-app/*.ui`,
`qt-app/resources/wits.qss`, `qt-app/theme.h`) against the three Claude
Design references named as the visual source of truth in the spec (spec §3):
`Admin Dashboard.dc.html`, `Library Kiosk v2.dc.html`, and
`Kiosk Style Options.dc.html`. The spec's §5 design-system decisions (tokens,
component library, `Theme` singleton) are the already-agreed target; this
section only diagnoses the gap between that target and what exists today —
it does not re-decide any token or layout choice.

### 4.1 The design references, read closely

`Kiosk Style Options.dc.html` is a three-direction exploration (`1a`
"Scholar Light", `1b` "Midnight Modern", `1c` "Maroon Bold" —
`Kiosk Style Options.dc.html:26,76,127`), each a self-contained static mock.
`Library Kiosk v2.dc.html` and `Admin Dashboard.dc.html` are the built-out,
interactive direction — both share one `:root` token block
(`Library Kiosk v2.dc.html:14`, `Admin Dashboard.dc.html:14`):
`--brand:#7E1A15`, `--brand-deep:#5E0E0B`, `--brand-soft:#F6E9E7`,
`--gold:#E8B10E`, `--gold-soft:#FDF3E0`. This confirms direction `1c` (Maroon
Bold) is the one that got built out — `Kiosk Style Options.dc.html:129-133`
uses the identical `#7E1A15`/`linear-gradient(180deg,#7E1A15 0%,#5E0E0B
100%)`/`#E8B10E` triple as the finished files' CSS variables, and `1a`/`1b`
(`#8C1D18` maroon-lite and `#0C1524` navy respectively) do not recur anywhere
in the two built-out files — they are rejected directions, not alternate
themes to reconcile against.

Beyond brand/gold, both built-out files use: page background `#FBF8F3`
(`Library Kiosk v2.dc.html:15`, `Admin Dashboard.dc.html:15`), card
background `#FFFFFF` with a `2px solid #EFE5D4` border (e.g.
`Admin Dashboard.dc.html:80`, `.../:114`, `.../:139`), body text `#3A2C22`
(`Library Kiosk v2.dc.html:27`, `Admin Dashboard.dc.html:27`), muted/caption
text `#8B7A62` (e.g. `Admin Dashboard.dc.html:66`,
`Library Kiosk v2.dc.html:99` "VISITORS TODAY" label color `#B0A08A` is a
second, lighter muted tone used specifically for stat-tile captions), row
zebra/header tint `#F7F1E6` (`Admin Dashboard.dc.html:140`,
`Library Kiosk v2.dc.html:110`), and row hairlines `#F3ECDD` (e.g.
`Admin Dashboard.dc.html:145`). Radii read 12px (inputs/buttons,
`Admin Dashboard.dc.html:135-137`) up to 16-18px (cards, e.g.
`Admin Dashboard.dc.html:75,80`, `Library Kiosk v2.dc.html:76,98`) — the
spec's "8-18px" range (spec §5) brackets this correctly, with the low end
(8px) appearing on smaller controls such as the hourly-bar corners
(`Admin Dashboard.dc.html:107`, `border-radius:6px 6px 3px 3px`) rather than
the primary card radius. Typography is `Source Serif 4` for headings (e.g.
page titles `Admin Dashboard.dc.html:65`, kiosk brand wordmark
`Library Kiosk v2.dc.html:37`) and `Public Sans` for everything else (both
files' root `font-family`, `Library Kiosk v2.dc.html:27`,
`Admin Dashboard.dc.html:27`) — matching the spec verbatim.

The derivation math is not just described in the spec — it is executable in
the reference files themselves and matches `brandcolormath.h`'s intent
exactly. Both `Library Kiosk v2.dc.html:219` and
`Admin Dashboard.dc.html:314` compute `--brand-deep` via
`this.shade(a, -0.28)`, and both compute `--brand-soft`/`--gold-soft` via
`this.mix(a-or-g, '#FFFFFF', 0.9)` /`0.88`
(`Library Kiosk v2.dc.html:220,222`, `Admin Dashboard.dc.html:315,317`); the
`shade`/`mix` JS bodies (`Library Kiosk v2.dc.html:225-235`,
identical in `Admin Dashboard.dc.html:319-328`) are a channel-wise multiply
and linear-interpolate over the hex triplet — the same shape of operation as
`BrandTheme`'s `shade()`/`mix()` (per the spec §5, "Derivations use the
shipped `brandcolormath.h` rules"). This is a second, independent
confirmation (beyond the spec's own claim) that the design files were built
*against* the shipped brand-math contract, not an unrelated palette that
merely happens to look similar.

Motion in the references is real CSS `@keyframes`, not just prose:
`Admin Dashboard.dc.html:19-23` defines `pageIn` (opacity+translateY,
page-level), `rowIn` (opacity+translateY, per-row list entry), and `barGrow`
(`transform:scaleY(0)→(1)`, the hourly chart); `Library Kiosk v2.dc.html:18-23`
adds `cardIn`, `scanPulse` (the RFID-affordance breathing dot/border), and
`goldSweep` (the login button's light-sweep affordance). Durations in the
inline `style` attributes cluster at 150-400ms with cubic-bezier easing
(e.g. `animation:pageIn .4s cubic-bezier(.2,.8,.3,1)`,
`Admin Dashboard.dc.html:73`; `transition:transform .15s, box-shadow .2s`,
`Library Kiosk v2.dc.html:98`) — squarely inside the spec's "150-400ms"
range (spec §7, "Motion"). Both files are light-theme only; neither
`Admin Dashboard.dc.html` nor `Library Kiosk v2.dc.html` defines a dark
palette or a `prefers-color-scheme` branch, consistent with the spec's own
note (spec §3, "they are light-theme only (dark is derived, §6)").

### 4.2 Where today's Widgets UI already aligns

Structurally, the current admin surface's shape is closer to the design
than a from-scratch rebuild would suggest: `adminwindow.ui:142`
(`QFrame#sidebarFrame`) is already a persistent left sidebar with five nav
buttons (`generalBtn`, `databaseBtn`, `reportingBtn`, `studentSearchBtn`,
`visitorBtn` — `adminwindow.ui:182,198,214,230,246`), which is the same
"persistent left sidebar" shape as `Admin Dashboard.dc.html:30-57`'s `<aside>`
nav — only the token values and card treatment differ, not the layout
skeleton. Radii are already in use on card-like frames:
`resources/wits.qss:158-165` sets `QGroupBox { border-radius: 16px; }` and
`resources/wits.qss:174-180` sets the same 16px on
`securityFrame`/`adminFrame`/`libraryFrame`/`settingsFrame`/`databaseFrame`
— inside the design's 8-18px band even though the color values under those
rounded corners are wrong (Section 4.3). The kiosk's right-panel card
(`QFrame#frame_2`, `resources/wits.qss:84-87`) already uses a 16px radius
matching the design's card radius almost exactly. `MainWindow` already runs
one animation on login: `QPropertyAnimation` fading in the student photo
over 700ms (`mainwindow.cpp:260-264`) — slower than the design's 150-400ms
band but directionally the same idea (a soft entrance transition on new
data), not something that needs inventing from zero.

### 4.3 Where today's UI diverges from the design references

The color system is the largest, most consequential gap, and it is a
three-way divergence, not a two-way one: `theme.h`'s `Color::*` constants
(`theme.h:65-79`) are blue/green/slate — `AdminPrimary = "#2563EB"`,
`KioskPrimary = "#10B981"`, `SidebarBase = "#1E293B"` — with zero overlap
with the design's maroon/gold vocabulary; `resources/wits.qss` independently
hardcodes the same blue/slate family at every major surface (`QPushButton`
default background `#3B82F6`/hover `#2563EB`, `resources/wits.qss:31-39`;
`QHeaderView::section` background `#1E293B`, `resources/wits.qss:52-58`;
`QFrame#sidebarFrame` background `#1E293B`, `resources/wits.qss:132`); and
several `.ui` files layer in a *third*, ad hoc set of hardcoded hex values
inline in Qt Designer's per-widget stylesheet property rather than through
`wits.qss` at all — `adminwindow.ui:2225-2236`'s `extractVisitorBtn` hardcodes
`background-color: #2ECC71` / hover `#27AE60` directly in the `.ui` XML, and
`adminwindow.ui:2251-2280`'s `visitorTotalLabel` hardcodes a `qlineargradient`
from `#FFFFFF` to `#F0F0F0` plus a `📊` emoji in the label text — a rule that
bypasses even `theme.h`'s own constants, let alone the design's tokens. None
of these three layers uses `#7E1A15`/`#E8B10E`/`#FBF8F3`/`#3A2C22`/`#8B7A62`
anywhere. This is exactly the situation the spec's §5 `Theme` singleton is
designed to end ("the PRD's 'no stray hardcoded colors' grep audit remains
enforceable") — today, a grep for the design's tokens against `qt-app/`
returns zero hits, which is the single clearest piece of evidence for why
the migration cannot just re-skin the existing widgets in place.

> ⚠ Decision tension: the spec locks "logo-derived brand palette overrides
> brand roles" (spec §2) as the *runtime* theming mechanism, and
> `BrandTheme::fallbackPalette()` is defined to reproduce `theme.h`'s
> blue/green constants verbatim for an unbranded install (Section 2.6 of
> this document). That means an unbranded LOAMS 2.0 install's *default*
> palette is still blue/green, not maroon/gold, by the current fallback
> contract — the design references' maroon/gold is a *branded* look (the
> Maryknoll School of Lupon sample data baked into both `.dc.html` files'
> `schoolName`/`brandColor` props, e.g. `Library Kiosk v2.dc.html:133`),
> not the zero-config fallback. This is not a contradiction to fix in Phase
> 0 — the spec's fallback-palette contract already anticipates exactly this
> — but it means Section 12/13's design-system spec must state explicitly
> which of the two (maroon/gold vs. today's blue/green fallback) is the
> *unbranded* default going forward, since the design references alone
> don't answer that question.

Typography diverges completely: neither `Source Serif 4` nor `Public Sans`
is referenced anywhere in `qt-app/` — `resources/wits.qss:6` sets
`font-family: "Segoe UI", "Roboto", sans-serif` as the global default, and
no `.ui` file loads a custom font family beyond scattered `Segoe UI`
point-size overrides (e.g. `adminwindow.ui`'s `groupBox_3` font block at
`adminwindow.ui:1712-1717`). Component chrome also diverges structurally,
not just chromatically: the design's reporting/dashboard cards are flat
`<div>`s with a 2px border and 16px radius (`Admin Dashboard.dc.html:75,80`),
while the current reporting page composes native `QGroupBox` widgets with
Designer's default frame/title-bar chrome and all-caps titles like
"VISUALIZATION" (`adminwindow.ui:1705-1719`, `groupBox_3`) — `QGroupBox`'s
native title-bar-and-frame look has no equivalent in the design's card
vocabulary at all, so this is a component-family gap, not a color gap.
Motion is essentially absent from the current app: outside the one 700ms
photo fade (Section 4.2), there is no row-entry animation on the recent-logins
feed (`MainWindow::refreshRightPanel`, `mainwindow.cpp:366-403`, sets text
synchronously with no transition), no page-transition on sidebar navigation
(`adminwindow.cpp:292-311`'s `setCurrentWidget` swaps instantly), and no
chart-bar growth animation on the reporting page's charts — every
`pageIn`/`rowIn`/`barGrow`/`cardIn`/`scanPulse`/`goldSweep` keyframe in the
reference files (Section 4.1) is net-new work, not a speed/easing tweak to
something that already animates.

The admin page-header is a second structural gap worth calling out
specifically because it is easy to miss: the design's persistent page header
(title + subtitle + live clock, `Admin Dashboard.dc.html:63-69`) has no `.ui`
counterpart at all — `adminWindow::buildHeaderBar()` (`adminwindow.cpp:85-127`)
constructs the header bar, title label, avatar, and "Administrator" label
entirely in C++ at runtime and physically re-parents `ui->stackedWidget`
into a new wrapper widget to make room for it. This works, but it means the
current "page header" is a runtime patch bolted onto a `.ui` layout that
was not designed to have one — the QML rebuild should treat the design's
header as a first-class `AppShell.qml` region (per spec §4's
`AppShell.qml`) rather than repeating the reparent-at-runtime pattern.

### 4.4 Reconciliation notes (repo vs. design conflicts)

- **Fallback vs. branded palette** — covered above (Section 4.3's
  ⚠ Decision tension): the repo's `BrandTheme::fallbackPalette()` contract
  and the design references' maroon/gold sample do not describe the same
  install state, and Section 12/13 must say which is the true zero-config
  default.
- **"General" page vs. a dashboard** — the design's first sidebar entry is
  a stats/charts dashboard (`Admin Dashboard.dc.html:72-129`, "General":
  visitor/week/student/peak-hour tiles + hourly and department bars). The
  current first sidebar entry, also literally labeled `generalBtn`
  (`adminwindow.ui:182`) and landing on `generalPage`
  (`adminwindow.ui:275`), is a **settings** page — it hosts `adminFrame`,
  `libraryFrame`, `securityFrame`, and `settingsFrame`
  (`adminwindow.ui:278,343,474,563`, school name/address/font, admin key,
  library hours, logo/poster import). None of today's five sidebar pages is
  a stats dashboard; the dashboard the design calls "General" does not
  exist yet in any form — it is wholly new UI backed by data the app
  already has (`ReportController`'s existing endpoints, Section 2.2). This
  is a naming collision, not a design conflict to resolve by choice — the
  design's "General"/dashboard concept and the repo's `generalBtn`/settings
  concept are simply two different screens that happen to share a word,
  and Section 9's module organization should not conflate them.
- **Visit-log filter shape** — the design's Visit Logs page uses a two-way
  segmented Today/This-Week toggle (`Admin Dashboard.dc.html:232-236`,
  `logRanges`). The current equivalent control,
  `adminWindow::collectVisitorFilter()` (`adminwindow.cpp:2477-2506`,
  reading `ui->visitorDateFilterBox`, `adminwindow.ui:2310`), is a
  four-option `QComboBox` (All / Specific Day / Month / Date Range) on what
  is actually the **guest/visitor log** page (`visitorPage`,
  `adminwindow.ui:2214`), not the student attendance log. The spec already
  names this precisely as work, not a pre-existing feature: "the
  Today/This-Week visitor-log toggle inherited from the old P3 scope, which
  lands natively here" (spec §7, "Admin") — so no reconciliation is needed
  beyond noting that the combo-box version is the actual current state this
  quoted line refers to.

## 5. Product UX assessment

This section walks each existing screen/workflow in the current Widgets
app, citing the driving `.ui` file and the `mainwindow.cpp`/
`adminwindow.cpp`/`guestwindow.cpp` slot(s) that implement it, and names
friction points and simplification opportunities at the "what to improve
and why" level (the concrete redesign is Section 18). Per the interface
this document establishes for later sections, the canonical screen names
are: **Kiosk login, Live attendance feed, Guest flow, Admin dashboard,
Database/students, Reporting, Search, Visit logs, Settings.** Two of those
names (Admin dashboard, Visit logs) name *design-target* screens that do
not exist as such in the current app (Section 4.4); the walk below says so
explicitly at each point rather than describing aspirational UI as if it
were shipped.

### 5.1 Kiosk login

**Current workflow:** `mainwindow.ui`'s left panel (`QFrame#frame`,
`mainwindow.ui:40`) hosts a single `QLineEdit#username`
(`mainwindow.ui:143`) and one `QPushButton#loginBtn`
(`mainwindow.ui:168`). `MainWindow::handleLogin()`
(`mainwindow.cpp:180-235`) inspects the typed text with
`input.toLongLong(&ok)` (`mainwindow.cpp:193`) to decide, purely by
shape, whether the string is a student ID (numeric → POSTs to
`student_login.php`) or an admin key (non-numeric → POSTs to
`admin_login.php`) — one field serves two entirely different auth flows.
Submission fires on `QLineEdit::returnPressed`
(`mainwindow.cpp:92`) or the login button's `clicked`
(`mainwindow.cpp:126`). RFID input runs a second, parallel path: an
application-wide event filter (`RfidKeyboardFilter`,
installed `mainwindow.cpp:95`) intercepts fast keystroke bursts and emits
`rfidScanned`, handled by `MainWindow::handleRfidLogin()`
(`mainwindow.cpp:299-342`), which POSTs to `rfid_login.php` and includes a
2.5-second same-card debounce (`mainwindow.cpp:301-305`) so one tap
does not double-log a visit.

**Friction points:**
- The dual-purpose ID/admin-key field
  (`mainwindow.cpp:193-200`) has no visible affordance distinguishing "you
  are about to log in as a student" from "you are about to authenticate as
  an admin" — the only feedback is that the field's echo mode silently
  flips to password-dots the instant a non-numeric first character is
  typed (`mainwindow.cpp:144-154`), which is easy to miss and offers no
  explanation of *why* the field just changed behavior.
- Failed login surfaces as a modal `QMessageBox::warning`
  (`mainwindow.cpp:184,232`) on a full-screen kiosk (`showFullScreen()`,
  `mainwindow.cpp:44`) — a blocking dialog is a heavier interruption than
  the design's inline `scanPulse`/status-row affordances
  (`Library Kiosk v2.dc.html:20`) for a walk-up, no-mouse kiosk context.
- There is no visible "scanning" affordance distinct from typing — the
  design's pulsing-dot "SCAN OR TYPE YOUR ID" label
  (`Library Kiosk v2.dc.html:43-46`) has no counterpart; today's field is a
  plain `QLineEdit` with a static placeholder, so a student approaching the
  kiosk gets no cue that RFID is even an option versus typing.

**Simplification/automation opportunities:** separating the admin
authentication path from the student-ID field (an explicit "Admin" affordance
rather than shape-sniffing the input) would remove the silent password-mode
flip entirely; replacing the blocking `QMessageBox` with the kiosk's own
inline status surface (`MainWindow::showKioskStatus`, `mainwindow.cpp:278-297`,
which already exists and is used for RFID failures but not for
`handleLogin()`'s failures) would make failed manual logins consistent with
failed RFID scans instead of two different error UX for the same outcome.

### 5.2 Live attendance feed

**Current workflow:** `MainWindow::displayStudent()`
(`mainwindow.cpp:237-276`) prepends a successful login to an in-memory
`recentLogins` list capped at 9 (`mainwindow.cpp:272-274`), then calls
`refreshRightPanel()` (`mainwindow.cpp:366-403`), which writes into five
parallel, hand-named `QList<QLabel*>` — `nameLabels`, `courseLabels`,
`yearLabels`, `depLabels`, `timeLabels` — each listing exactly 9 named `.ui`
widgets by object name (e.g. `ui->nameLabel`, `ui->nameLabel_2` …
`ui->nameLabel_13`, `mainwindow.cpp:367-368`, backed by `mainwindow.ui:380`
and its numbered siblings, plus the `widget_2`..`widget_9` row containers at
`mainwindow.ui:651,800,949,1098,1247,1396,1545,1694`). Slot 0 doubles as
both the "now signed in" hero card and row 1 of the feed — there is no
separate hero widget, just index-0 of the same fixed arrays.

**Friction points:** the feed is a hard 9-row ceiling baked into named
widget instances, not a scrollable/virtualized list — the 10th visitor of
the day is simply not shown anywhere in the feed (only `recentLogins`'s
in-memory cap enforces this; there is no "view more" or scroll affordance
at all, since `mainwindow.ui`'s row widgets are fixed, non-repeating
instances). There is no live-row-entry motion (Section 4.3) — a new login
simply overwrites label text with no transition, unlike the design's
`rowIn` animation (`Library Kiosk v2.dc.html:18`) that visually distinguishes
"just happened" from "already there." The hero/row-0 conflation also means
the "now signed in" spotlight and the feed's most-recent row cannot be
styled or timed independently (e.g. the design's 12-second auto-clear on the
hero, versus the feed row staying until scrolled off) — they are
structurally the same object today.

**Simplification/automation opportunities:** this is the literal example
the spec's own architecture section calls out as needing a
`QAbstractListModel`-backed `VisitLogsViewModel` (spec §4; this document's
Section 2.4 already flags `refreshRightPanel` for exactly this reason) — an
unbounded/scrollable list model would remove the 9-slot ceiling and let the
hero card become a genuinely separate, independently-animated element
rather than an alias for row 0.

### 5.3 Guest flow

**Current workflow:** `GuestWindow` (`guestwindow.h`/`.cpp`, backed by
`guestwindow.ui`) is a modal `QDialog` opened from
`ui->guestLoginBtn->clicked` (`mainwindow.cpp:86-89`), itself only visible
when the admin has enabled it via `adminWin`'s
`guestLoginToggled` signal (`mainwindow.cpp:83-85,91`) — i.e. guest login is
an admin-gated, off-by-default kiosk feature. The dialog collects four
fields — full name, contact, company, purpose
(`guestwindow.cpp:30-33`) — validates that name/company/purpose are
non-empty (`guestwindow.cpp:35-38`), then POSTs to `guest_login.php`
(`guestwindow.cpp:40-50`) and closes itself on a successful JSON response
(`guestwindow.cpp:69-71`).

**Friction points:** all validation and network-error feedback is a modal
`QMessageBox` (`guestwindow.cpp:36,54,64,70,73`) — five separate dialog
call sites for what is a four-field form, each one a full interruption
rather than inline field-level feedback. "Contact" is optional while name/
company/purpose are required, but the form gives no visual distinction
(no asterisk, no styling difference) between the required and optional
fields until the user already submitted and hit the warning dialog.

**Simplification/automation opportunities:** inline validation (mark
required fields, disable submit until satisfied) removes 3 of the 5 modal
dialogs (the three validation-only ones) without touching the two genuine
network-outcome dialogs; this is a small, self-contained flow well suited
to a single QML form component with bound validation state.

### 5.4 Admin dashboard

**Current state — does not exist as a dashboard.** As established in
Section 4.4, the sidebar button literally named `generalBtn`
(`adminwindow.ui:182`) and its `generalPage`
(`adminwindow.ui:275`) is a **settings** screen (school info, admin key,
library hours, logo/poster import — `adminFrame`/`libraryFrame`/
`securityFrame`/`settingsFrame`, `adminwindow.ui:278,343,474,563`), not a
stats/charts overview. There is no current screen showing visitors-today,
visitors-this-week, registered-student-count, peak-hour, or the
hourly/department bar charts as a landing dashboard — those numbers exist
in the backend and are already computed for the Reporting page's charts
(`ReportController`, `reportcontroller.cpp`, Section 2.2), but nothing
today surfaces them as an at-a-glance admin landing screen. The Admin
dashboard the design references show (`Admin Dashboard.dc.html:72-129`) is
therefore net-new UX, not a redesign of an existing screen — Section 18
should treat it as new construction over existing report data, and Section
9's module organization should not assume a 1:1 mapping from the current
`generalBtn`/`generalPage` pair to the design's dashboard concept.

### 5.5 Settings

**Current workflow:** what the design calls "Settings" is today folded into
the `generalBtn`/`generalPage` screen described in Section 5.4 — there is
no dedicated settings sidebar entry; `SettingsController`
(`qt-app/settingscontroller.h`, constructed `adminwindow.cpp:132`) backs the
school/admin/library `QSettings` fields on that one page, and the same page
also hosts logo/poster import, which triggers
`BrandingController`/`BrandTheme::regenerateFromLogo` re-theming
(`adminwindow.cpp:1174-1187`, covered in Section 2.3).

**Friction points:** because "settings" and the (currently nonexistent)
"dashboard" concept share one sidebar slot and one page, there is no room
to add dashboard content to that slot without first splitting it — the two
screens' current entanglement is itself the friction, not any one control
on the page. `adminWindow::collectHeaderInfo()`
(`adminwindow.cpp:1401-1412`) additionally reads the same settings keyspace
through a second, separate `QSettings` construction rather than through
`SettingsController::load()` (Section 2.4) — a maintenance/consistency
issue more than an end-user-visible one, but worth carrying into Section 10
as a case the settings ViewModel must consolidate rather than reproduce.

**Simplification/automation opportunities:** splitting today's one
"General" page into two design-aligned screens (a genuine dashboard and a
genuine Settings screen, each with its own sidebar entry) is the direct
fix; Section 9 should treat this as adding a sidebar entry, not renaming an
existing one, since the settings content itself does not need to move, only
to stop sharing a page with dashboard content that doesn't exist yet.

### 5.6 Database/students

**Current workflow:** `databasePage` (`adminwindow.ui:685`) combines two
sub-flows on one screen: a bulk import path — `onAttachFileBtnClicked()`
(`adminwindow.cpp:825-857`) opens `AttachFilesDialog`
(`adminwindow.cpp:827`) to pick a CSV/Excel + optional photo ZIP, populating
`ui->bulkTable` (`adminwindow.cpp:1059-1068`) via `ImportController`
(constructed `adminwindow.cpp:153`), with `onUpdateDatabaseBtnClicked()`
(`adminwindow.cpp:858`) committing the reviewed batch — and a live-edit path
on the same `bulkTable`, where `QTableWidget::itemChanged`
(`adminwindow.cpp:2061`) drives inline per-cell edits. `StudentController`
(constructed `adminwindow.cpp:166`) backs department/course list population
for whichever filters this page exposes.

**Friction points:** import and direct in-place table editing are two
different mental models (review-a-staged-batch vs. edit-a-live-record)
living on the same table widget, distinguished only by which code path last
touched `bulkTable` — there is no persistent visual cue on the page for
"you are viewing a staged import" versus "you are editing live records."
The design's Database page (`Admin Dashboard.dc.html:132-164`) treats these
as clearly separate actions (`+ ADD STUDENT` / `IMPORT CSV` buttons,
`Admin Dashboard.dc.html:136-137`) feeding one always-live table, which is a
simpler mental model than today's dual-mode single table.

**Simplification/automation opportunities:** separating "staged import
preview" from "live student table" into two distinct visual states (even
before any QML work) would remove the current implicit-mode ambiguity; this
maps directly onto the design's add/import-as-actions-not-modes pattern and
is a natural `StudentsViewModel` responsibility (Section 10) — track "am I
previewing an import" as explicit state rather than inferring it from which
handler last ran.

### 5.7 Reporting

**Current workflow:** `reportingPage` (`adminwindow.ui:1298`) exposes a
duration-type switch (`ui->durationTypeBox`,
feeding `QStackedWidget#durationTypeWidget`, `adminwindow.ui:1523`, with
per-mode pages `specificDay`/`specificMonth`/`semester`,
`adminwindow.ui:1527,1541,1626`) plus a department/course filter pair and a
palette selector. `adminWindow::collectReportFilters()`
(`adminwindow.cpp:1498-1605`) reads all of these directly from `ui->`,
validates with inline `QMessageBox::warning` calls, and delegates only the
date-range math to `ReportController::computeDateRange`
(Section 2.2/2.4). Charts render into `QGroupBox`-framed regions
(e.g. `groupBox_3`/"VISUALIZATION", `adminwindow.ui:1705-1719`) via
`ReportRenderer`'s `QChart`-based static painters (Section 2.2).

**Friction points:** `collectReportFilters` hard-requires a department
selection before anything can be generated
(`adminwindow.cpp:1505-1509`, `if (ui->filterDepartmentBox->currentIndex()
<= 0) { ...; return {}; }`) — there is no "all departments" report path at
all; a librarian who wants a school-wide report must still pick one
department first, or the generation call silently returns an empty
`QJsonObject` after the warning dialog. Seven separate inline
`QMessageBox::warning` calls across one method
(Section 2.4) mean seven slightly different ways to tell the user "you
forgot a filter," rather than one consistent inline validation surface. The
`QGroupBox` chart containers (Section 4.3) read as native OS-chrome frames
with all-caps titles, not the design's flat bordered cards
(`Admin Dashboard.dc.html:75-95`'s stat tiles and chart cards) — a purely
visual gap, but one that makes the reporting page look the least "premium"
of the five current admin pages next to the design references.

**Simplification/automation opportunities:** the missing "all departments"
path is a real product gap worth carrying into Section 18 as a required
capability (not just a visual refresh) — the design's report cards
(`Admin Dashboard.dc.html:170-177`) imply one-click report generation
without a mandatory per-department step. Consolidating the seven validation
messages into one inline filter-summary (e.g. disable "Generate" until
filters are valid, with per-field inline hints) is the same
"replace modal validation with inline state" pattern already identified in
Sections 5.1 and 5.3.

### 5.8 Search

**Current workflow:** `studentSearchPage` (`adminwindow.ui:1966`) has one
`QLineEdit#searchLineEdit` wired to `QLineEdit::returnPressed`
(`adminwindow.cpp:438`) calling `adminWindow::onSearchBtnClicked()`
(`adminwindow.cpp:2441-2444`), which calls `performStudentSearch(true)`.
Department/course filter combos
(`onDepartmentFilterChanged`/`onCoursesLoaded`, `adminwindow.cpp:2451-2475`)
narrow the search via `StudentController`. Results populate
`ui->studentSearchTable` (9 columns, `adminwindow.cpp:435-436`), which also
supports inline bulk-edit (the same `itemChanged`-driven pattern as
Section 5.6's `bulkTable`, `adminwindow.cpp:2061`).

**Friction points:** search only executes on Enter
(`adminwindow.cpp:438`) — there is no live-as-you-type filtering and no
keyboard shortcut to jump into the search field from elsewhere in the admin
surface (no `QShortcut` for search exists anywhere in `adminwindow.cpp`,
confirmed by grep). This is the direct current-state gap the spec's
"Ctrl+K quick search" requirement (spec §7, "Keyboard & accessibility")
is written against — today there is no fast way to reach Search at all
except clicking the sidebar button.

**Simplification/automation opportunities:** live filtering (debounced
`textChanged` instead of `returnPressed`-only) and a global quick-search
shortcut are both additive, non-breaking changes conceptually — but per the
spec they land as new QML behavior (`Navigator`-owned shortcut, spec §4)
rather than a Widgets patch, since the shortcut needs to work across the
whole admin surface, not just this one page.

### 5.9 Visit logs

**Current state — does not exist as a dedicated screen; closest existing
analog is the guest/visitor log.** There is no sidebar entry or page named
"Visit Logs" in `adminwindow.ui`. The nearest existing screen is
`visitorPage` (`adminwindow.ui:2214`, reached via `ui->visitorBtn`,
`adminwindow.cpp:246`), which lists **guest sign-ins** (name, company,
purpose, date, time — `adminWindow::populateVisitorTable`,
`adminwindow.cpp:2508-2532`), filtered by the four-mode
`visitorDateFilterBox` combo covered in Section 4.4
(`collectVisitorFilter`, `adminwindow.cpp:2477-2506`) — not a log of
*student* attendance (time-in/time-out), which is what the design's Visit
Logs page shows (`Admin Dashboard.dc.html:229-260`, `lg.timeIn`/`lg.timeOut`
per row). The student-attendance data these design rows would need already
exists as the source feeding the Kiosk's live attendance feed
(Section 5.2) and the Reporting page's charts, but there is currently no
admin screen that presents it as a browsable, filterable log the way the
design shows.

**Friction points:** the naming collision itself is the friction — a
future contributor reading `visitorBtn`/`visitorPage`/`VisitorController`
could reasonably assume this is the "visit logs" screen the design and the
spec (spec §7, "the Today/This-Week visitor-log toggle") describe, when it
is actually the guest-only log. The spec's own phrasing ("visitor-log
toggle... lands natively here") uses "visitor" to mean the guest-sign-in
concept specifically, which this document's screen-name interface
disambiguates going forward: **Visit logs** (design-target, student
attendance history) versus the current, narrower **guest log**
(`visitorPage`/`VisitorController`) are two different things, and Section
18 needs to design both — a genuine student Visit Logs screen (net-new,
per Section 5.4's reasoning) and a modernized guest log (a redesign of
`visitorPage`, gaining the design's Today/This-Week segmented control in
place of today's four-mode combo).

**Simplification/automation opportunities:** none beyond what Section 18
will specify for the two now-distinguished screens; flagged here as scope
clarification, not a workflow fix.

### 5.10 Out-of-scope modules (explicit non-findings)

Per the owner brief and the spec's locked 2.0 scope (spec §2, "Migrate
existing modules only... Inventory / Borrow-Return / AI Assistant are OUT
of 2.0"), this assessment explicitly does not evaluate UX for Inventory,
Borrow/Return, or an AI Assistant, because **none of the three exists
anywhere in this repository** — no `.ui` file, controller, or PHP endpoint
under `deliverables/loams_api/` references inventory items, borrow/return
transactions, or an assistant/chat feature of any kind. These are named
here only as confirmed forward-seams: the architecture (Sections 7-10)
must leave room for a future `inventory`/`borrowreturn`/`aiassistant`
viewmodel+qml+core-service triple (spec §4, "Future modules... plug in as
new viewmodel + qml/module + core-service triples"), but there is no
current-state UX to diagnose because there is no current-state screen to
walk.

## 6. Qt Widgets → Qt Quick migration strategy

Strategy A — a **parallel rebuild on a shared core** — is locked (spec §2)
and is not reopened here. This section states it at implementation depth:
the two rejected alternatives and why they were rejected, what is preserved
untouched, the UI-replacement approach per surface, how the missing
ViewModel/model seam gets built, how the component library and theme migrate,
the rollback posture, and the implementation order that keeps every gate
green.

### 6.1 The chosen strategy, and the two alternatives already rejected

Strategy A rebuilds the presentation layer in Qt Quick **against the same
business logic**, running the new Quick app and the legacy Widgets app side
by side out of one repository (Section 7.4) until the Quick app reaches
feature parity, at which point the Widgets app is deleted (Section 6.6). The
two alternatives were considered during brainstorming and rejected as
*already-decided* rationale, not live options:

- **In-place port (re-skin the existing widgets).** Restyling `*.ui`/
  `wits.qss` to the maroon/gold design without changing the architecture was
  rejected because the design gap is structural, not chromatic: Section 4.3
  establishes that `QGroupBox` native chrome "has no equivalent in the
  design's card vocabulary at all" (a component-family gap), that every
  `pageIn`/`rowIn`/`barGrow`/`scanPulse` motion is net-new rather than a
  tweak, and that "a grep for the design's tokens against `qt-app/` returns
  zero hits." An in-place re-skin also leaves the 45-widget fixed feed
  (`mainwindow.cpp:366-403`, Section 2.4/5.2), the closed-set sidebar
  (`adminwindow.cpp:292-311`, Section 2.5), and the runtime-reparented
  page header (`adminwindow.cpp:85-127`, Section 4.3) exactly as they are —
  none of which can meet the spec §4 forward-compat requirement ("nothing in
  2.0 may assume the admin sidebar is a closed set") without the very MVVM
  seam a re-skin declines to build.
- **Big-bang rewrite (new app, backend and logic included, cut over once).**
  Rejected because it discards the assets Section 2.8 certifies as already
  reusable — the five domain controllers, `ReportRenderer`, the brand
  engine, `RfidScanDetector` — and because it has no continuously-shippable
  intermediate state: there is no phase at which the 12 tests (Section 2.7)
  and a runnable app both exist until the very end, which is exactly the
  verification posture spec §8 requires *every* phase to preserve. A rewrite
  also reopens the locked "keep the PHP/MySQL backend" decision (Section 16),
  which is out of scope.

Strategy A is the only one of the three that (a) reuses the Widgets-free core
by `git mv` rather than rewrite (Section 2.8/7.1), (b) keeps a runnable,
test-green app at every phase boundary (Section 6.7), and (c) retains a
working rollback until parity is proven (Section 6.6).

### 6.2 What is preserved untouched: the backend and the core

Two whole layers do **not** get rebuilt and are carried across verbatim:

- **The backend.** The PHP/MySQL server, its 25 request-handling scripts,
  and every request/response contract are reused as-is (Section 16). The
  migration replaces the *caller* (widget method bodies → ViewModel/service
  seams), never the wire contract; the only backend work in the whole plan
  is the bounded, contract-tests-first Phase-6 hardening backlog of
  Section 17, which is behavior-preserving by construction and does not block
  the client migration.
- **The Widgets-free C++ core.** The six units Section 2.8 lists as
  "well-positioned today" — the five domain controllers, `ReportRenderer`,
  the brand engine, `RfidScanDetector`, `apiconfig.h`, and `theme.h`'s
  constants — move into the `witscore` static library (Section 7.1) by
  `git mv` + CMake path update, not rewrite. Their unit tests move with them,
  keeping the exact per-target shape Section 2.7 documents (Section 7.6/8).
  The one layer that is explicitly **re-authored rather than moved** is
  `RfidKeyboardFilter` (`rfidkeyboardfilter.*`), whose `QApplication`
  coupling (`rfidkeyboardfilter.cpp:25,33-36`, Section 2.4) has no
  `QQuickWindow` equivalent — the pure `RfidScanDetector` state machine
  underneath it is reused verbatim (Section 7.1/10.1).

### 6.3 The UI-replacement approach, per surface

The replacement is **surface-by-surface, not screen-by-screen-in-place**:
each of the two surfaces (kiosk, admin) is rebuilt as a set of QML screens
hosted by one `AppShell.qml` (Section 8), and the legacy window that used to
own that surface (`MainWindow`, `adminWindow`, `GuestWindow`) is retired only
once its Quick replacement reaches parity. The "one executable, two surfaces"
topology (Section 2.1/7) is preserved — surface switching changes from
constructing a second `QMainWindow` (`mainwindow.cpp:52,226`, Section 14.1) to
flipping one `Navigator.currentSurface` enum inside a single running QML
scene. Section 18 gives the concrete redesign for each of the nine screens in
the Section 5 inventory; Section 20 assigns each to a phase.

### 6.4 ViewModel/model migration: building the seam that does not exist yet

The core is reusable; the seam between it and QML is the net-new work
(Section 2.8: "there is currently no `ReportsViewModel`-shaped seam between
'read the widgets' and 'call `computeDateRange`'"). The migration builds, per
module, exactly the ViewModel + `qml/` module + core-service triple Section 9
fixes, with the eight canonical ViewModels of Section 10 — `KioskViewModel`,
`DashboardViewModel`, `StudentsViewModel`, `VisitLogsViewModel`,
`ReportsViewModel`, `SettingsViewModel`, `ThemeViewModel`, and `Navigator` as
connective tissue outside the triple pattern. Widget-reading method bodies
(`collectReportFilters`, `collectHeaderInfo`, `refreshRightPanel`, Section
2.4) are not ported line-for-line: their *logic* already lives (or is being
lifted) into a controller, and the ViewModel replaces the `ui->` reads with
`Q_PROPERTY` bindings and the inline `QMessageBox` validation with bindable
error state (Sections 10.3-10.6). Fixed-widget feeds become
`QAbstractListModel`s (`StudentListModel`, `VisitLogModel`, `ReportRowModel`,
Section 10) — this is where the 9-slot ceiling (Section 5.2) and the
per-column parallel-array indexing disappear.

### 6.5 Component-library and theme migration

Two cross-cutting layers are built once, up front (Phase 1), and every screen
composes them rather than re-inventing chrome:

- **Component library.** The ten `L*` primitives of Section 11 (`LButton`,
  `LCard`, `LTable`, `LSegmented`, `LSideNav`, `LToast`, `LDialog`,
  `LStatTile`, `LBarChart`, `LPageHeader`) are the whole vocabulary Section
  18's screens are built from. The standing rule (Section 11): screens
  compose these and never restyle a primitive locally, which is what makes
  Section 18 composition rather than styling.
- **Theme.** The three legacy theming layers (`theme.h`, `wits.qss`, the
  brand engine, Section 2.6) collapse into one `Theme.qml` singleton backed
  by `ThemeViewModel` (Sections 12/13). This is a *replacement, not a port*
  (Section 8): `theme.h`/`wits.qss` stay legacy-only and are retired as each
  Quick screen ships, while the brand engine's own contract
  (`BrandTheme::current()`/`setCurrent()`, cache-first sync, live
  logo-regeneration) is reused unchanged behind `ThemeViewModel` (Section
  10.7/13.2). The zero-config default palette question Section 4.3 flagged is
  resolved in Section 12.4 (shipped maroon/gold), and is a `ThemeViewModel`
  seeding behavior, not an engine change.

### 6.6 Rollback posture: the Widgets app survives until parity

Per Strategy A's own exit condition (spec §2: "Widgets app remains the
rollback until parity, then is deleted"), the legacy `WITS` executable stays
buildable and runnable for every phase before parity, gated by
`-DBUILD_LEGACY_WIDGETS=ON` (default `ON`, Section 7.6). Both executables link
the one `witscore` artifact (Section 7.4), so the core is compiled once and
the rollback path exercises the *same* business logic the new app does — a
rollback never means running stale logic. The flag's default flips to `OFF`
and the legacy sources are deleted only at the **Phase 6** deletion milestone
(Section 20), never earlier; there is no "Phase 5 deletes Widgets" step.

### 6.7 Recommended implementation order (continuous verification)

The order is the spec §9 roadmap (elaborated in Section 20), chosen so the
verification model (spec §8) holds at every boundary — **the 12 existing
ctest targets stay green in every phase**, because the core move is `git mv` +
path update (Section 7.6), not a rewrite that would break them:

1. **Phase 1 — core + shell first.** Extract `witscore` (both apps build,
   12/12 green), stand up `AppShell.qml` + `Theme.qml` + the `L*` component
   library, and run the RFID-in-QML spike (Section 19, Risk 1) and the
   deployment-hardware render check (Risk 4) *before* any screen depends on
   them. Nothing user-facing ships, but the two highest risks are retired.
2. **Phases 2-4 — one surface/screen-group at a time**, each behind a parity
   checklist (spec §8): kiosk (Phase 2), admin dashboard/visit-logs/search
   (Phase 3), admin database/reports/settings (Phase 4). Each phase adds
   ViewModel unit tests and Qt Quick Tests *on top of* the 12-target baseline
   (Section 8), never replacing it.
3. **Phase 5 — dark-mode + a11y polish**, the two items with no design
   reference (Sections 13.4/12.7/15.1) and therefore a budgeted human-review
   round (Risk 3).
4. **Phase 6 — backend hardening (Section 17) + Widgets deletion + final
   whole-branch review.**

At every phase gate the same four checks apply — Debug+Release build clean,
full ctest green, `/claude-review`, and a human visual walkthrough (premium
look is a human judgment, spec §8). Because the rollback (Section 6.6) stays
alive until Phase 6, a phase that fails its gate never leaves the project
without a working app.

## 7. Proposed project architecture

This section takes the spec's shared-core design (spec §4) to implementation
depth: the concrete CMake library boundary, its precise (non-uniform)
dependency envelope, the MVVM layering rule, the `Navigator` singleton, how
the legacy and new executables co-exist during the rebuild, and the
AUTOMOC/AUTOUIC/AUTORCC mechanics of splitting one flat target into several.

### 7.1 The `witscore` boundary — what moves, what doesn't

The spec's shared core becomes a concrete CMake static library, `witscore`
(`add_library(witscore STATIC ...)`), that absorbs every file Section 2.8
already certified as "well-positioned" — Widgets-free today and movable by
`git mv`, not rewrite: the five domain controllers (`settingscontroller`,
`visitorcontroller`, `importcontroller`, `studentcontroller`,
`reportcontroller`, plus their `*data.h` structs), `reportrenderer`, the
brand engine (`brandcolormath.h`, `brandthemedata.h`, `brandtheme.h/.cpp`,
`brandingcontroller.h/.cpp`), `apiconfig.h`, and the `RfidScanDetector` state
machine (`rfidscandetector.h/.cpp`, the pure `feedKey(QChar, qint64) ->
std::optional<QString>` machine, `rfidscandetector.h:20`). Section 2.4
already identified the one layer that does **not** qualify:
`RfidKeyboardFilter` (`rfidkeyboardfilter.h/.cpp`) is `QApplication`-coupled
at its core (`QApplication::activeWindow()`/`focusWidget()` gates,
`rfidkeyboardfilter.cpp:25,33-36`), and the spec explicitly calls for it to
be **re-authored** against `QQuickWindow` focus semantics, not moved (spec
§4, "Rules"); it stays a legacy-Widgets-only file until its Phase 1 spike
(spec §9/§10 Risk 1) produces its Quick-side replacement.

`witscore`'s dependency envelope is deliberately *not* uniform across its own
sources, and this document states that honestly rather than rounding it off:
the controller layer, brand engine, `apiconfig.h`, and `RfidScanDetector`
link only `Qt::Core`/`Qt::Network`/`Qt::Gui` — no `Qt::Widgets`, no
`Qt::Quick`. `reportrenderer` is a named, spec-accepted exception (spec §4,
"Rules": "`reportrenderer` is the one exception: it links `Qt::Charts` +
`Qt::Widgets` + `QXlsx`"; this document's own Section 3.9). `QChart` derives
from `QGraphicsWidget` in Qt 6, so anything that constructs a `QChart`
transitively pulls in `Qt::Widgets` regardless of whether it ever shows a
widget — `reportrenderer.h`'s three chart-image makers
(`makeBarChartImage`/`makePieChartImage`/`makeLineChartImage`,
`reportrenderer.h:29-35`) all route through the private
`renderChartToImage(QChart *chart, QSize size)` helper (`reportrenderer.h:49`),
and the existing `tst_reportrenderer` test target already pays this cost
today — `tests/CMakeLists.txt:152-157` links `Qt::Test`, `Qt::Gui`,
`Qt::Charts`, **`Qt::Widgets`**, and `QXlsx` for a target whose only public
surface is `QImage`-returning static functions. `witscore` is therefore not
one dependency-uniform target; in CMake terms it needs either (a) a single
`witscore` target whose `target_link_libraries` includes
`Qt::Charts`/`Qt::Widgets`/`QXlsx` as `PUBLIC` (simplest, but leaks
`Qt::Widgets` into every consumer, including the new Quick executable — an
accepted trade-off per spec §4/§10 Risk 2, not a defect to fix in Phase 0),
or (b) splitting `reportrenderer.*` into a second library
(`witscore_reports`) with the wider link set, keeping the base `witscore`
genuinely Widgets-free. Section 8's tree assumes (a) for simplicity during
the parallel-rebuild window, with (b) named as a Phase-6 cleanup candidate
once the legacy Widgets app — whose own `Qt::Widgets` link is already
unavoidable (`qt-app/CMakeLists.txt:65-72`) — is deleted and there is no
longer a second consumer to protect from the leak.

### 7.2 MVVM layering

QML → ViewModels → controllers → network/backend, exactly as spec §4
states, with one rule enforced at the architecture level rather than left as
convention: **ViewModels are the only QML-facing C++.** No QML file calls a
controller method, connects to a `*Controller` signal, or touches
`reportdata.h`/`visitordata.h`/etc. structs directly — every controller
signal is re-emitted (or translated into a `Q_PROPERTY` write) by exactly
one owning ViewModel, and every `Q_INVOKABLE` a QML `onClicked` handler
calls lives on a ViewModel, never a controller. This is the seam Section 2.8
found missing today ("there is currently no `ReportsViewModel`-shaped seam
between 'read the widgets' and 'call `computeDateRange`'"); Section 10 names
each ViewModel and its owned controller(s) concretely.

### 7.3 `Navigator`

A `QObject` singleton, registered to QML (`qmlRegisterSingletonType` or
`QML_SINGLETON`), owns surface (kiosk/admin) and page/modal state as
`Q_PROPERTY`s — replacing today's hand-wired `QPushButton::clicked` →
`ui->stackedWidget->setCurrentWidget(...)` connection per sidebar button
(`adminwindow.cpp:292-311`, Section 2.5) with a data-driven `currentPage`
property that `AppShell.qml`'s page host binds against. `Navigator` also
centralizes keyboard shortcuts — the Ctrl+K quick search the spec requires
(spec §7, "Keyboard & accessibility"; Section 5.8 already establishes there
is no `QShortcut` for search anywhere in `adminwindow.cpp` today) — so a
shortcut is wired once, application-wide, rather than per page. `Navigator`
manages *which page/modal is current*, not screen data, so it owns no
controller; Section 9 places it outside the module-triple pattern, as
connective tissue between triples rather than a triple itself.

### 7.4 Parallel linkage during the rebuild window

Per migration Strategy A (spec §2, locked), both the legacy Widgets
executable (`WITS`, today's `qt-app/CMakeLists.txt:31-53` block) and the new
Quick executable link `witscore`: `target_link_libraries(WITS PRIVATE
witscore)` and `target_link_libraries(<quick-executable> PRIVATE witscore)`,
both against the one CMake-built static-library artifact — the controllers,
brand engine, and `reportrenderer` are compiled exactly once per build, not
duplicated per executable. `WITS`'s own `qt_add_executable(...)` source list
shrinks accordingly: the eight core business-logic `.h`/`.cpp` pairs plus
seven data-struct headers currently listed inline
(`settingsdata.h`/`settingscontroller.h/.cpp` through
`brandingcontroller.h/.cpp`, `qt-app/CMakeLists.txt:38-52`) are removed from
the `WITS` target's source
list and replaced by the `witscore` link; only the genuinely Widgets-bound
files (`mainwindow.*`, `adminwindow.*`, `guestwindow.*`,
`attachfilesdialog.*`, `busyindicator.*`, `rfidkeyboardfilter.*`, the four
`.ui` files, `resources.qrc`) remain directly in `WITS`'s own source list.
Nothing about this changes `WITS`'s runtime behavior — it is a pure
"move the compilation unit, keep the include path working" change, backed by
the fact that these moved files already build as standalone units today
(Section 2.7/2.8's ctest evidence).

### 7.5 AUTOMOC / AUTOUIC / AUTORCC implications

`qt-app/CMakeLists.txt:5-7` sets `CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, and
`CMAKE_AUTORCC` all `ON` as directory-scoped variables; a new `witscore`
target created via `add_subdirectory(core)` inherits them unless the
subdirectory overrides them. Three implications follow, stated honestly
rather than assumed to "just work":

- **AUTOMOC is required and free.** Six of `witscore`'s classes are
  `Q_OBJECT`-derived (`SettingsController`, `VisitorController`,
  `ImportController`, `StudentController`, `ReportController`,
  `BrandingController` — the "injected, not owned" pattern Section 2.2
  already cites) and need `moc` to run over them. Because `CMAKE_AUTOMOC` is
  already `ON` at the point `witscore` is defined, this needs no extra CMake
  ceremony — the same mechanism that moc's these classes inside the flat
  `WITS` target today continues to work identically inside the library
  target.
- **AUTOUIC is a no-op for `witscore` and should be turned off there.** All
  four `.ui` files (`mainwindow.ui`, `adminwindow.ui`, `guestwindow.ui`,
  `attachFilesDialog.ui`) stay with the legacy Widgets sources; `witscore`
  has zero `.ui` inputs, so `set_target_properties(witscore PROPERTIES
  AUTOUIC OFF)` is a correctness no-op (nothing to generate) but avoids
  CMake scanning a source list that will never contain one.
- **AUTORCC needs two consumers, not one shared file.** `resources.qrc`
  (icons/fonts used by the `.ui` files) stays exclusively with the legacy
  `WITS` target — `witscore` has no UI assets to bundle. The new Quick
  executable needs its own resource mechanism for QML/JS files (either its
  own `.qrc` or Qt 6's `qt_add_qml_module` automatic resource embedding); the
  two executables each run their own `AUTORCC` pass over their own resource
  file with no shared or conflicting state.

### 7.6 The CMake surgery, stated honestly

This is real, non-trivial CMake work, not a relabeling exercise: the current
flat `qt_add_executable(WITS MANUAL_FINALIZATION ...)` block
(`qt-app/CMakeLists.txt:31-53` — the single target Section 3.1 already flags
as the direct blocker to any shared-core architecture) must be split into at
minimum three CMake targets: `witscore` (the new static library), `WITS`
(the legacy executable, now linking `witscore` instead of compiling its
sources inline), and the new Quick executable (also linking `witscore`).
This is genuinely Phase 1 work, not Phase 0 — Phase 0 produces this
document, not the CMake change itself (spec §2, "Process"; spec §9, Phase 1:
"`witscore` extraction"). Crucially, the 12 existing ctest targets
(Section 2.7's table) do **not** change shape: per the spec's own
verification model ("each target keeps compiling its controller `.cpp`
directly rather than linking a monolithic `witscore`... e.g.
`tst_reportcontroller` still omits `reportrenderer.cpp`," spec §8), every
target that today does `${CMAKE_SOURCE_DIR}/somecontroller.cpp` in its own
`qt_add_executable(tst_somecontroller ...)` call continues to do exactly
that after the move — only the path changes, from
`${CMAKE_SOURCE_DIR}/reportcontroller.cpp` to (for example)
`${CMAKE_SOURCE_DIR}/core/reportcontroller.cpp`. No test target is rewritten
to `target_link_libraries(... witscore)` in place of its direct compile —
that would trade today's lean, single-class dependency surface
(Section 2.7: "`tst_reportcontroller` links only `reportcontroller.cpp`, not
`reportrenderer.cpp`") for a monolithic one, the exact anti-pattern the
spec's verification model rules out by name.

The parallel-rebuild window is bounded by a build flag:
`-DBUILD_LEGACY_WIDGETS=ON` (default `ON`) keeps `WITS` buildable and
runnable as the rollback path for every phase before parity is declared;
only when the roadmap (Section 20) reaches the Phase 6 deletion milestone
does this flag's default flip to `OFF` and the legacy sources get removed,
per Strategy A's own exit condition (spec §2, "Widgets app remains the
rollback until parity, then is deleted").

## 8. Proposed folder structure

This expands the spec's §4 tree (reproduced there in outline form) to the
full target layout, annotating each directory with its one responsibility.
It does not contradict the spec tree — every top-level entry the spec names
(`core/`, `quick/` with `main.cpp`/`viewmodels/`/`models/`/`qml/`/`tests/`,
the retained `tests/`, and legacy sources building until Phase 6) appears
below, expanded to file level.

```
qt-app/
  CMakeLists.txt                 top level: defines witscore, WITS (legacy), the Quick executable
  core/                          witscore (static library) — Widgets-free business logic
    apiconfig.h                  backend base-URL/endpoint builder (unchanged)
    settingsdata.h
    settingscontroller.h/.cpp
    visitordata.h
    visitorcontroller.h/.cpp
    importdata.h
    importcontroller.h/.cpp
    studentdata.h
    studentcontroller.h/.cpp
    reportdata.h
    reportcontroller.h/.cpp
    reportrenderer.h/.cpp        the one Qt::Charts + Qt::Widgets + QXlsx exception (§7.1)
    brandcolormath.h
    brandthemedata.h
    brandtheme.h/.cpp
    brandingcontroller.h/.cpp
    rfidscandetector.h/.cpp      pure state machine; RfidKeyboardFilter stays out (§7.1)
  quick/                         NEW: LOAMS 2.0 Qt Quick app
    main.cpp                     entry point; registers Navigator, ViewModels, models to QML
    viewmodels/                  QObject ViewModels — one per screen/module (Section 10)
      KioskViewModel.h/.cpp
      DashboardViewModel.h/.cpp
      StudentsViewModel.h/.cpp
      VisitLogsViewModel.h/.cpp
      ReportsViewModel.h/.cpp
      SettingsViewModel.h/.cpp
      ThemeViewModel.h/.cpp
      Navigator.h/.cpp
    models/                      QAbstractListModel wrappers
      StudentListModel.h/.cpp
      VisitLogModel.h/.cpp       backs both the kiosk live feed and the admin Visit Logs table (§10)
      ReportRowModel.h/.cpp
    qml/
      AppShell.qml               window, surface switching, navigation host (Navigator-bound)
      kiosk/
        Login.qml
        LiveFeed.qml
        Guest.qml
      admin/
        Dashboard.qml
        Database.qml
        Reporting.qml
        Search.qml
        VisitLogs.qml
        Settings.qml
      components/                design-system library (Section 11)
        LButton.qml, LCard.qml, LTable.qml, LSegmented.qml,
        LSideNav.qml, LToast.qml, LDialog.qml, ...
      theme/
        Theme.qml                 singleton, backed by ThemeViewModel (Section 13)
        qmldir
    tests/                       Qt Quick Test targets (tst_qml_*) — additive, not part of the 12-target baseline
  tests/                         existing 12 ctest targets (Section 2.7) — unchanged shape, core/ paths updated
  # legacy Widgets sources — build WITS until Phase 6, gated by -DBUILD_LEGACY_WIDGETS=ON
  main.cpp
  mainwindow.h/.cpp/.ui
  adminwindow.h/.cpp/.ui
  guestwindow.h/.cpp/.ui
  attachfilesdialog.h/.cpp, attachFilesDialog.ui
  busyindicator.h/.cpp
  rfidkeyboardfilter.h/.cpp      re-authored for Quick, not moved (§7.1)
  theme.h                        legacy QPalette/QSS constants — superseded, not ported (see below)
  resources.qrc, resources/wits.qss
  libs/QXlsx/                    vendored, out of review scope per project instructions (unchanged)
```

One placement decision needs its own note because it looks inconsistent at
first glance: `theme.h` (`qt-app/theme.h`) is, at the API level, genuinely
Widgets-free — `lightPalette()` returns a `QPalette` (`QtGui`, not
`QtWidgets`) and `loadStyleSheet()` (`theme.h:83-90`) just reads a file into
a `QString`, so by the "Widgets-free means it can move" test alone it would
qualify for `core/`. It stays with the legacy sources instead, because its
*purpose* — feeding `.ui`/`.qss` styling (`theme.h:29-61,65-79`) — has no
meaning inside a Quick scene tree; Section 13 defines `qml/theme/Theme.qml`
+ `ThemeViewModel` as `theme.h`'s replacement, not its port, so carrying
`theme.h` into `core/` would create a second, unused copy of constants
`Theme.qml` already re-derives from `brandtheme.h`'s fallback palette.
`resources/wits.qss` and `resources.qrc` follow the same reasoning and stay
legacy-only.

The existing `qt-app/tests/` baseline (Section 2.7's 12-target table) keeps
its exact target list and per-target shape — every target's `qt_add_executable`
call still lists its class-under-test `.cpp` directly (Section 7.6);
the only diff per target is a path prefix, e.g.
`${CMAKE_SOURCE_DIR}/reportcontroller.cpp` becomes
`${CMAKE_SOURCE_DIR}/core/reportcontroller.cpp`. The new `quick/tests/`
Qt Quick Test targets are additive — they do not replace or absorb any of
the 12 (spec §8, "New layers: ... Qt Quick Test for components/screens").

## 9. Feature / module organization

Each in-scope module — kiosk, admin dashboard, students/database, reports,
search, visit logs, settings, plus theme as cross-cutting connective
tissue — is organized as a **ViewModel + `qml/` module + core-service
triple**, per spec §4. The table below is the concrete mapping from
Section 2.2's existing controllers to the canonical ViewModel names this
document fixes for Sections 11-20:

| Module | Owning ViewModel | QML module | Core service(s) it owns | Existing controller mapping |
|---|---|---|---|---|
| Kiosk (login, live feed, guest) | `KioskViewModel` | `qml/kiosk/{Login,LiveFeed,Guest}.qml` | net-new `AuthController` (Phase 2) — today's login network calls are ad hoc, not a controller (`MainWindow::handleLogin`/`handleRfidLogin`, `mainwindow.cpp:180-235,299-342`, Section 5.1); `RfidScanDetector` reused verbatim (`rfidscandetector.h:20`) | *none today* — this is the one module with no existing controller to reuse |
| Admin dashboard | `DashboardViewModel` | `qml/admin/Dashboard.qml` | `ReportController` | `reportcontroller.h` — net-new screen over existing data (Section 5.4) |
| Database/students | `StudentsViewModel` | `qml/admin/Database.qml` | `StudentController` + `ImportController` | `studentcontroller.h` + `importcontroller.h` |
| Search | `StudentsViewModel` (shared class, search-only instance) | `qml/admin/Search.qml` | `StudentController` | `studentcontroller.h:33-35,44,47` |
| Reporting | `ReportsViewModel` | `qml/admin/Reporting.qml` | `ReportController` + `ReportRenderer` | `reportcontroller.h` + `reportrenderer.h` |
| Visit logs (+ guest log) | `VisitLogsViewModel` | `qml/admin/VisitLogs.qml` | `VisitorController` today; net-new student-attendance service (Section 16/17) | `visitorcontroller.h` — see the tension note under Section 10's worked example |
| Settings | `SettingsViewModel` | `qml/admin/Settings.qml` | `SettingsController` | `settingscontroller.h` |
| Theme (cross-cutting, not a screen) | `ThemeViewModel` | `qml/theme/Theme.qml` | `BrandTheme` free functions + `BrandingController` | `brandtheme.h` + `brandingcontroller.h` |

`Navigator` (Section 7.3) sits outside this table by design — it has no
owned controller and is not itself a screen; it is the mechanism the table's
QML modules register against, not a triple.

**Search and Database/students share one ViewModel class, not two.** Both
screens ultimately call `StudentController::searchStudents`/
`loadDepartments`/`loadCourses` (`studentcontroller.h:33-35,44,47`); the
Database instance additionally owns an `ImportController` for the
bulk-import path (Section 5.6), while the Search instance is a lighter,
search-only `StudentsViewModel`. This keeps the canonical ViewModel list
exact (no separate `SearchViewModel` is introduced) while still giving each
screen its own ViewModel *instance* and independent network lifecycle.

**The forward-seam contract is concrete, not aspirational.** Today's sidebar
is five hand-wired `connect(ui->xBtn, &QPushButton::clicked, ...)` blocks
(`adminwindow.cpp:292-311`, Section 2.5) — adding a sixth page today means
adding a sixth hand-wired block. Under the triple pattern, `Navigator`
(Section 7.3) exposes a data-driven list of registered pages that
`AppShell.qml`'s sidebar repeats over; adding a module means adding one more
triple (a ViewModel class, a `qml/admin/<Module>.qml` file, and a
core-service) plus one more entry in that list — never a new hand-wired
`connect()`. This is the literal mechanism behind spec §4's "nothing in 2.0
may assume the admin sidebar is a closed set," and it is exactly the seam
Section 5.10 requires for the confirmed-absent-today modules: `inventory`,
`borrowreturn`, and `aiassistant` each plug in later as one more
viewmodel + `qml/`-module + core-service triple, with no change to
`Navigator`, `AppShell.qml`, or any existing triple required to add them.

**Department-list duplication (Section 3.6) does not fully resolve under
this mapping, and that is stated plainly rather than hidden.** Today three
independent code paths fetch `get_departments.php`
(`adminWindow::loadDepartments` at `adminwindow.cpp:1365-1399`,
`ReportController::loadDepartments()`, `StudentController::loadDepartments()`).
Under the table above, `adminWindow::loadDepartments`'s hand-rolled fetch has
no ViewModel — it disappears entirely once `adminWindow` itself is deleted
in Phase 6 — but `DashboardViewModel`/`ReportsViewModel` (via
`ReportController`) and `StudentsViewModel` (via `StudentController`) each
legitimately still own their own controller, so the duplication shrinks from
three call sites to two, not one. A single shared department-list core
service is a plausible further consolidation but is not mandated by this
Phase 0 architecture — Section 3.6 already frames it as "a concrete case
Section 9's module organization should account for, not a bug to fix in
Phase 0," and this table is that accounting.

## 10. ViewModel architecture

For each named ViewModel: which controller(s) it owns, the
`Q_PROPERTY`/`Q_INVOKABLE`/signal surface it exposes to QML, and which
`QAbstractListModel` (if any) it produces.

### 10.1 `KioskViewModel`

- **Owns:** a net-new `AuthController` (Phase 2 — Section 9 already flags
  that no controller exists for login today) wrapping
  `student_login.php`/`admin_login.php`/`rfid_login.php`, currently ad hoc
  `QNetworkAccessManager` calls inside `MainWindow::handleLogin()`
  (`mainwindow.cpp:180-235`) and `MainWindow::handleRfidLogin()`
  (`mainwindow.cpp:299-342`); plus `RfidScanDetector` (`rfidscandetector.h:20`),
  reused verbatim per spec §4.
- **Surface:** `Q_PROPERTY(QString loginInput ...)`,
  `Q_PROPERTY(bool isAdminMode ...)` (an explicit affordance replacing the
  silent password-echo flip at `mainwindow.cpp:144-154`, Section 5.1's
  friction point), `Q_PROPERTY(QString statusMessage ...)` (feeding the
  design's inline status affordance in place of the current
  `QMessageBox::warning`, `mainwindow.cpp:184,232`), `Q_PROPERTY(bool
  guestLoginEnabled ...)` (mirrors `adminWin`'s `guestLoginToggled` signal,
  `mainwindow.cpp:83-85,91`); `Q_INVOKABLE void submitLogin(const QString
  &input)`, `Q_INVOKABLE void requestGuestLogin()`; signals
  `loginSucceeded(...)`/`loginFailed(QString message)`.
- **Model:** `VisitLogModel` (own instance) backing the live attendance
  feed — replacing the five parallel, hand-named `QList<QLabel*>` arrays
  capped at 9 (`MainWindow::refreshRightPanel`, `mainwindow.cpp:366-403`,
  Section 5.2) with an unbounded/scrollable list model populated as each
  login succeeds, the same conceptual source as today's in-memory
  `recentLogins` (`mainwindow.cpp:272-274`) rebuilt on a real model instead
  of named widgets.

### 10.2 `DashboardViewModel`

- **Owns:** `ReportController` (`reportcontroller.h`) for the existing
  department/year/course lists and report-row data, plus
  `ReportRenderer::aggregateVisitsByCourse`/`aggregateVisitsByCourseHour`
  (`reportrenderer.h:25-27`) for the hourly/department bar tiles — Section
  5.4 already confirms these numbers exist in the backend via
  `ReportController` even though no screen surfaces them today.
- **Surface:** `Q_PROPERTY(int visitorsToday ...)`, `Q_PROPERTY(int
  visitorsThisWeek ...)`, `Q_PROPERTY(int registeredStudents ...)`,
  `Q_PROPERTY(QString peakHour ...)`, `Q_PROPERTY(QVariantList hourlyBars
  ...)`, `Q_PROPERTY(QVariantList departmentBars ...)`;
  `Q_INVOKABLE void refresh()`; signals `dataReady()`/`loadFailed(QString)`.
- **Model:** none required — the bar-tile data is small, fixed-shape
  aggregate data, exposed as `QVariantList` properties rather than a list
  model (a list model is reserved for genuinely scrolling record sets, per
  Sections 10.1/10.3/10.4).

### 10.3 `StudentsViewModel`

- **Owns:** `StudentController` (`searchStudents`/`bulkUpdateStudents`/
  `deleteStudents`/`loadDepartments`/`loadCourses`,
  `studentcontroller.h:33-47`); the Database-page instance additionally owns
  `ImportController` (`checkDuplicates`/`uploadStudents`/`parseCsv`/
  `parseExcel`, `importcontroller.h:21,26,29,32`) for the bulk-import path
  (Section 5.6); the Search-page instance omits it (Section 9).
- **Surface:** `Q_PROPERTY(QString searchTerm ...)`, `Q_PROPERTY(QString
  departmentFilter ...)`, `Q_PROPERTY(QString courseFilter ...)`,
  `Q_PROPERTY(bool isImporting ...)` (Database instance only — Section 5.6's
  "am I previewing an import" state, tracked explicitly instead of inferred
  from which handler last ran); `Q_INVOKABLE void search()`,
  `Q_INVOKABLE void bulkUpdate(...)`, `Q_INVOKABLE void
  deleteSelected(QStringList schoolIds)`, `Q_INVOKABLE void
  importFile(QString path)`; signals mirror the controller's
  (`searchFinished`/`searchFailed`/`bulkUpdateFinished`/`bulkUpdateFailed`/
  `deleteFinished`/`deleteFailed`, `studentcontroller.h:50-63`) translated
  into property updates + a `QML`-facing `errorOccurred(QString)`.
- **Model:** `StudentListModel` (`QAbstractListModel` wrapping
  `QList<StudentRecord>`; role names track `StudentRecord`'s fields —
  `code`, `schoolId`, `name`, `course`, `department`, `yearLevel`, `gender`,
  `status`, `studentdata.h:13-20`).

### 10.4 `VisitLogsViewModel` — worked example

`VisitLogsViewModel` wraps `VisitorController` end-to-end:

- **Owns:** `VisitorController` (`qt-app/visitorcontroller.h`) — the real
  fetch/filter/export surface: `void fetchVisitors(const VisitorFilter
  &filter)` (async, result via signals, `visitorcontroller.h:19`);
  `bool exportToExcel(const QList<VisitorRecord> &records, const
  VisitorFilter &filter, const QString &schoolName, const QString
  &filePath)` (synchronous `QXlsx` export, `visitorcontroller.h:22-25`);
  the pure statics `static QString wireDateType(DateFilterType t)`,
  `static QPair<QString, QString> monthRange(int month, int year)`,
  `static QString defaultExportFileName(const VisitorFilter &f)`
  (`visitorcontroller.h:28-30`); and the signals `void
  visitorsLoaded(const QList<VisitorRecord> &records, int totalCount)` /
  `void fetchError(const QString &title, const QString &message)`
  (`visitorcontroller.h:37-38`).
- **Surface:** `Q_PROPERTY(RangeMode rangeMode READ rangeMode WRITE
  setRangeMode NOTIFY rangeModeChanged)` — a two-value
  `enum class RangeMode { Today, ThisWeek }` implementing the design's
  Today/This-Week segmented control (`Admin Dashboard.dc.html:232-236`,
  Section 4.4). `VisitorFilter`'s `DateFilterType`
  (`All`/`SpecificDay`/`Month`/`DateRange`, `visitordata.h:5`) has no native
  "this week" case, so the mapping happens *inside the ViewModel*, not in
  the controller: `Today` sets `DateFilterType::SpecificDay` with
  `startDate` = today; `ThisWeek` sets `DateFilterType::DateRange` with
  `startDate`/`endDate` spanning the current ISO week. `VisitorController`'s
  own surface is untouched — the spec's "moved, not rewritten" contract
  holds even for this redesigned control. Also: `Q_PROPERTY(bool isLoading
  ...)`, `Q_PROPERTY(QString searchTerm ...)` (backs `VisitorFilter::search`,
  `visitordata.h:18`); `Q_INVOKABLE void refresh()`,
  `Q_INVOKABLE QString suggestedExportFileName()` (delegates to
  `VisitorController::defaultExportFileName`, `visitorcontroller.h:30`),
  `Q_INVOKABLE bool exportToExcel(const QString &filePath)` (delegates to
  `VisitorController::exportToExcel`, `visitorcontroller.h:22-25`; the
  `schoolName` argument is read from `SettingsViewModel`, a small
  cross-ViewModel dependency flagged here rather than hidden).
- **Model:** `VisitLogModel` (`QAbstractListModel` wrapping
  `QList<VisitorRecord>`; role names track `VisitorRecord`'s fields —
  `name`, `company`, `purpose`, `date`, `time`, `visitordata.h:9-14`),
  re-populated from `fetchVisitors`'s `visitorsLoaded` signal.

> ⚠ Decision tension: Section 5.9 already disambiguates two different
> things that share a name — the design's **Visit Logs** page (student
> time-in/time-out history) versus today's **guest log**
> (`visitorPage`/`VisitorController`, guest sign-ins only). This worked
> example wraps `VisitorController` because that is the only one of the two
> that exists as a controller today; until Section 16/17 defines a backend
> surface for genuine student attendance history, `VisitLogsViewModel`'s
> model surfaces guest-log rows only. When that backend surface lands, the
> design's "Visit Logs" sidebar entry is intended to be powered by this same
> `VisitLogsViewModel` with its model source widened or swapped — not a
> second ViewModel — but that widening is Section 16/17's decision to make,
> not this section's to pre-empt.

### 10.5 `ReportsViewModel`

- **Owns:** `ReportController` (`computeDateRange`/`getPalette`/
  `fetchReportRows`/`loadDepartments`/`loadYears`/`loadCourses`,
  `reportcontroller.h:23,34,42,45`) and `ReportRenderer`
  (`makeBarChartImage`/`makePieChartImage`/`makeLineChartImage`/
  `paintReport`/`writeReportToXlsx`, `reportrenderer.h:29-45`).
- **Surface:** `Q_PROPERTY(int durationType ...)`, day/month/semester/
  custom-range properties matching `ReportController::computeDateRange`'s
  parameters (`reportcontroller.h:34-39`), `Q_PROPERTY(QString department
  ...)`, `Q_PROPERTY(QString course ...)`, `Q_PROPERTY(QString palette
  ...)`; `Q_INVOKABLE void generateReport()`, `Q_INVOKABLE bool
  exportPdf(QString path)`, `Q_INVOKABLE bool exportExcel(QString path)`.
  Unlike today's `collectReportFilters` (`adminwindow.cpp:1498-1605`,
  Section 2.4), invalid filter combinations are surfaced as a bindable
  `Q_PROPERTY(QString validationError ...)` rather than one of seven inline
  `QMessageBox::warning` calls — one consistent inline-validation pattern
  instead of seven ad hoc dialogs (Section 5.7's simplification note).
- **Model:** `ReportRowModel` (optional; only needed if the redesigned
  Reporting page keeps a raw-row table view alongside the charts).

### 10.6 `SettingsViewModel`

- **Owns:** `SettingsController` (`load`/`save`/`importLogo`/`importPoster`,
  `settingscontroller.h:12-16`), consolidating both of the two current read
  paths (`SettingsController::load()` and `adminWindow::collectHeaderInfo()`'s
  separate ad hoc `QSettings` read, `adminwindow.cpp:1401-1412`, Section 2.4)
  into the one controller call.
- **Surface:** `Q_PROPERTY(QString schoolName ...)`, `Q_PROPERTY(QString
  address ...)`, `Q_PROPERTY(QString logoPath ...)`, `Q_PROPERTY(QString
  librarianName ...)`, `Q_PROPERTY(QString librarianPosition ...)`,
  `Q_PROPERTY(int openHour ...)`, `Q_PROPERTY(int closeHour ...)`;
  `Q_INVOKABLE bool save()`, `Q_INVOKABLE void importLogo(QString path)`,
  `Q_INVOKABLE void importPoster(QString path)`. On a successful
  `importLogo`, `SettingsViewModel` does not call `BrandTheme` itself — it
  invokes `ThemeViewModel::regenerateFromImportedLogo(path)` (Section 10.7),
  keeping all `BrandTheme` calls behind one ViewModel.
- **Model:** none — a single-record settings form.

### 10.7 `ThemeViewModel`

Confirms the brief's requirement directly: `ThemeViewModel` wraps the
free-function brand engine (`BrandTheme::current()`/`setCurrent()`,
`brandtheme.h:67-68`) **without modifying `BrandTheme`** — no new function
is added to `brandtheme.h`; `ThemeViewModel` only calls the engine's
existing public API and emits its own `changed()` signal so QML property
bindings re-evaluate, since `BrandTheme` is deliberately not a `QObject`
(Section 2.3) and therefore has no signal of its own to relay.

- **Owns:** `BrandingController` (`fetchRemoteConfig`/`saveBranding`,
  `brandingcontroller.h:23,27`) for the cache-first startup sync — today
  performed inline in `MainWindow` (`BrandingController` constructed
  `mainwindow.cpp:163`; cached palette applied synchronously then
  re-applied if newer, `mainwindow.cpp:159-172`); this logic moves into
  `ThemeViewModel` so both surfaces (kiosk and admin) share one theme
  source instead of kiosk-only startup wiring.
- **Surface:** one `Q_PROPERTY` per `BrandPalette` role
  (`brandthemedata.h:19-40`) — `adminPrimary`, `adminPrimaryHover`,
  `adminOnPrimary`, `adminPrimarySoft`, `kioskPrimary`,
  `kioskPrimaryHover`, `kioskOnPrimary`, `kioskPrimarySoft`, `secondary`,
  `sidebarBase`, `card`, `appBackground`, `border`, `text`, `mutedText`,
  `success`, `error` — each `NOTIFY changed`; `Q_PROPERTY(ThemeMode mode
  ...)` (`brandthemedata.h:13`); `Q_INVOKABLE void
  regenerateFromImportedLogo(QString path)` (calls
  `BrandTheme::regenerateFromLogo(config, path, &err)` then, on success,
  `BrandTheme::setCurrent(config.palette)` and emits `changed()` — the
  Manual-mode hook, `return false` with a cleared error when
  `config.mode == ThemeMode::Manual`, is unchanged, Section 2.3); signal
  `void changed()`.
- **Model:** none.

### 10.8 Threading rule and the no-live-network test rule

**Threading:** no ViewModel blocks the UI thread. Every controller's network
call is already asynchronous (`QNetworkAccessManager` + signals — the
pattern Section 2.2 documents across all five controllers); every ViewModel
invokes the call and returns immediately, flips an `isLoading`-style
property, and updates its `Q_PROPERTY`s / model rows only from the
connected `*Finished`/`*Loaded`/`*Error` signal handler. The one exception
already in the codebase, `ImportController::parseExcel`
(synchronous `QXlsx` parse, `importcontroller.h:26`), stays synchronous at
the controller level (it always has been) but is called from
`StudentsViewModel` only in response to a direct user action (file picked),
never on a path that blocks a already-rendered frame.

**No-live-network test rule (house rule, spec §8):** every ViewModel unit
test feeds synthetic payloads — either constructing model/record structs
directly or running a captured `QByteArray` through the controller's own
pure parser (e.g. `VisitorController::parseVisitorsResponse`,
`visitorcontroller.h:31-34`) — never a live `QNetworkAccessManager` request.
This mirrors the existing `tst_visitorcontroller`/`tst_studentcontroller`/
`tst_reportcontroller` convention (Section 2.7's table) exactly; ViewModel
tests add one more layer on top (asserting the `Q_PROPERTY`/model updates a
synthetic signal emission produces) without changing how the underlying
controller itself is tested.

## 11. QML component library

Ten reusable primitives live in `qml/components/` (§8's tree) and implement
every control the design references need exactly once. **The standing
rule: screens compose these components and never restyle primitives
locally** — no per-screen `Rectangle { color: "#..." }` chrome, no one-off
`border.width`/`radius` override on a specific page's card. A new visual
need is solved by adding a property to the primitive (or, rarely, adding an
eleventh primitive reviewed the same way) — never a local style override.
This is the QML-era version of the same discipline Section 12.8 states for
color literals, and it is what makes Section 18's screen-by-screen work
composition rather than styling. The names below are fixed for this
document and are reused verbatim by Section 18 (Task 6); no screen invents
a new top-level component name outside this list without amending this
section.

Every property listed is a `Q_PROPERTY`/QML `property` on the component
itself, not on a screen; every `Theme` reference is to Section 12/13's
token surface; every `Accessible.role` is the nearest safe
`QAccessible::Role` value — a couple of components note where the exact
enum name should be re-verified against the Qt 6.11 Quick Controls
`Accessible` attached-property surface at implementation time rather than
assumed from this document alone.

### `LButton`

- **Purpose:** the one clickable-action primitive — brand-fill primary CTA,
  gold "accent" CTA, outlined/ghost secondary, small row actions (EDIT/
  DELETE-style), replacing every ad hoc `QPushButton` stylesheet rule today
  (Section 4.3).
- **Key properties:** `variant` (`Primary | Accent | Outline | Danger |
  Ghost`), `text`, `icon`, `enabled`, `busy` (spinner state for an in-flight
  `Q_INVOKABLE` call), `compact` (row-action sizing).
- **Theme tokens:** `Theme.brand.admin`/`Theme.brand.kiosk` (surface-aware
  `Primary` fill), `Theme.secondary` (`Accent`), `Theme.error`/
  `Theme.errorSoft` (`Danger`), `Theme.radius.md`/`Theme.radius.sm2`,
  `Theme.typography.control`, `Theme.elevation.ctaGold`/`ctaDark`,
  `Theme.motion.hoverLift`/`Theme.motion.press` (translateY(-2px) hover,
  scale(0.97) active — `Admin Dashboard.dc.html:136-137`,
  `Library Kiosk v2.dc.html:48`).
- **`Accessible.role`:** `Accessible.Button`; `Accessible.name` bound to
  `text` (or an explicit `accessibleName` override when `text` is empty,
  e.g. an icon-only compact action).

### `LCard`

- **Purpose:** the one bordered-container primitive — every stat-tile
  substrate, dashboard/report/database panel, and list-section chrome;
  replaces native `QGroupBox` chrome entirely (Section 4.3's "component
  gap, not a color gap" finding).
- **Key properties:** `padding` (defaults from `Theme.spacing`), `radius`
  (`Theme.radius.card` = 16 by default, `Theme.radius.hero` = 18 via
  `variant: "hero"`), `filled` (switches from the neutral `card`/`border`
  surface to a brand-gradient fill for hero/stat cards), `elevated` (bool,
  toggles the hover-lift shadow on/off per-instance).
- **Theme tokens:** `Theme.card`, `Theme.border`, `Theme.brand.admin`/
  `Theme.brand.adminHover` (hero-fill gradient), `Theme.elevation.resting`/
  `hover`/`heroFill`/`reportHover`.
- **`Accessible.role`:** `Accessible.Grouping` for an in-page card;
  `Accessible.Pane` when a card is a whole scrollable page region (e.g. the
  Database/Visit Logs table's outer frame).

### `LTable`

- **Purpose:** the row-grid primitive backing Database, Search, Visit Logs,
  and the guest log — header row with the design's zebra tint, hairline
  row separators, per-row entry animation, row-hover tint.
- **Key properties:** `columns` (list of `{title, weight, align}` column
  defs), `model` (one of Section 10's `QAbstractListModel`s —
  `StudentListModel`, `VisitLogModel`, `ReportRowModel`), `rowDelegate`
  (optional per-column override), `selectable`, `emptyStateText`.
- **Theme tokens:** `Theme.card`, `Theme.tableHeaderBg`, `Theme.rowHairline`,
  `Theme.text`, `Theme.mutedText`, `Theme.typography.eyebrow` (header
  labels), `Theme.motion.rowIn` (Section 15).
- **`Accessible.role`:** `Accessible.Table` on the component root; per-row
  `Accessible.Row`, per-cell `Accessible.Cell`. Keyboard: arrow-key row
  focus and Enter-to-activate, centralized per Section 14's "table focus"
  shortcut requirement.

### `LSegmented`

- **Purpose:** the N-way segmented toggle — concretely, the design's
  Today/This-Week visit-log control
  (`Admin Dashboard.dc.html:232-236`, `logRanges`) that
  `VisitLogsViewModel.rangeMode` (Section 10.4) binds to directly.
- **Key properties:** `options` (list of `{value, label}`), `currentValue`
  (bound two-way, e.g. to `RangeMode`), `onSelectionChanged` signal.
- **Theme tokens:** `Theme.card`/`Theme.border` (track), `Theme.brand.admin`
  (selected-pill fill/text, mirroring `lr.bg`/`lr.fg`'s binding pattern in
  the reference), `Theme.radius.sm2`.
- **`Accessible.role`:** `Accessible.PageTabList` on the track;
  `Accessible.PageTab` per option; left/right arrow-key cycling between
  options.

### `LSideNav`

- **Purpose:** the persistent left sidebar shared by `AppShell.qml`'s admin
  surface and the kiosk brand panel — brand-gradient background, nav item
  list, active-item highlight, admin-avatar footer (admin only).
- **Key properties:** `items` (bound to `Navigator`'s registered-page list,
  Section 14 — the data-driven replacement for today's five hand-wired
  `connect()` blocks, `adminwindow.cpp:292-311`, Section 2.5/9),
  `currentPage` (two-way bound to `Navigator.currentPage`), `footer`
  (optional admin-avatar slot, kiosk instance omits it).
- **Theme tokens:** `Theme.brand.admin`/`Theme.brand.adminHover` (gradient
  background — Section 12.5 already establishes this component reads the
  brand roles directly, not `Theme.sidebarBase`), `Theme.secondary`
  (active-item dot/accent), `Theme.brand.onPrimary` (`#FFF6E8`-class text,
  not raw white).
- **`Accessible.role`:** `Accessible.List` on the nav; `Accessible.ListItem`
  per entry; Up/Down arrow-key cycling centralized in `Navigator`
  (Section 14), not re-implemented per instance.

### `LToast`

- **Purpose:** the one inline, non-blocking status surface — replaces
  every validation-only `QMessageBox` this document has flagged as an
  interruption for what should be inline feedback: kiosk login failures
  (`mainwindow.cpp:184,232`, Section 5.1), three of the guest form's five
  dialogs (`guestwindow.cpp:36,54,64`, Section 5.3), and the reporting
  page's seven inline `QMessageBox::warning` calls
  (`adminwindow.cpp:1498-1605`, Section 5.7). `MainWindow::showKioskStatus`
  (`mainwindow.cpp:278-297`) is the direct current-state precedent — it
  already exists for RFID failures; `LToast` is its generalized QML
  successor, used consistently rather than only for one failure path.
- **Key properties:** `message`, `severity` (`Info | Success | Warning |
  Error`), `autoDismissMs` (defaults from `Theme.motion.toastIn`/`toastOut`,
  Section 15), `visible`.
- **Theme tokens:** `Theme.success`, `Theme.error`/`Theme.errorSoft`,
  `Theme.card`/`Theme.border`, `Theme.motion.toastIn`/`toastOut`.
- **`Accessible.role`:** `Accessible.AlertMessage` — assertive live-region
  semantics so a screen reader announces it without stealing keyboard
  focus from whatever the user was doing.

### `LDialog`

- **Purpose:** the only modal-interruption primitive 2.0 keeps — reserved
  for genuine yes/no confirmations and real network-outcome messages (the
  two guest-flow dialogs Section 5.3 says should survive:
  `guestwindow.cpp:70,73`), never for field-level validation (that is
  `LToast`'s job — see Section 5.3's "removes 3 of the 5 modal dialogs...
  without touching the two genuine network-outcome dialogs").
- **Key properties:** `title`, `message`, `buttons` (list of `{label,
  role}`), `visible`.
- **Theme tokens:** `Theme.card`, `Theme.border`,
  `Theme.elevation.modal` (Section 12.7 — the one extrapolated elevation
  tier), a semi-opaque scrim.
- **`Accessible.role`:** `Accessible.Dialog`; traps focus while open and
  returns it to the invoking control on close, coordinated with
  `Navigator.activeModal` (Section 14) so only one modal can be open
  application-wide.

### `LStatTile`

- **Purpose:** the dashboard's four "General" stat cards (visitors today/
  week, registered students, peak hour — `Admin Dashboard.dc.html:74-95`)
  and the kiosk's two stat cards (visitors today, this hour —
  `Library Kiosk v2.dc.html:97-105`) — a label + value + caption card in
  either the neutral bordered variant or the brand-gradient "hero" variant
  used for the first/primary stat in each set.
- **Key properties:** `label`, `value`, `caption`, `variant` (`Neutral |
  Hero`), `valueFormat` (tabular-nums numeric display, per
  `font-variant-numeric:tabular-nums` throughout both references).
- **Theme tokens:** `Theme.card`/`Theme.border` (`Neutral`),
  `Theme.brand.admin`/`Theme.brand.kiosk` + `Theme.secondary` (label color,
  `Hero`), `Theme.mutedTextCaption` (the second, lighter muted tone,
  Section 12.1), `Theme.typography.statValue`, `Theme.typography.eyebrow`
  (label).
- **`Accessible.role`:** `Accessible.Grouping` around the whole tile (label
  read before value); inner text as `Accessible.StaticText`.

### `LBarChart`

- **Purpose:** the dashboard's two custom (non-`QtCharts`) bar
  visualizations — vertical hourly bars with a grow-in
  (`Admin Dashboard.dc.html:104-111`) and horizontal department bars with a
  width-fill transition (`Admin Dashboard.dc.html:116-124`) — as **one**
  parameterized primitive (`orientation: Vertical | Horizontal`) rather
  than two components, per the spec's Risk #2 guidance to build the
  dashboard's simple bars as custom QML and reserve `QtCharts` for
  `reportrenderer`'s export path only (spec §10 Risk 2; this document's
  Section 3.9/7.1 already accept that `reportrenderer` alone carries the
  `Qt::Charts`/`Qt::Widgets` dependency).
- **Key properties:** `orientation`, `model` (list of `{label, value,
  color}` — fed by `DashboardViewModel.hourlyBars`/`departmentBars`,
  Section 10.2's `QVariantList` properties), `maxValue`, `barRadius`.
- **Theme tokens:** deliberately *not* a per-bar color — the ViewModel
  supplies each bar's `color` (matching the reference's `{{ b.color }}`/
  `{{ d.color }}` per-item binding), so the component only supplies
  structural tokens: `Theme.rowHairline` (department-bar track color),
  `Theme.radius.xs` (hourly-bar cap radius), `Theme.motion.barGrow`/
  `deptBarFill` (Section 15).
- **`Accessible.role`:** the chart container as `Accessible.Pane`; each bar
  exposes an `Accessible.StaticText` fallback with a numeric description
  (label + value), since the bars are otherwise a purely visual encoding —
  verify the exact per-bar role against the Qt Quick Controls `Accessible`
  surface in use at implementation time rather than assuming a dedicated
  chart-element role exists.

### `LPageHeader`

- **Purpose:** the admin page-header region — title, subtitle, live clock
  (`Admin Dashboard.dc.html:63-69`) — as a first-class component, replacing
  `adminWindow::buildHeaderBar()`'s runtime C++ construction and its
  `ui->stackedWidget` re-parenting trick (`adminwindow.cpp:85-127`,
  Section 4.3's "bolted onto a `.ui` layout that was not designed to have
  one" finding).
- **Key properties:** `title`, `subtitle` (per-page, e.g.
  `Admin Dashboard.dc.html:281-287`'s per-page subtitle map), `clockText`
  (ticked by a shared clock source, not re-implemented per page), `actions`
  (optional right-aligned slot — e.g. Visit Logs' EXPORT CSV/PRINT REPORT
  buttons, `Admin Dashboard.dc.html:238-239`).
- **Theme tokens:** `Theme.typography.pageTitle`, `Theme.mutedText`
  (subtitle/clock date), `Theme.brand.admin` (clock time accent color, per
  `Admin Dashboard.dc.html:68`'s `color:var(--brand)` on the clock time).
- **`Accessible.role`:** title and subtitle/clock as `Accessible.StaticText`
  — heading semantics are conveyed through the `Theme.typography.pageTitle`
  token (visual weight/size), not a dedicated accessible heading role,
  since `QAccessible::Role` has no universally-available "Heading" value to
  rely on across Qt Quick Controls versions without checking the exact Qt
  6.11 surface first.

Smaller design elements the references also use — avatar/initial circles,
filter chips, the "✓ LOGGED IN" status pill — are explicitly **not** new
top-level primitives; they are compositions of `LCard`/`LButton`/
`LSegmented` (e.g. a pill badge is an `LButton`-shaped, non-interactive
`Ghost` variant, or a small `Rectangle` inside `LStatTile`/`LCard` bound to
the same `Theme.radius.pill`/`Theme.secondarySoft` tokens). Keeping the
catalog closed to these ten names is what lets Section 18 (Task 6) reuse
them verbatim without inventing an eleventh name mid-redesign.

## 12. Design system

This section catalogs the token set the spec's §5 already commits to
("Tokens from the Claude Design files... derivations use the shipped
`brandcolormath.h` rules... a QML `Theme` singleton... is the single source
of every visual property"), sourced directly from the two built-out
references (`Admin Dashboard.dc.html`, `Library Kiosk v2.dc.html` — both
light-theme only, Section 4.1), and states it as the `Theme` singleton's
property surface (the singleton's mechanics, brand-engine wiring, and
light/dark behavior are Section 13's job; this section is the token catalog
plus the one open decision Section 4.3 deferred here).

### 12.1 Color tokens

| `Theme` property | Light value | Source | `BrandPalette` field | Derivation |
|---|---|---|---|---|
| `Theme.brand.admin` / `Theme.brand.kiosk` | `#7E1A15` | `Admin Dashboard.dc.html:14`, `Library Kiosk v2.dc.html:14` (`--brand`) | `adminPrimary` / `kioskPrimary` | literal (shipped default — see 12.4) |
| `Theme.brand.adminHover` / `Theme.brand.kioskHover` | `#5E0E0B` | `Admin Dashboard.dc.html:14,314` (`--brand-deep`, `this.shade(a, -0.28)`) | `adminPrimaryHover` / `kioskPrimaryHover` | `BrandColorMath::shade(brand, -0.28)` (`brandcolormath.h:39-45`) |
| `Theme.brand.adminSoft` / `Theme.brand.kioskSoft` | `#F6E9E7` | `Admin Dashboard.dc.html:14,315` (`--brand-soft`, `this.mix(a, '#FFFFFF', 0.9)`) | `adminPrimarySoft` / `kioskPrimarySoft` | `BrandColorMath::mix(brand, white, 0.90)` (`brandcolormath.h:49-56`) |
| `Theme.brand.onPrimary` | `#FFF6E8` | `Admin Dashboard.dc.html:30`, `Library Kiosk v2.dc.html:30` (sidebar text color) | `adminOnPrimary` / `kioskOnPrimary` | literal — see 12.4's note on the divergence from `extractPalette()`'s pure-white on-color |
| `Theme.secondary` (accent) | `#E8B10E` | `Admin Dashboard.dc.html:14`, `Library Kiosk v2.dc.html:14` (`--gold`) | `secondary` | literal (shipped default) |
| `Theme.secondarySoft` *(extra, no `BrandPalette` field)* | `#FDF3E0` | `Admin Dashboard.dc.html:14,317` (`--gold-soft`, `this.mix(g, '#FFFFFF', 0.88)`) | — | `BrandColorMath::mix(secondary, white, 0.88)` |
| `Theme.appBackground` | `#FBF8F3` | `Admin Dashboard.dc.html:15`, `Library Kiosk v2.dc.html:15` | `appBackground` | literal — a neutral role `extractPalette()` never derives from a logo anyway (`brandtheme.cpp:350-357` copies every neutral role straight from `fallbackPalette()`) |
| `Theme.card` | `#FFFFFF` | e.g. `Admin Dashboard.dc.html:80` | `card` | literal |
| `Theme.border` (2px) | `#EFE5D4` | e.g. `Admin Dashboard.dc.html:80` | `border` | literal |
| `Theme.text` | `#3A2C22` | `Admin Dashboard.dc.html:27`, `Library Kiosk v2.dc.html:27` | `text` | literal |
| `Theme.mutedText` | `#8B7A62` | `Admin Dashboard.dc.html:66,81` | `mutedText` | literal |
| `Theme.mutedTextCaption` *(extra)* | `#B0A08A` | `Library Kiosk v2.dc.html:99` ("VISITORS TODAY" stat-tile eyebrow) | — | literal; a second, lighter muted tone distinct from `mutedText`, reserved for stat-tile captions only (Section 4.1 already flags this second tone) |
| `Theme.tableHeaderBg` *(extra)* | `#F7F1E6` | `Admin Dashboard.dc.html:140`, `Library Kiosk v2.dc.html:110` | — | literal |
| `Theme.rowHairline` *(extra)* | `#F3ECDD` | `Admin Dashboard.dc.html:145` | — | literal |
| `Theme.success` | `#10B981` | *(no design reference exists — carried from `fallbackPalette()`, `brandtheme.cpp:47`)* | `success` | literal — an explicit assumption, not a grounded design decision; flagged for the Phase-5 a11y/dark-mode review round (spec §10 Risk 3) to revisit if a success/confirmation state ships |
| `Theme.error` (text) / `Theme.errorSoft` / `Theme.errorBorder` *(soft/border extra)* | `#A33B34` / `#FDF4F3` / `#F3D9D6` | `Admin Dashboard.dc.html:156` (the DELETE row action) | `error` (text only) | literal; recommended replacement for `fallbackPalette()`'s saturated `#EF4444` (`brandtheme.cpp:48`, `theme.h:78`) in the shipped default, so the error role stays tonally warm/maroon rather than clashing with the rest of the palette — see 12.4 |
| `Theme.sidebarBase` | `#1E293B` (unchanged) | *(legacy — `theme.h:66`)* | `sidebarBase` | not consumed by any Quick component — see 12.5 |

The `shade()`/`mix()` derivations above are not a re-invention of the
design's math: `Admin Dashboard.dc.html:319-328`'s `shade`/`mix` JS bodies
(a channel-wise multiply and linear interpolation) are the same operation
`BrandColorMath::shade`/`BrandColorMath::mix` implement in C++
(`brandcolormath.h:39-56`) — Section 4.1 already established this
independently. `Theme.brand.*Hover`/`*Soft`/`Theme.secondarySoft` are
therefore *computed* properties (derived once from `Theme.brand.admin`/
`Theme.secondary` via the same math the engine already ships), not four
more independently-authored literals to keep in sync by hand.

### 12.2 Spacing scale

The references do not use a perfectly uniform 8pt grid — 14px/18px/22px
recur alongside 16px/24px — so the token scale below rounds to the nearest
of a slightly denser ladder rather than forcing an artificial strict-8pt
system onto values the design actually uses:

`Theme.spacing = { xs: 4, sm: 8, md: 12, base: 14, lg: 16, xl: 18, xl2: 22, xxl: 24, xxxl: 28 }`

Citations: `xs`/4 — `Admin Dashboard.dc.html:77` (`margin-top:4px`);
`sm`/8 — `Admin Dashboard.dc.html:205` (chip padding `8px 14px`); `md`/12 —
`Admin Dashboard.dc.html:135` (input padding `12px 16px`); `base`/14 —
`Admin Dashboard.dc.html:97` (stat-row `gap:14px`); `lg`/16 —
`Admin Dashboard.dc.html:73` (page-level `gap:16px`); `xl`/18 —
`Admin Dashboard.dc.html:75` (stat-tile padding `18px 22px`); `xl2`/22 — the
same stat-tile padding's second value; `xxl`/24 —
`Admin Dashboard.dc.html:198` (search-card `padding:24px`); `xxxl`/28 —
`Library Kiosk v2.dc.html:30` (kiosk brand-panel `gap:28px`).

### 12.3 Radius scale

`Theme.radius = { xs: 6, sm: 8, sm2: 10, md: 12, card: 16, hero: 18, pill: 999, circle: "50%" }`

Citations: `xs`/6 (paired with an inner 3px) —
`Admin Dashboard.dc.html:107` (hourly-bar cap, `border-radius:6px 6px 3px
3px`); `sm`/8 — `Admin Dashboard.dc.html:155-156` (EDIT/DELETE row-action
buttons); `sm2`/10 — `Admin Dashboard.dc.html:175,184` (the GENERATE CTA and
the export-type icon chip); `md`/12 — the most common radius in the
references, e.g. inputs and standard buttons at `Admin Dashboard.dc.html:
135-137,200-201`; `card`/16 — the admin card family, e.g.
`Admin Dashboard.dc.html:75,80,114,139`; `hero`/18 — the kiosk's larger
hero/stat/feed cards, e.g. `Library Kiosk v2.dc.html:76,98,102,109`;
`pill`/999 — filter chips and status badges, e.g.
`Admin Dashboard.dc.html:205`, `Library Kiosk v2.dc.html:82` (the department
progress-bar track uses `border-radius:99px` at `Admin Dashboard.dc.html:
120-121`, functionally the same pill treatment at an 8px track height);
`circle`/50% — avatars/logo roundels, e.g. `Admin Dashboard.dc.html:34,148,
213`. This brackets the spec's stated 8-18px band (spec §5) correctly, with
the pill/circle values named as the explicit exceptions outside that band
rather than silently folded into it.

### 12.4 Resolving Section 4.3's ⚠ Decision tension — the zero-config default palette

> **Resolution.** The zero-config (never-branded) default for a fresh LOAMS
> 2.0 install is the design's **maroon/gold identity** (`#7E1A15`/`#E8B10E`
> and the rest of 12.1's table), not `BrandTheme::fallbackPalette()`'s
> blue/green (`theme.h:65-79`, reproduced in `brandtheme.cpp:26-50`).
> `fallbackPalette()` itself is **not changed** — it keeps its existing job
> exactly as documented in `brandtheme.h:24-27`: the safety net
> `extractPalette()` (`brandtheme.cpp:265-361`) falls back to when a logo
> fails to decode/validate (`renderLogo` failure, `brandtheme.cpp:143-200`)
> or contains no usable chromatic pixels (`brandtheme.cpp:291-294`), and the
> value `loadCachedConfig()` (`brandtheme.h:51`, `brandtheme.cpp:384-404`)
> returns when the local cache has never been populated. Both of those
> remain blue/green, unchanged.
>
> What changes is a layer *above* the engine, not inside it:
> `ThemeViewModel` (Section 10.7/13) — not `BrandTheme` — seeds its own
> initial `Q_PROPERTY` values from a shipped maroon/gold `BrandPalette`
> literal (12.1's table, itself internally consistent via the engine's own
> `shade()`/`mix()` derivations) *before* the cache-first load
> (`mainwindow.cpp:159-172`'s pattern, moved into `ThemeViewModel` per
> Section 10.7) runs, and only defers to `loadCachedConfig()`'s literal
> result — including its blue/green fallback — when the cached
> `BrandingConfig` is genuinely "never branded"
> (`config.updatedAt.isValid() == false`, the exact signal
> `loadCachedConfig()` already produces when its `palette` key is empty,
> `brandtheme.cpp:391-393`). Concretely: if `updatedAt` is invalid,
> `ThemeViewModel` keeps its shipped maroon/gold values (and calls
> `BrandTheme::setCurrent()` with them, so any other code path reading
> `BrandTheme::current()` — e.g. a legacy Widgets consumer during the
> parallel-rebuild window — sees the same shipped identity) instead of
> adopting `config.palette`'s blue/green; if `updatedAt` *is* valid (the
> install has been branded before, even if the backend now reports nothing
> newer), the cached palette is used exactly as today, unchanged. The
> background sync (`BrandingController::fetchRemoteConfig`/
> `remoteConfigLoaded`, `isNewer()`, `brandtheme.h:55`) is untouched either
> way — a real `branding_config` row always wins over the shipped default,
> per `isNewer()`'s existing "a valid remote always beats a never-branded
> local" contract (`brandtheme.h:54`).
>
> **Why this is the right default, not just a possible one:** the design
> references are the visual source of truth for 2.0 (spec §3), both
> built-out files ship the identical maroon/gold `:root` block with no
> alternate branch (Section 4.1), and the three-way legacy theming split
> (Section 2.6) that produced blue/green was never a deliberate visual
> choice for 2.0 — it is `theme.h`'s pre-branding-engine palette, kept only
> because `fallbackPalette()` was built (Phase 1) to *not* change what an
> unbranded install already looked like at the time. 2.0 is a full
> presentation-layer rebuild (spec §1); there is no shipped-look
> continuity to protect for an install that has never imported a logo, so
> nothing forces the new app's out-of-the-box identity to match the old
> Widgets app's out-of-the-box identity. Shipping maroon/gold as the
> zero-config default also means a school that never touches Settings still
> gets the fully-designed, on-brand look the references show, rather than a
> blue/green look that appears nowhere in any design reference.

### 12.5 `sidebarBase` is legacy-only in the Quick app

`BrandPalette::sidebarBase` is a neutral role `extractPalette()` never
derives from a logo (`brandtheme.cpp:350` copies it straight from
`fallbackPalette()` regardless of branding state) — today it backs
`QFrame#sidebarFrame`'s background in `resources/wits.qss:132`. Neither
design reference uses a separate neutral "sidebar" color at all: both
`Admin Dashboard.dc.html:30` and `Library Kiosk v2.dc.html:30` paint the
sidebar with `linear-gradient(180deg, var(--brand) 0%, var(--brand-deep)
0%)` directly — i.e., the brand/hover roles themselves, not a distinct
neutral. `LSideNav` (Section 11) therefore binds its background to
`Theme.brand.admin`/`Theme.brand.adminHover` directly; `Theme.sidebarBase`
stays in the `Theme` singleton only for `ThemeViewModel`/`BrandPalette`
surface completeness (Section 10.7's property list is fixed and this
document does not trim it) but has zero QML consumers in the component
catalog below.

### 12.6 Typography

Both references load the same two families
(`Admin Dashboard.dc.html:12`, `Library Kiosk v2.dc.html:12`:
`Source+Serif+4:...600,700` + `Public+Sans:...400;500;600;700;800`) as app
fonts (bundled, not a system/Google Fonts network fetch, in the Quick app).
`Theme.typography` exposes a fixed size/weight scale rather than raw
per-screen font declarations:

| Token | Family / weight | Size | Citation |
|---|---|---|---|
| `Theme.typography.pageTitle` | Source Serif 4 / 700 | 30px | `Admin Dashboard.dc.html:65` |
| `Theme.typography.heroName` | Source Serif 4 / 700 | 27px | `Library Kiosk v2.dc.html:84` (kiosk "now signed in" name) |
| `Theme.typography.heroPlaceholder` | Source Serif 4 / 700 | 22px | `Library Kiosk v2.dc.html:93` ("Scan your ID to begin") |
| `Theme.typography.brandWordmark` | Source Serif 4 / 700 | 19px | `Admin Dashboard.dc.html:36` ("Library Admin") |
| `Theme.typography.cardTitle` | Source Serif 4 / 600 | 17px | `Admin Dashboard.dc.html:101,115,173,180` (chart/report/section card titles) |
| `Theme.typography.statValue` | Public Sans / 800, tabular-nums | 34-36px | `Admin Dashboard.dc.html:77,82,87,92` (34px); `Library Kiosk v2.dc.html:100,104` (36px) |
| `Theme.typography.body` | Public Sans / 600-700 | 13-14px | e.g. row text, `Admin Dashboard.dc.html:149-153` |
| `Theme.typography.control` | Public Sans / 800 | 12-13px | button/CTA labels, e.g. `Admin Dashboard.dc.html:136,175` |
| `Theme.typography.eyebrow` | Public Sans / 800, letter-spacing ~0.12em | 10-11px | stat-tile/table-header labels, e.g. `Admin Dashboard.dc.html:76,140` |

The Source Serif 4 sizes are not a single uniform "heading" size —
30/27/22/19/17px is a genuine five-step scale across page titles, kiosk
hero text, and card titles, and the catalog above keeps them distinct
rather than collapsing them into one `Theme.typography.heading` value.

### 12.7 Elevation

The references use elevation sparingly and consistently: bordered cards are
flat at rest (no shadow, just the `#EFE5D4` 2px border) and gain a shadow
only on hover or when brand-filled.

| Token | Value | Citation |
|---|---|---|
| `Theme.elevation.resting` | none (border only) | e.g. `Admin Dashboard.dc.html:80` at rest |
| `Theme.elevation.hover` | `0 12px 26px rgba(94,14,11,0.10)` | `Admin Dashboard.dc.html:80,85,90` (`style-hover`), `Library Kiosk v2.dc.html:98,102` |
| `Theme.elevation.heroFill` | `0 10-12px 26-30px rgba(94,14,11,0.22-0.25)` | `Admin Dashboard.dc.html:75` (`0 10px 26px … 0.22`), `Library Kiosk v2.dc.html:76` (`0 12px 30px … 0.25`) |
| `Theme.elevation.reportHover` | `0 14px 30px rgba(94,14,11,0.12)` | `Admin Dashboard.dc.html:171` (reporting card hover) |
| `Theme.elevation.focusRing` | `0 0 0 4px rgba(126,26,21,0.10)` | `Admin Dashboard.dc.html:135,200` (identical value on both input fields) |
| `Theme.elevation.ctaGold` | `0 5px 14px rgba(232,177,14,0.3)` | `Admin Dashboard.dc.html:201` (gold SEARCH CTA) |
| `Theme.elevation.ctaDark` | `0 6px 18px rgba(0,0,0,0.28)` | `Library Kiosk v2.dc.html:48` (kiosk LOG IN CTA) |
| `Theme.elevation.modal` *(extrapolated — no dialog reference exists in either file)* | a distinct, higher tier than `heroFill`, e.g. `0 24px 48px rgba(0,0,0,0.30)` | none — flagged as a Phase-1 human-review item, same caveat as the dark-mode risk (spec §10 Risk 3) |

### 12.8 The grep-audit constraint carries forward

Section 4.3 already established the current-state evidence for why this
rule matters: "a grep for the design's tokens against `qt-app/` returns
zero hits" today, across three independently-maintained color sources
(`theme.h`, `wits.qss`, and ad hoc `.ui` literals). The spec's standing rule
— "the PRD's 'no stray hardcoded colors' grep audit remains enforceable"
(spec §5) — becomes concretely checkable for the Quick app the same way:
every color literal in `qt-app/quick/qml/` should resolve through
`Theme.*`, and a grep for a raw hex literal
(`grep -rnE '#[0-9A-Fa-f]{3,6}' qt-app/quick/qml/`) should only ever match
inside `qml/theme/Theme.qml` itself — any hit inside `qml/components/` or a
screen file is the QML-era equivalent of today's `adminwindow.ui:2225-2236`
inline-hex violation (Section 4.3) and should fail review the same way.

## 13. Theme system

`qml/theme/Theme.qml` (§8's tree) is a `QML_SINGLETON`, and it is the
**only** source of visual properties any component or screen reads —
Section 12's full token catalog (color, spacing, radius, typography,
elevation) plus `Theme.motion.*` (Section 15) and `Theme.mode`. It is
backed 1:1 by `ThemeViewModel` (Section 10.7): every `Theme.*` property is
a thin QML-facing mirror of a `ThemeViewModel` `Q_PROPERTY`, re-evaluated
whenever `ThemeViewModel::changed()` fires. This section does not
introduce a second definition of `ThemeViewModel` — its owned controller,
`Q_PROPERTY` list, `Q_INVOKABLE`s, and `changed()` signal are exactly as
Section 10.7 already fixed them; this section specifies how `Theme.qml`
consumes that surface and the mechanics (brand-engine integration,
light/dark, mode persistence) around it.

### 13.1 Property role catalog

`Theme.qml` exposes, grouped by origin:

- **Brand roles (mirror `BrandPalette` exactly, one per `ThemeViewModel`
  property, Section 10.7):** `Theme.brand.admin`/`adminHover`/
  `adminOnPrimary`/`adminSoft`, `Theme.brand.kiosk`/`kioskHover`/
  `kioskOnPrimary`/`kioskSoft`, `Theme.secondary`, `Theme.sidebarBase`,
  `Theme.card`, `Theme.appBackground`, `Theme.border`, `Theme.text`,
  `Theme.mutedText`, `Theme.success`, `Theme.error`.
- **Extra design tokens layered on top (Section 12.1, no `BrandPalette`
  field — computed once from the brand roles via
  `BrandColorMath::mix`/`shade`, not independently stored):**
  `Theme.secondarySoft`, `Theme.mutedTextCaption`, `Theme.tableHeaderBg`,
  `Theme.rowHairline`, `Theme.errorSoft`, `Theme.errorBorder`.
- **Structural scales (Section 12.2/12.3/12.6/12.7, fixed constants, not
  brand-derived):** `Theme.spacing.*`, `Theme.radius.*`,
  `Theme.typography.*`, `Theme.elevation.*`.
- **Motion (Section 15):** `Theme.motion.*` durations/easing plus
  `Theme.motion.enabled` (the reduce-motion switch).
- **Mode:** `Theme.mode` (`Light | Dark | System`, 13.4) and
  `Theme.isDark` (a resolved bool a component can bind to directly instead
  of comparing `Theme.mode == System` plus an OS query itself).

`Theme.qml` is intentionally a **superset** of `BrandPalette` — the extra
tokens and the structural/motion scales exist only in `Theme`, never in
`BrandPalette` or `ThemeViewModel`'s `Q_PROPERTY` list, so nothing here
requires widening the engine's own data type.

### 13.2 Brand-engine integration

`ThemeViewModel` wraps the shipped brand engine without adding to its
public API (Section 10.7 already states this constraint; this section
describes the runtime flow that constraint produces):

- **Source of truth:** `BrandTheme::current()`/`setCurrent()`
  (`brandtheme.h:67-68`, implemented via a function-local static in
  `brandtheme.cpp:442-458`) hold the process-wide palette exactly as they
  do today; `ThemeViewModel` reads/writes through these two functions only.
- **Cache-first startup, relocated:** today `MainWindow`'s constructor
  applies the cached palette synchronously before any network I/O, then
  re-applies it if the backend later reports something newer
  (`mainwindow.cpp:159-172`: `BrandTheme::setCurrent(BrandTheme::
  loadCachedConfig(brandingStore).palette)` runs first, then
  `BrandingController::remoteConfigLoaded` calls `BrandTheme::isNewer()`
  and re-applies+re-caches only on a strictly-newer remote config). Section
  10.7 already moves this flow into `ThemeViewModel`'s construction path so
  both surfaces (kiosk and admin) share one theme source instead of
  kiosk-only startup wiring; 13.3 below states the one behavior change this
  relocation carries (the zero-config default from Section 12.4).
- **Live re-theme on logo import, no restart:** `SettingsViewModel`
  (Section 10.6) calls `ThemeViewModel::regenerateFromImportedLogo(path)`
  on a successful logo import, which calls
  `BrandTheme::regenerateFromLogo(config, path, &err)`
  (`brandtheme.h:62-63`, implemented `brandtheme.cpp:412-440`) and, on
  success, `BrandTheme::setCurrent(config.palette)` followed by emitting
  `changed()` (Section 10.7). Because every `LButton`/`LCard`/`LSideNav`/…
  instance binds to `Theme.*` properties through ordinary QML property
  bindings rather than reading a value once, Qt Quick's declarative binding
  system re-evaluates every bound expression the moment the underlying
  `Q_PROPERTY` changes — this *is* the "live re-theme via property
  bindings (no restart)" mechanism spec §6 requires; nothing beyond
  emitting `changed()` needs to happen for the whole UI to repaint with the
  new brand.
- **The Manual-mode hook, unchanged:** `regenerateFromLogo` skips
  auto-regeneration entirely and returns `false` with a cleared error when
  `config.mode == ThemeMode::Manual` (`brandtheme.cpp:414-419`) — the exact
  "code hook only, no editor UI" behavior spec §6 locks in for 2.0.
  `ThemeViewModel` does not add a theme-editor UI around this hook; it only
  calls the existing function and surfaces whatever it returns.
- **Persistence, unchanged:** `saveCachedConfig`/`loadCachedConfig`
  (`brandtheme.h:50-51`, `brandtheme.cpp:368-404`) remain the local
  `QSettings`-backed cache under the `"branding"` group
  (`brandtheme.cpp:365`); `BrandingController::fetchRemoteConfig`/
  `saveBranding` (`brandingcontroller.h:23,27`) remain the
  `branding_config`-backend read/write path; `isNewer()`
  (`brandtheme.h:55`, `brandtheme.cpp:406-409`) remains the sole freshness
  rule ("remote wins if strictly newer").

### 13.3 The zero-config default, restated as a mechanism (see Section 12.4 for the *values* and the rationale)

`ThemeViewModel`'s constructor loads the cached `BrandingConfig` via
`loadCachedConfig()` before doing anything else. If
`config.updatedAt.isValid()` is `false` — the exact, already-existing
signal for "never branded" (`brandtheme.cpp:391-393` returns
`fallbackPalette()` precisely when the cache's `palette` key is empty) —
`ThemeViewModel` seeds its own `Q_PROPERTY` values (and calls
`BrandTheme::setCurrent()`) from the shipped maroon/gold literal instead of
adopting `config.palette`'s blue/green. If `updatedAt` is valid, the cached
palette is used exactly as today. The subsequent
`BrandingController::fetchRemoteConfig()`/`remoteConfigLoaded`/`isNewer()`
sync is unmodified in either branch: a genuine `branding_config` row from
the backend always wins, per the engine's own existing contract.

### 13.4 Light + dark

Both design references are light-theme only (Section 4.1 confirms neither
file defines a dark palette or a `prefers-color-scheme` branch) — dark
values are **derived**, not designed, exactly as spec §6 states. The
derivation reuses the engine's own math rather than inventing a second
color pipeline:

- Neutral surfaces invert in luminance while keeping the same hue family:
  `Theme.appBackground`(dark) and `Theme.card`(dark) are computed via
  `BrandColorMath::shade`/`mix` from the light values (e.g. a near-black
  base rather than `#FBF8F3`, with `card`(dark) a step lighter than
  `appBackground`(dark) to preserve the same figure/ground relationship the
  light theme has); `Theme.text`(dark)/`Theme.mutedText`(dark) move toward
  near-white/light-warm-grey.
- Brand roles (`Theme.brand.admin`/`kiosk`, `Theme.secondary`) keep their
  hue in dark mode — the maroon/gold identity should still read as
  maroon/gold at night — but are **re-contrast-checked against the new
  dark surfaces** using the engine's own WCAG machinery
  (`BrandColorMath::contrastRatio`, `brandcolormath.h:28-35`) rather than
  assumed to still pass: `MinContrast = 3.0` (`brandtheme.h:22`) for
  UI-role fills against their surface, and the spec's 4.5 threshold for
  body text (spec §6). Where a brand role fails against a dark surface, the
  same iterative-darken/lighten pattern `enforceOnWhite()` already uses
  against a *light* surface (`brandtheme.cpp:242-253`, "darken `c` until it
  meets `MinContrast` against white, capped iterations") generalizes
  directly: iterate `BrandColorMath::shade()` against the dark surface
  color instead of white, capped the same way.
- This derivation is explicitly named a Phase-5 human-review item, not a
  mechanical guarantee this document can certify from a text description
  alone — spec §10 Risk 3 ("Dark mode has no design reference — derived via
  contrast math; budget a human review round") applies verbatim here.

### 13.5 Theme mode (light/dark/system)

`Theme.mode` is backed by a local `QSettings` key distinct from the
`"branding"` group `saveCachedConfig`/`loadCachedConfig` use
(`brandtheme.cpp:365`) — e.g. a separate `"ui"` group / `themeMode` key —
so theme-mode is not conflated with the brand-palette cache; the two are
independent axes (which palette vs. which light/dark variant of it).
`System` mode should resolve via whatever OS color-scheme signal the Qt
version in use exposes; **this document does not assert a specific Qt API
here** (the exact mechanism — a live-updating signal vs. a
launch-time-only read — needs verification against the Qt 6.11 patch level
this project builds against, Phase 1 work, not a Phase 0 claim). Manual
theme mode (`ThemeMode::Manual`, `brandthemedata.h:13`) remains, per spec
§6 and Section 10.7, a **code hook only** — `regenerateFromLogo`'s existing
skip-if-Manual branch (`brandtheme.cpp:414-419`) is exercised by nothing
new in 2.0; there is no settings-screen affordance to *set* `ThemeMode::
Manual` in `SettingsViewModel`'s surface (Section 10.6), matching spec
§11's "no theme-editor UI in 2.0."

## 14. Navigation architecture

`Navigator` (introduced Section 7.3, placed outside the module-triple
pattern by Section 9) is a `QObject` singleton
(`qmlRegisterSingletonType`/`QML_SINGLETON`) owning surface, page, and
modal state as `Q_PROPERTY`s. This section elaborates its concrete surface;
it is the same `Navigator` those sections already name, not a second one.

### 14.1 Surface switching

`Q_PROPERTY(Surface currentSurface READ currentSurface WRITE
setCurrentSurface NOTIFY currentSurfaceChanged)` — a two-value
`enum class Surface { Kiosk, Admin }`. `AppShell.qml` (§8's tree) binds its
top-level scene selection to this property, replacing today's actual
mechanism: `MainWindow` constructing an `adminWindow*` member
(`mainwindow.h:42`) in its own constructor
(`mainwindow.cpp:52`) and showing it as a second top-level `QMainWindow`
on successful admin login (`mainwindow.cpp:226`, `adminWin->show();`).
Under `Navigator`, this becomes one property flip inside the same running
QML scene rather than a second window instantiation — the "one executable,
two surfaces" topology (Section 2.1/7) stays intact, only the *mechanism*
for switching between them changes from constructing a window to changing
a bound enum.

### 14.2 Page stack within admin

`Q_PROPERTY(QString currentPage READ currentPage WRITE setCurrentPage
NOTIFY currentPageChanged)`, valued from the canonical admin page set
Section 9's table already fixes — `Dashboard`, `Database`, `Reporting`,
`Search`, `VisitLogs`, `Settings` — plus
`Q_PROPERTY(QVariantList registeredPages ...)`, a data-driven list
`LSideNav` (Section 11) repeats over instead of Designer-time buttons. This
is the literal mechanism behind Section 9's forward-seam contract: today
adding a sixth admin page means adding a sixth hand-wired
`connect(ui->xBtn, &QPushButton::clicked, ...)` block
(`adminwindow.cpp:292-311`, Section 2.5); under `Navigator`, adding a
module means appending one entry to `registeredPages` and providing its
`qml/admin/<Module>.qml` + ViewModel + core-service triple — `Navigator`
itself needs no change, matching spec §4's "nothing in 2.0 may assume the
admin sidebar is a closed set" and Section 9's confirmed-absent-today
modules (`inventory`, `borrowreturn`, `aiassistant`). A small
`Q_PROPERTY(QStringList pageHistory ...)` (or an internal stack exposed via
`Q_INVOKABLE bool goBack()`) backs simple back-navigation for flows that
enter a page transiently (e.g. Ctrl+K jumping into Search from anywhere,
14.4).

### 14.3 Modal/dialog state

`Q_PROPERTY(QVariant activeModal READ activeModal NOTIFY
activeModalChanged)` — `null`/empty when nothing is open, otherwise a small
payload (`{component: "LDialog", title, message, buttons}`) that the one
`LDialog` instance `AppShell.qml` hosts binds its visibility and content
to. `Q_INVOKABLE void openModal(const QVariantMap &spec)`/
`Q_INVOKABLE void closeModal()`. Centralizing modal state here — rather
than each screen owning its own `LDialog` instance — is what guarantees
only one modal can be open application-wide, directly serving the
dialog-consolidation goal Sections 5.3/5.7 already argue for (fewer,
consistent interruptions instead of five/seven ad hoc `QMessageBox` call
sites). `LToast` (Section 11) is deliberately **not** routed through
`activeModal` — toasts are non-blocking and multiple/queued toasts are
fine; only the blocking `LDialog` needs the single-instance guarantee.

### 14.4 Centralized keyboard shortcuts

`Navigator` owns every application-wide shortcut so each is wired once
rather than per page:

- **Ctrl+K quick search:** a QML `Shortcut` bound once at `AppShell.qml`
  level calls `Navigator.navigateTo("Search")` then a
  `Q_INVOKABLE void focusPrimaryControl()` that the Search page's ViewModel
  or root item responds to by focusing its search field. This is the
  direct fix for the gap Section 5.8 already confirms by grep: "no
  `QShortcut` for search exists anywhere in `adminwindow.cpp`" — today
  Search is reachable only by clicking the sidebar button
  (`adminwindow.cpp:304-307`, `studentSearchBtn`).
- **Sidebar cycling:** Up/Down (or number-key 1-6) cycling through
  `registeredPages` while `LSideNav` has focus, implemented once in
  `Navigator`/`LSideNav`'s interaction contract rather than per sidebar
  button.
- **Table focus:** a shortcut (e.g. Tab or a dedicated accelerator) that
  moves focus into the current page's primary `LTable` instance, so
  keyboard-only admin use (spec §7, "full keyboard navigation on admin")
  does not require a mouse click to reach table rows after navigating to a
  page.

### 14.5 Testability

`Navigator` is a plain `QObject` with no QML dependency of its own — the
same shape Section 2.2 already documents for the five domain controllers
("injected, not owned... async signals") and the same shape Section 7.3
already commits to ("owns surface/page/modal state... so flow is
testable"). A `QtTest` case constructs `Navigator` standalone, calls
`navigateTo(...)`/`openModal(...)`/`goBack()`, and asserts on
`currentPage`/`currentSurface`/`activeModal` without instantiating a
`QQuickView` — the identical testing shape `tst_reportcontroller`/
`tst_studentcontroller`/etc. already use (Section 2.7's table), just for
navigation state instead of network/report data.

## 15. Animation guidelines

Motion is specified from the design files' real CSS `@keyframes`, not
prose invention — `Admin Dashboard.dc.html:19-23` defines `pageIn`, `rowIn`,
`barGrow`; `Library Kiosk v2.dc.html:18-23` adds `cardIn`, `scanPulse`,
`goldSweep`, plus `ringSpin`/`flashIn` used in both files (Section 4.1
already catalogs these keyframe names). Every duration below routes through
`Theme.motion.*` (Section 13.1) rather than a literal `Behavior on ...`
duration inside a component or screen — the same "no stray literals"
discipline Section 12.8 states for color.

### 15.1 Duration/easing table

| `Theme.motion` token | Duration | Easing | Citation |
|---|---|---|---|
| `pageIn` | 400ms | `cubic-bezier(.2,.8,.3,1)` | `Admin Dashboard.dc.html:73` (`animation:pageIn .4s cubic-bezier(.2,.8,.3,1)`) |
| `rowIn` | 350-500ms (+ per-row stagger) | `cubic-bezier(.2,.8,.3,1)` | `Admin Dashboard.dc.html:145` (.4s), `:212` (.35s); `Library Kiosk v2.dc.html:115` (.5s) — each staggered via a per-row `animation-delay` |
| `cardIn` | 450ms | `cubic-bezier(.2,.8,.3,1)` | `Library Kiosk v2.dc.html:76` (`animation:cardIn .45s cubic-bezier(.2,.8,.3,1)`) |
| `barGrow` | 600ms (+ per-bar stagger) | `cubic-bezier(.2,.8,.3,1)` | `Admin Dashboard.dc.html:107` (`animation:barGrow .6s … animation-delay:{{b.delay}}`) |
| `deptBarFill` | 800ms | `cubic-bezier(.2,.8,.3,1)` | `Admin Dashboard.dc.html:121` (`transition:width .8s cubic-bezier(.2,.8,.3,1)`) |
| `hoverLift` | 150-200ms | ease (default) | `Admin Dashboard.dc.html:80` (`transition:transform .18s, box-shadow .18s`), `Library Kiosk v2.dc.html:98` (`transition:transform .15s, box-shadow .2s`) |
| `scanPulse` | 1.6-3s, infinite | ease-in-out | `Library Kiosk v2.dc.html:44` (1.8s), `:71` (1.6s), `:69` (3s) |
| `goldSweep` | 3.2s, infinite | ease-in-out | `Library Kiosk v2.dc.html:50` (`animation:goldSweep 3.2s ease-in-out infinite`) |
| `flashIn` | 400-450ms | ease | `Library Kiosk v2.dc.html:66` (.45s), `:82` (.4s) |
| `ringSpin` | 60s (decorative) / 1.6s (loading-ring variant), infinite | linear | `Library Kiosk v2.dc.html:31` (60s), `:90` (1.6s) |
| `toastIn` / `toastOut` *(extrapolated — no toast exists in either reference file)* | ~200ms / ~150ms | `cubic-bezier(.2,.8,.3,1)` (reused, for consistency) | none — flagged as a Phase-1 human-review item alongside `Theme.elevation.modal` (Section 12.7) |

Per the spec's stated "150-400ms" motion band (spec §7), most interaction
transitions land inside it (`hoverLift`, `pageIn`, `rowIn`, `cardIn`,
`flashIn`); the two explicit exceptions — `barGrow`/`deptBarFill` at
600-800ms, and the multi-second ambient loops (`scanPulse`, `goldSweep`,
`ringSpin`) — are named here rather than silently rounded into the band,
since they are genuinely different motion categories (a one-shot
data-reveal animation vs. a continuous idle affordance) and the reference
files treat them differently on purpose. There is one shared easing curve,
`cubic-bezier(0.2, 0.8, 0.3, 1)`, used across every one-shot transition in
both files; the infinite ambient loops use plain `ease-in-out`/`linear`
instead, and `Theme.motion` keeps that distinction (`Theme.motion.easing`
for one-shot transitions vs. per-token easing for the ambient set) rather
than forcing one curve onto both categories.

### 15.2 Global reduce-motion switch

`Theme.motion.enabled` (bool) gates every animation token above. When
`false`: one-shot transitions (`pageIn`/`rowIn`/`cardIn`/`barGrow`/
`deptBarFill`/`hoverLift`/`flashIn`) either run at zero duration or are
skipped outright (state still changes, just without the animated
transition); the ambient/infinite loops (`scanPulse`, `goldSweep`,
`ringSpin`) are **suppressed entirely**, not merely shortened, since they
communicate no state change and exist purely as decorative/idle
affordances — a user who has opted out of motion should not see a
perpetually-pulsing dot or a perpetually-sweeping CTA highlight regardless
of how visually central that affordance is to the kiosk's idle-state
design. `Theme.motion.enabled` should be seeded from the OS-level
"prefers reduced motion" signal where the Qt version in use exposes one,
with a manual override persisted locally (the same local-`QSettings`
pattern as `Theme.mode`, Section 13.5) — as with System theme-mode
detection, the exact OS-signal API is a Phase-1 verification item against
the Qt 6.11 patch level in use, not asserted here as a specific call.

### 15.3 Per-interaction guidance

- **Live attendance feed (kiosk, Section 5.2):** `rowIn` plays once per row
  on initial load (staggered), but a genuinely new arrival — a fresh row
  inserted into `VisitLogModel` (Section 10.1) after a successful login —
  plays `rowIn` **immediately, undelayed**, so the newest entry visibly
  distinguishes itself the instant it appears. This directly targets
  Section 5.2's named friction point: today `refreshRightPanel`
  (`mainwindow.cpp:366-403`) overwrites label text with zero transition, so
  a new login is visually indistinguishable from data that was always
  there.
- **Dashboard bar growth (Section 10.2):** `barGrow` plays once per
  `DashboardViewModel.refresh()` call, staggered left-to-right across the
  hourly bars (matching `hourBars`' per-item `{{ b.delay }}` in the
  reference); the department bars use `deptBarFill` (a width transition),
  never `barGrow` (a vertical scale) — they are visually and mechanically
  different animations for two different bar shapes, not one animation
  reused across both.
- **Page transitions (Navigator, Section 14.2):** `pageIn` plays on the
  incoming page's root item only when `Navigator.currentPage` changes —
  `AppShell.qml`'s persistent chrome (`LSideNav`, `LPageHeader`) does not
  re-animate on every navigation, only the page content region does. This
  replaces today's instant `setCurrentWidget` swap
  (`adminwindow.cpp:292-311`, Section 4.3) with the reference's actual
  per-page entrance animation rather than a full-window fade.
- **Kiosk ambient affordances:** `scanPulse` on the idle "waiting for
  scan" state (`Library Kiosk v2.dc.html:69`) and `goldSweep` on the LOG IN
  CTA (`Library Kiosk v2.dc.html:50`) are always-on, event-independent
  motion — not triggered by a discrete action — and are exactly the
  affordances Section 5.1 identifies as currently missing ("no visible
  'scanning' affordance distinct from typing... no cue that RFID is even
  an option"). They remain subject to 15.2's reduce-motion suppression.
- **Toast in/out (Section 11's `LToast`):** since no toast exists in either
  reference file, its motion is extrapolated (15.1's `toastIn`/`toastOut`
  row) rather than measured from a keyframe — flagged, alongside dark mode
  (Section 13.4) and the modal elevation tier (Section 12.7), as a Phase-1
  human-judgment item rather than a value this document can cite a
  `file:line` for.

## 16. Backend reuse plan

**Locked decision (restated):** the 2.0 client keeps the existing PHP/MySQL
backend. The modernization is a Qt Widgets → Qt Quick migration of the
*client*; the server contracts are reused as-is. Nothing in this section
proposes a backend rewrite, an ORM, a framework, or a schema change — those
are explicitly out of scope (see Section 17's non-goals).

### 16.1 What the backend actually is (inventory)

`deliverables/loams_api/` holds **29 `.php` files**. Grouped by role:

| Role | Count | Files |
| --- | --- | --- |
| Shared helpers (not HTTP-callable on their own) | 3 | `config.php`, `db.php`, `auth_helper.php` |
| Single-entry dispatcher (alternate router) | 1 | `api.php` |
| Debug/diagnostic scripts (retire — see §17) | 2 | `phpinfo.php`, `testupload.php` |
| Application endpoints | 23 | the remainder listed in the table below |

So the client-facing surface is **25 request-handling scripts** (23
application endpoints + 2 debug scripts), sitting on **3 shared helpers**
and **1 dispatcher**. This corrects the round-number "30 endpoints" figure
some earlier planning notes carried — `ls deliverables/loams_api/*.php | wc
-l` returns **29**, and four of those are infrastructure, not endpoints.

The stack itself is reused verbatim: plain PHP with the `mysqli` driver, no
framework, JSON over HTTP. The shared helpers are:

- **`db.php`** — opens the `mysqli` connection every endpoint `include`s
  (`db.php:11`). Note it hardcodes `localhost`/`root`/empty-password/`wits_app`
  (`db.php:6-9`); `config.php` restates the same credentials as constants
  (`config.php:8-11`). This is the same-machine XAMPP topology the project's
  `security-hygiene.md` rule treats as illustrative rather than a live
  secret; §17 flags the duplication, not a rewrite.
- **`auth_helper.php`** — `requireAdminAuth($conn)` (`auth_helper.php:11-47`)
  verifies a posted `admin_key` against the bcrypt hash in the `admin` table
  with `password_verify` and exits `401` on failure. This is the sanctioned
  admin-auth pattern and is reused unchanged.
- **`config.php`** — environment/CORS/upload constants (`config.php:8-26`).

Because the core schema (`students`, `library_visits`, `visitors`, `admin`
tables) is **not checked into this repo** — `git ls-files '*.sql'` returns
only `deliverables/sql/branding_config.sql` — the table/column facts below
are read out of the endpoints' own SQL rather than a schema file. For
example the RFID flow reads `students.code` and writes `library_visits`
(`rfid_login.php:23,32-35`), the report join is `students LEFT JOIN
library_visits ON s.school_id = v.student_id` (`get_report_data.php:42-45`),
and the guest log is the `visitors` table (`get_visitors.php:37-40`,
`guest_login.php:35`). The 2.0 client depends on these shapes, not on any
`.sql` artifact.

### 16.2 Contracts reused unchanged

The 2.0 QML client calls the **same endpoints the current Widgets client
already calls**, with the **same request bodies and response JSON**. The
migration replaces the *caller* (widget method bodies → ViewModel/service
seams per Sections 9-10), never the wire contract. Endpoints already wired
through `ApiConfig::endpoint()` today:

| Endpoint (relative to base URL) | Method | Server auth | Query style | Client call site (reused) |
| --- | --- | --- | --- | --- |
| `student_login.php` | POST | none (public kiosk) | prepared (`student_login.php:25`) | `mainwindow.cpp:195` |
| `rfid_login.php` | POST | none (public kiosk) | prepared, transactional (`rfid_login.php:23,32-35,57`) | `mainwindow.cpp:309` |
| `guest_login.php` | POST | none (public kiosk) | prepared (`guest_login.php:35`) | `guestwindow.cpp:40` |
| `admin_login.php` | POST | verifies `admin_key` inline (`admin_login.php:12-17`) | prepared | `mainwindow.cpp:198` |
| `update_admin_key.php` | POST | verifies `old_key` inline (`update_admin_key.php:18`) | prepared (`update_admin_key.php:14,20`) | `adminwindow.cpp:780` |
| `get_departments.php` | GET | none | static query (`get_departments.php:15-16`) | `adminwindow.cpp:447,1366`, `studentcontroller.cpp:246`, `reportcontroller.cpp:197` |
| `get_courses.php` | GET | none | prepared (`get_courses.php:25`) | `studentcontroller.cpp:262`, `reportcontroller.cpp:249` |
| `get_years.php` | GET | none | static query (`get_years.php:17`) | `reportcontroller.cpp:224` |
| `get_report_data.php` | POST | none | dynamic-but-parameterized (`get_report_data.php:90,103`) | `reportcontroller.cpp:280` |
| `api.php/reports/data` | POST | none | dispatches `get_report_data.php` (`api.php:95-100`) | `reportcontroller.cpp:315` |
| `search_students.php` | POST | none | prepared (`search_students.php:48`) | `studentcontroller.cpp:144` |
| `register_student.php` | POST | none | prepared (`register_student.php:86-91`) | `adminwindow.cpp:728` |
| `bulk_update_students.php` | POST | none (see §17) | prepared (`bulk_update_students.php:43`) | `studentcontroller.cpp:175` |
| `delete_students.php` | POST | none (see §17) | prepared (`delete_students.php:19`) | `studentcontroller.cpp:214` |
| `check_duplicates.php` | POST | none | prepared (`check_duplicates.php:16`) | `importcontroller.cpp:157` |
| `upload_students_zip.php` | POST | none (see §17) | prepared insert (`upload_students_zip.php:73`) | `importcontroller.cpp:201` |
| `deactivate_department.php` | POST | none (see §17) | prepared (`deactivate_department.php:14`) | `adminwindow.cpp:547` |
| `delete_department.php` | POST | none (see §17) | prepared (`delete_department.php:21,30`) | `adminwindow.cpp:644` |
| `reset_visits.php` | POST | none (see §17) | prepared (`reset_visits.php:17,23`) | `adminwindow.cpp:595` |
| `get_visitors.php` | POST | none | **string-built (see §17)** (`get_visitors.php:19-33`) | `visitorcontroller.cpp:25` |
| `get_branding.php` | GET | none | static query (`get_branding.php:5`) | `brandingcontroller.cpp:20` |
| `save_branding.php` | POST | **`requireAdminAuth`** (`save_branding.php:6`) | prepared + validation (`save_branding.php:31-43`) | `brandingcontroller.cpp:43` |

Two application endpoints exist server-side but are **not currently wired
to the client**: `update_admin_info.php` (`update_admin_info.php:14`) and
`get_courses_by_department.php` (`get_courses_by_department.php:10-13`).
They are reused (left in place) but need no client change for 2.0; if a
redesigned admin screen (Section 18) wants them, they call through the same
`ApiConfig::endpoint()` seam.

Every one of the reused contracts above is treated as **frozen** for the
migration. The single most important consequence for §17: hardening must
not change any of these request/response shapes (see the contract-tests-first
method).

### 16.3 The branding endpoints (Phase 1) are reused verbatim

`get_branding.php` / `save_branding.php` and their backing table
`branding_config` (`deliverables/sql/branding_config.sql:4-12`, a new
singleton `id = 1` row that alters no existing table) were added in Phase 1
for the brand-engine work. They are **already the model** the rest of the
backend should look like: `save_branding.php` requires admin auth
(`save_branding.php:6`), whitelists `theme_mode` to `auto`/`manual`
(`save_branding.php:12`), rejects a non-object palette via `json_decode` +
`is_array` (`save_branding.php:18-19`), validates `logo_hash` as a 64-char
SHA-256 hex string (`save_branding.php:25`), and writes through a prepared
`INSERT ... ON DUPLICATE KEY UPDATE` (`save_branding.php:31-43`). The 2.0
`BrandingController` (`brandingcontroller.cpp:20,43`) consumes these
unchanged; no reuse work is needed beyond keeping the QML `Theme` singleton
(Section 13) reading the same JSON.

### 16.4 New endpoints are additive off one base URL

`apiconfig.h` is the single source of truth for the backend base URL
(`apiconfig.h:18-21`) and `ApiConfig::endpoint()` (`apiconfig.h:27-34`)
builds every request path relative to it — already unit-tested
(`qt-app/tests/tst_apiconfig.cpp:17-37`). Any endpoint a redesigned 2.0
screen needs that does not exist today (for example the **student Visit
Logs** screen that Section 5.9 establishes is *net-new* and distinct from
the existing guest log — `get_visitors.php` is the guest-sign-in log, not a
student-attendance endpoint) is added **additively**: a new `.php` file
reached through the same `ApiConfig::endpoint("new_endpoint.php")` seam, with
no change to any existing contract. The product decision on whether such a
student-attendance endpoint is built is left to Section 18; §16 only fixes
the rule that it would be additive, not a rework of `get_visitors.php`.

## 17. Backend refactoring plan

**Scope fence.** This section is **targeted hardening only**. It does not
propose — and explicitly excludes — a backend redesign, a framework/ORM
adoption, a migration off `mysqli`, a schema redesign, or any change to the
request/response contracts reused in Section 16. Every item below is a
bounded, cited fix to the *existing* code. Anything larger is out of scope
for Phase 0's proposal and is not relitigated here. All of this work maps to
the spec's **Phase 6** backend-hardening entry (spec §9, Phase 6); it is not
Phase 1 work and it does not block the client migration.

### 17.1 Method: contract-tests-first (zero contract drift)

Because Section 16 freezes the wire contracts, hardening must be provably
behavior-preserving. The method, per endpoint, is:

1. **Capture a fixture.** Record the current request (method + body) and the
   current response JSON for each endpoint, using **synthetic data only** —
   no real student names, school IDs, photos, or visit logs (per
   `security-hygiene.md`). A captured `visitors`/`students` row in a fixture
   must be invented, e.g. `{"name":"Test User","company":"ACME","school_id":"S0001"}`.
2. **Pin it as a golden test.** A PHP-side contract test (or a captured
   `QByteArray` replayed through the client's JSON decoders, the same
   isolation the workflow rule already prescribes for network logic) asserts
   the response shape stays identical.
3. **Harden against the pinned fixture.** Apply the change (parameterize,
   add auth, etc.) and require the golden test to still pass — same JSON keys,
   same status strings. If the shape must change, it is no longer "targeted
   hardening" and is out of scope.

This gives each item below an objective done-condition: the fixture still
matches. `ApiConfig` is already unit-tested (`qt-app/tests/tst_apiconfig.cpp`),
so the client half of this harness has a home; the server half is new Phase-6
work.

### 17.2 Hardening backlog (cited)

| # | Item | Evidence (file:line) | Disposition |
| --- | --- | --- | --- |
| H1 | **String-built SQL in the guest log.** `get_visitors.php` interpolates the search term via `real_escape_string` into a `LIKE` and interpolates the raw `start_date`/`end_date` **unescaped** into the `WHERE` | `get_visitors.php:19-33` (dates at lines 26, 30, 33) then `$conn->query($sql)` at `:42` | Parameterize with `prepare`/`bind_param`, matching the pattern `get_report_data.php:90-103` already uses for its dynamic query. This is the one clear injection surface; every other endpoint audited already uses prepared statements. |
| H2 | **Destructive endpoints are unauthenticated at the server.** `requireAdminAuth` exists but is wired to exactly **one** endpoint (`save_branding.php:6`). The mutating/destructive endpoints do not call it | `delete_students.php` (no auth), `reset_visits.php:8-9`, `bulk_update_students.php`, `deactivate_department.php:14`, `delete_department.php`, `upload_students_zip.php` — a grep for `requireAdminAuth` hits only `auth_helper.php` and `save_branding.php` | Add `include "auth_helper.php"; requireAdminAuth($conn);` to each destructive endpoint. Admin gating is currently **client-side only** (the Qt UI hides the buttons), which is not a server control. |
| H3 | **Doc/code drift on H2.** `deliverables/loams_api/SECURITY_UPDATES.md` claims auth was *already* added to `delete_students.php`, `bulk_update_students.php`, `reset_visits.php`, and `upload_students_zip.php` | `SECURITY_UPDATES.md:53-89` vs. the actual files (which contain no `admin_key` check) | Treat the doc as the intended spec and actually apply it; then the doc is true. Do **not** trust `SECURITY_UPDATES.md` as a description of current state. |
| H4 | **A credentials copy that bypasses `db.php`.** `delete_students.php` opens its own inline `new mysqli("localhost","root","","wits_app")` instead of `include "db.php"` | `delete_students.php:11` | Route it through the shared `db.php` (`db.php:11`) so credentials live in one place — a mechanical change, no contract impact. |
| H5 | **Duplicated DB credentials in source.** `db.php` and `config.php` both hardcode `root`/empty-password/`wits_app` | `db.php:6-9`, `config.php:8-11` | Collapse to one source (have `db.php` read `config.php`, which `SECURITY_UPDATES.md:36-43` claims was done but the committed `db.php` does not do). Illustrative XAMPP creds per `security-hygiene.md`; keep placeholders in any doc example. |
| H6 | **Debug/diagnostic endpoints shipped.** `phpinfo.php` dumps the full PHP runtime config to any caller; `testupload.php` echoes ini limits and probes `uploads/` writability | `phpinfo.php:1-3`, `testupload.php:1-24` (both established in Section 3.5) | **Remove** both. Neither is referenced by any `ApiConfig::endpoint(...)` call in `qt-app/`. This is the spec's Phase 6 line item verbatim ("remove debug-only endpoints"). |
| H7 | **`hash_admin.php` decision.** | Section 3.5; a directory listing of `deliverables/loams_api/` (29 `.php` files) contains no such file | **No-op.** The file does not exist in this repo, so there is nothing to remove or refactor. Recording the decision *is* the resolution the spec's Phase 6 note asked for. |
| H8 | **Plaintext transport.** The single base URL is `http://localhost/loams_api/` — plaintext HTTP, no TLS, compile-time constant | `apiconfig.h:20` | Track the transport question: make the base URL configurable (env/config/`QSettings` override) and support `https://` for any non-localhost deployment. Fine as a same-machine default; a hardening item once auth endpoints carry an admin key over the wire. Ties to Section 3.4. |
| H9 | **CORS wildcard.** `Access-Control-Allow-Origin: *` is emitted both in the dispatcher and via config | `api.php:9`, `config.php:20` | Lock to the deployed origin for production (`config.php:18-19` already documents this). Config change, no contract impact. |

### 17.3 Validation looseness — extend the branding pattern

The branding endpoints already validate input strictly (`save_branding.php:12-29`
— whitelist, `is_array` JSON check, regex on the hash). Most read endpoints
take no user-controlled input worth validating, but the write endpoints that
accept free-form fields (`register_student.php:8-16`, `bulk_update_students.php`,
`update_admin_info.php:14`) currently rely on `trim()` plus the prepared
statement. Prepared statements neutralize injection, so this is a
**defense-in-depth / data-quality** item, not an injection fix: apply the
same whitelist/format-check discipline `save_branding.php` demonstrates
(e.g. constrain `gender`/`status`/`year_level` to known enums). Low
priority; grouped here so the pattern is named, not scattered.

### 17.4 Explicit non-goals

To keep the scope fence unambiguous, the following are **out of scope** for
this plan and are called out so a future contributor does not read them into
it:

- **No backend redesign / rewrite.** The PHP + `mysqli` + MySQL stack stays.
- **No framework, ORM, router replacement, or migration tooling.** `api.php`
  stays as-is (the client uses both the direct `.php` paths and the one
  `api.php/reports/data` route — `reportcontroller.cpp:315`).
- **No schema redesign.** The unversioned core schema (§16.1) is not
  restructured; the only tracked DDL remains `branding_config.sql`.
- **No contract changes.** Every item in §17.2 is behavior-preserving by the
  §17.1 method; anything that would alter a response shape is rejected as
  out of scope.

Net: §17 is a short, bounded Phase-6 checklist (H1-H9 + §17.3) executed
against golden fixtures, not a backend project.

## 18. Screen-by-screen redesign recommendations

One subsection per screen in the Section 5 canonical inventory — **Kiosk
login, Live attendance feed, Guest flow, Admin dashboard, Database/students,
Reporting, Search, Visit logs, Settings**. Each states *what changes*, *which
ViewModel (Section 10) and `L*` components (Section 11) compose it*, and *the
parity items that must be preserved* (verified against the per-module parity
checklist, spec §8). Every component name and ViewModel name is reused
verbatim from Sections 10-11; no screen invents a new one. Where a screen
depends on RFID behavior, "RFID behaves exactly as today" is stated as
**observable behavior to be verified in the parity checklist** (Section 19's
forward-note), not as assumed code reuse — the `RfidKeyboardFilter` install
layer is re-authored for `QQuickWindow`, only the `RfidScanDetector` state
machine is reused verbatim (Sections 6.2/7.1).

### 18.1 Kiosk login

**Redesign (design: `Library Kiosk v2.dc.html`).** The maroon brand panel
becomes an `LSideNav`-style brand region (kiosk instance, footer omitted —
Section 11) with the school wordmark, live clock, and library hours. The
single dual-purpose `QLineEdit` (`mainwindow.ui:143`, Section 5.1) is
replaced by a bound input with an **explicit admin affordance**
(`KioskViewModel.isAdminMode`, Section 10.1) instead of the silent
password-echo flip (`mainwindow.cpp:144-154`), and a pulsing "SCAN OR TYPE
YOUR ID" affordance (`Theme.motion.scanPulse`, Section 15.1/15.3, ambient) so
a walk-up student sees that RFID is an option — the cue Section 5.1 flags as
missing. The gold **LOG IN** CTA is an `LButton variant: Accent`
(`Theme.secondary` gold fill, `Theme.elevation.ctaDark`, Section 11), with
the `goldSweep` idle affordance (`Library Kiosk v2.dc.html:50`). Failed logins
surface through **`LToast`** (`KioskViewModel.statusMessage`) rather than the
blocking `QMessageBox::warning` (`mainwindow.cpp:184,232`) — making manual and
RFID failures one consistent inline surface (Section 5.1's simplification).

- **ViewModel / components:** `KioskViewModel`; `LSideNav` (brand panel),
  `LButton` (Accent LOG IN), input field, `LToast`.
- **Parity to preserve:** student-ID → `student_login.php` vs. admin-key →
  `admin_login.php` shape-routing (`mainwindow.cpp:193-200`); RFID →
  `rfid_login.php` with the 2.5-second same-card debounce
  (`mainwindow.cpp:301-305`); submit on Enter and on button click;
  full-screen kiosk presentation. **RFID behaves exactly as today** — an
  observable parity item, since the install layer is re-authored (above).

### 18.2 Live attendance feed

**Redesign (design: `Library Kiosk v2.dc.html`).** The five parallel,
hand-named 9-slot `QList<QLabel*>` arrays (`mainwindow.cpp:366-403`, Section
5.2) are replaced by a model-driven, scrollable list backed by `VisitLogModel`
(`KioskViewModel`'s own instance, Section 10.1) — removing the hard 9-row
ceiling entirely (the 10th visitor of the day is no longer dropped). The "now
signed in" hero becomes a genuinely separate element (`LCard variant: hero`,
brand-gradient fill, Section 11) rather than an alias for feed row 0, so hero
and feed can be timed/styled independently (Section 5.2). The kiosk's two stat
cards (visitors today, this hour — `Library Kiosk v2.dc.html:97-105`) are
`LStatTile`s (`Hero`/`Neutral` variants). A newly-arrived login plays
`Theme.motion.rowIn` **undelayed** (Section 15.3) so it visibly distinguishes
itself from data already on screen — the transition today's synchronous label
overwrite lacks.

- **ViewModel / components:** `KioskViewModel` + `VisitLogModel`;
  `LCard` (hero), `LStatTile` (×2), a model-driven feed list.
- **Parity to preserve:** each successful login prepends to the feed
  (conceptually today's `recentLogins`, `mainwindow.cpp:272-274`); the
  most-recent login is spotlighted as the hero; the 700ms photo fade-in
  intent (`mainwindow.cpp:260-264`) survives as an `LCard` entrance
  transition.

### 18.3 Guest flow

**Redesign (current: `guestwindow.ui`).** The modal four-field dialog
(`guestwindow.cpp:30-33`, Section 5.3) becomes a single QML form composed of
`LCard` + bound fields + an `LButton` submit, gated — as today — behind the
admin-controlled toggle (`KioskViewModel.guestLoginEnabled` mirrors
`adminWin`'s `guestLoginToggled`, `mainwindow.cpp:83-85,91`; guest login stays
off-by-default). The three validation-only `QMessageBox` calls
(`guestwindow.cpp:36,54,64`) collapse into inline `LToast` feedback with
required-field markers (name/company/purpose required, contact optional —
Section 5.3), and submit is disabled until valid. The **two genuine
network-outcome dialogs** (`guestwindow.cpp:70,73`) survive as the one modal
primitive 2.0 keeps, `LDialog` (Section 11) — matching Section 5.3's "removes
3 of the 5 modal dialogs... without touching the two genuine network-outcome
dialogs."

- **ViewModel / components:** `KioskViewModel` (`requestGuestLogin`);
  `LCard`, `LButton`, `LToast` (validation), `LDialog` (network outcome).
- **Parity to preserve:** four fields with name/company/purpose required;
  POST to `guest_login.php` (`guestwindow.cpp:40`); dialog dismisses on
  success; the feature stays admin-gated and off by default.

### 18.4 Admin dashboard

**Redesign (design: `Admin Dashboard.dc.html:72-129`) — net-new UX over
existing data.** This screen does not exist today (Section 4.4/5.4: the
`generalBtn`/`generalPage` slot is a *settings* page, not a dashboard), so it
is new construction, not a redesign of an existing screen. It is built as an
`LPageHeader` (title + "Overview of library activity" subtitle + live clock,
`Admin Dashboard.dc.html:63-69,282`) over four `LStatTile`s (visitors today,
visitors this week, registered students, peak hour) and two `LBarChart`
instances: vertical hourly bars with `barGrow` and horizontal department bars
with `deptBarFill` (Section 15.3), built as **custom QML, not QtCharts** (per
Risk 2 — QtCharts stays only in `reportrenderer`'s export path, Sections
3.9/7.1/19). All figures come from `DashboardViewModel`
(`ReportController` + `ReportRenderer` aggregates, Section 10.2) — the numbers
Section 5.4 confirms already exist in the backend though no screen surfaces
them.

- **ViewModel / components:** `DashboardViewModel`; `LPageHeader`,
  `LStatTile` (×4), `LBarChart` (×2), `LSideNav` (chrome).
- **Parity to preserve:** none pre-existing (net-new screen) — the parity
  criterion is instead that every displayed figure matches what
  `ReportController`'s existing endpoints already compute for the Reporting
  charts (no new backend needed).

### 18.5 Database/students

**Redesign (design: `Admin Dashboard.dc.html:132-164`).** Today's single
`bulkTable` serving two mental models — staged-import review vs. live-record
edit, distinguished only by which handler last ran (`adminwindow.cpp:825-857,
858,2061`, Section 5.6) — becomes one always-live `LTable` (backed by
`StudentListModel`, Section 10.3) with **import and add as explicit actions,
not modes**: `+ ADD STUDENT` / `IMPORT CSV` `LButton`s
(`Admin Dashboard.dc.html:136-137`) feeding it. "Am I previewing an import" is
tracked as explicit `StudentsViewModel.isImporting` state (Section 10.3/5.6),
not inferred. The file picker that today opens `AttachFilesDialog`
(`adminwindow.cpp:827`) is an `LDialog`; import/duplicate/upload outcomes
report through `LToast`.

- **ViewModel / components:** `StudentsViewModel` (Database instance, owns
  `StudentController` + `ImportController`, Section 10.3); `LPageHeader`,
  `LTable`, `LButton`, `LDialog` (file pick), `LToast`.
- **Parity to preserve:** CSV/Excel bulk import with duplicate check and
  photo-ZIP upload (`importcontroller.h`, Section 2.2); inline per-cell edit;
  the reviewed batch commits through the same `ImportController` path.

### 18.6 Reporting

**Redesign (current: `reportingPage`, `adminwindow.ui:1298`).** The
duration-type switch, department/course filters, and palette selector stay,
but `collectReportFilters`'s seven inline `QMessageBox::warning` calls
(`adminwindow.cpp:1498-1605`, Section 5.7) collapse into one bindable
`ReportsViewModel.validationError` surfaced via `LToast` (Section 10.5) — one
inline-validation pattern instead of seven ad hoc dialogs. The
mandatory-department gate (`adminwindow.cpp:1505-1509`, Section 5.7) is
**removed**: an "all departments" report path is added as a required
capability (not just a visual refresh, Section 5.7). `QGroupBox`-framed chart
regions (`groupBox_3`/"VISUALIZATION", `adminwindow.ui:1705-1719`, Section 4.3)
become flat `LCard`s. **Export reuses `reportrenderer` verbatim** —
`ReportsViewModel` delegates PDF/Excel to `ReportRenderer`'s `QChart`-based
static painters (`reportrenderer.h:29-45`, Section 10.5), the one
spec-accepted `Qt::Charts`+`Qt::Widgets` link (Sections 3.9/7.1); any on-screen
bar preview uses custom `LBarChart` (Section 18.4), not QtCharts.

- **ViewModel / components:** `ReportsViewModel` (`ReportController` +
  `ReportRenderer`); `LPageHeader`, `LCard`, `LButton` (GENERATE, export
  PDF/Excel), `LToast`, `LBarChart` (on-screen preview only).
- **Parity to preserve:** day/month/semester/custom duration modes and their
  date-range math (`ReportController::computeDateRange`,
  `reportcontroller.h:34-39`); department/course/palette filters; PDF and
  Excel export producing the same documents `ReportRenderer` produces today.

### 18.7 Search

**Redesign (design: `Admin Dashboard.dc.html:196-227`).** The Enter-only
search (`adminwindow.cpp:438`, Section 5.8) gains debounced live filtering and,
critically, the **Ctrl+K** global quick-search shortcut (`Navigator`-owned,
Sections 14.4/5.8 — there is no `QShortcut` for search anywhere today). The
results grid becomes an `LTable` (`StudentListModel`), the gold SEARCH CTA an
`LButton variant: Accent` (`Theme.elevation.ctaGold`,
`Admin Dashboard.dc.html:201`). Per Section 9, Search reuses the
`StudentsViewModel` class as a **search-only instance** (no `ImportController`)
— it does not introduce a view-model class of its own.

- **ViewModel / components:** `StudentsViewModel` (search-only instance);
  `LPageHeader`, `LCard`, `LButton` (Accent), `LTable`, `LSegmented`/filter
  controls.
- **Parity to preserve:** department/course filter narrowing
  (`studentcontroller.h`); the 9-column result set (`adminwindow.cpp:435-436`);
  inline bulk-edit (the same `itemChanged`-driven capability as Database,
  `adminwindow.cpp:2061`).

### 18.8 Visit logs — the two distinct surfaces, resolved

Section 5.9 established that two different things share the word "visit": the
design's **Visit Logs** page shows **student attendance** (per-row
time-in/time-out, `Admin Dashboard.dc.html:229-260`), while today's nearest
analog, `visitorPage` (`adminwindow.ui:2214`), is a **guest sign-in log**
(name/company/purpose/date/time, `adminwindow.cpp:2508-2532`) backed by
`get_visitors.php`. Section 16.4 fixed the rule that a genuine
student-attendance endpoint would be **additive** and left the product
decision here. **This section makes it.** Both surfaces are designed
explicitly; they are not collapsed into one:

- **(a) Guest sign-in log — ships in 2.0, reuses `get_visitors.php`.** The
  existing `visitorPage` is modernized in place: `VisitLogsViewModel` wraps
  `VisitorController` end-to-end (the worked example of Section 10.4), the
  four-mode `visitorDateFilterBox` combo (`adminwindow.cpp:2477-2506`,
  Section 4.4) is replaced by an **`LSegmented` Today/This-Week control**
  (`Admin Dashboard.dc.html:232-236`) two-way-bound to
  `VisitLogsViewModel.rangeMode`, and rows render through `LTable`
  (`VisitLogModel` over `QList<VisitorRecord>`). The Today→`SpecificDay` /
  ThisWeek→`DateRange` mapping happens inside the ViewModel;
  `VisitorController`'s own surface is untouched (Section 10.4). Excel export
  reuses `VisitorController::exportToExcel` (`visitorcontroller.h:22-25`). This
  is the surface the spec's "Today/This-Week visitor-log toggle... lands
  natively here" (spec §7) refers to.

- **(b) Student attendance "Visit Logs" — the design-target screen; backend
  is net-new additive work, deferred, and not a 2.0 blocker.** The design's
  time-in/time-out rows need student-attendance history, which
  `get_visitors.php` does **not** provide (it is the guest log, Section
  5.9/16.4). Serving it requires a **new additive endpoint** (e.g.
  `get_visit_logs.php` reading the `library_visits` table the RFID flow
  already writes — `rfid_login.php:32-35`, Section 16.1) reached through the
  same `ApiConfig::endpoint(...)` seam with **no change to any existing
  contract** (Section 16.4). That endpoint is **deferred backend work mapping
  to Section 17's Phase-6 scope** (additive rather than a rewrite of
  `get_visitors.php`), **not a dependency that blocks the 2.0 client
  migration** — the client, `Theme`, components, and `VisitLogsViewModel` all
  ship in Phase 3 regardless. When that endpoint lands, the design's Visit
  Logs sidebar entry is powered by the **same `VisitLogsViewModel`** with its
  model source widened/swapped from guest rows to attendance rows — **not a
  second ViewModel** (Section 10.4's tension note). Until then, the Visit Logs
  screen presents guest-log rows (surface (a))'s data.

- **ViewModel / components:** `VisitLogsViewModel` + `VisitLogModel`;
  `LPageHeader` (with EXPORT CSV / PRINT REPORT actions,
  `Admin Dashboard.dc.html:238-239`), `LSegmented` (Today/This-Week),
  `LTable`, `LButton`, `LToast`.
- **Parity to preserve (surface (a)):** guest-log fetch/filter/export via
  `VisitorController` (`fetchVisitors`/`exportToExcel`,
  `visitorcontroller.h:19,22-25`); the export filename default
  (`defaultExportFileName`); the guest-record columns
  (`visitordata.h:9-14`). Terminology stays consistent with Section 5.9/16.4:
  **Visit logs** (student attendance, design-target) vs. the current
  narrower **guest log** (`visitorPage`/`VisitorController`).

### 18.9 Settings

**Redesign (current: folded into `generalPage`, Section 5.5).** Settings gets
its **own sidebar entry**, split out from the (net-new) dashboard that now
occupies the "General" slot (Section 5.4/5.5 — this is adding a sidebar entry
via `Navigator.registeredPages`, Section 14.2, not renaming one). The
school/admin/library fields, admin-key update, and library hours render as
`LCard` sections over an `LPageHeader`; `SettingsViewModel` (Section 10.6)
consolidates the two current read paths (`SettingsController::load()` and
`collectHeaderInfo`'s separate `QSettings` read, `adminwindow.cpp:1401-1412`)
into the one controller call. **Live logo re-theming** is preserved and made
first-class: importing a logo calls
`ThemeViewModel::regenerateFromImportedLogo(path)` (Sections 10.6/10.7), which
runs the existing `BrandTheme::regenerateFromLogo` and emits `changed()`, so
every `Theme.*`-bound component repaints with the new brand **without a
restart** (Section 13.2's property-binding mechanism) — the "live re-theme via
property bindings" spec §6 requires.

- **ViewModel / components:** `SettingsViewModel` (+ `ThemeViewModel` for
  logo re-theme); `LPageHeader`, `LCard`, `LButton` (import logo/poster,
  save), `LToast`.
- **Parity to preserve:** school-name/address/font, admin-key change
  (`update_admin_key.php`, `adminwindow.cpp:780`), library open/close hours,
  logo/poster import; the Manual-mode hook stays code-only with no editor UI
  (Sections 10.7/13.5, spec §11); re-theme is gated on `ThemeMode::Auto`
  exactly as today (`adminwindow.cpp:1174-1187`, Section 2.3).

## 19. Risk analysis

This expands the spec's six named risks (spec §10) with an explicit
likelihood / impact / mitigation / owner-check for each, and adds the new
risks surfaced while writing Sections 2-18. "Owner-check" is the concrete
thing the owner (or the per-phase gate) verifies to confirm the risk is
retired — not a restatement of the mitigation.

### 19.1 Spec §10 risks, expanded

**R1 — RFID under QML focus (top risk).**
The kiosk RFID path today is an application-wide event filter,
`RfidKeyboardFilter`, gating on `QApplication::activeWindow()`/`focusWidget()`
(`rfidkeyboardfilter.cpp:25,33-36`, Section 2.4) — both `QApplication`/
`QWidget` APIs with no `QQuickWindow` equivalent. The pure `RfidScanDetector`
state machine underneath is reused verbatim (`rfidscandetector.h:20`, Section
7.1); the **install/focus layer is re-authored, not reused** (Sections 6.2/
7.1). Event delivery and focus semantics differ between Widgets and Quick,
which is why this is the top risk.
- **Likelihood:** Medium (a known-different mechanism, but a bounded one).
- **Impact:** High (RFID is the kiosk's primary walk-up input; a regression
  here breaks the core flow).
- **Mitigation:** de-risk with the **Phase 1 RFID-in-QML spike** *before*
  Phase 2 depends on it (spec §9; Section 20). Reuse the detector state
  machine as-is; re-author only the `QQuickWindow`-side capture/focus layer.
- **Owner-check:** the Phase 2 kiosk **parity checklist verifies "RFID
  behaves exactly as today" as observable behavior** — tap latency, the
  2.5-second same-card debounce (`mainwindow.cpp:301-305`), and no
  double-logging from propagated key events — **not** as assumed code reuse
  (the reviewer's forward-note, Sections 6.2/18.1). This is the single most
  important owner-check in the plan.

**R2 — QtCharts vs. QML styling.**
QtCharts' QML styling fights the maroon/gold design (spec §10 Risk 2). The
dashboard's simple bars are therefore built as **custom QML** (`LBarChart`,
Section 11/18.4), and QtCharts is confined to `reportrenderer`'s export path
only.
- **Likelihood:** Low (the decision is already made; the risk is only that
  someone reaches for QtCharts on-screen out of habit).
- **Impact:** Low-Medium (a styling mismatch on the dashboard, not a
  functional failure).
- **Mitigation:** `LBarChart` covers both dashboard bar shapes
  (`orientation: Vertical|Horizontal`, Section 11); `reportrenderer` keeps its
  `QChart`-based static painters for PDF/Excel export, carrying the accepted
  transitive `Qt::Charts` + `Qt::Widgets` link (Sections 3.9/7.1 — `QChart`
  derives from `QGraphicsWidget`; `tst_reportrenderer` already pays this cost
  at `tests/CMakeLists.txt:152-157`).
- **Owner-check:** the Section 12.8 grep audit — no raw QtCharts widget in any
  `qml/` screen; charts on screen resolve through `LBarChart`, and the only
  `Qt::Charts` link in the Quick app is on the `reportrenderer` export path.

**R3 — Dark mode has no design reference.**
Both built-out design files are light-theme only (Section 4.1); dark values
are **derived, not designed** (Sections 6.5/13.4), using the engine's own WCAG
math (`BrandColorMath::contrastRatio`, `brandcolormath.h:28-35`).
- **Likelihood:** High that *some* derived pairing needs hand-tuning (there is
  no reference to match).
- **Impact:** Low-Medium (cosmetic/contrast, isolated to dark mode; light mode
  is fully specified).
- **Mitigation:** derive dark surfaces via `shade`/`mix`, re-contrast-check
  brand roles against the new surfaces (`MinContrast = 3.0`, `brandtheme.h:22`;
  4.5 for body text), and **budget a human review round in Phase 5** (Section
  13.4).
- **Owner-check:** the Phase 5 a11y/human-walkthrough gate signs off dark mode
  visually and against the contrast thresholds; no dark-mode value is claimed
  "done" from a text description alone.

**R4 — Deployment-hardware rendering performance.**
Qt Quick's scene-graph rendering may underperform the current Widgets app on
the actual library PC (potentially older/integrated graphics).
- **Likelihood:** Medium (unknown hardware; Quick is GPU-oriented).
- **Impact:** High if it materializes (a sluggish kiosk is a visible
  regression at the point of use).
- **Mitigation:** **validate Qt Quick rendering on the real library PC in
  Phase 1** (spec §9/§10 Risk 4), before any screen is built on the
  assumption it renders smoothly; **document the software-rendering fallback**
  (e.g. the Qt Quick software backend) as the contingency.
- **Owner-check:** a Phase 1 sign-off that `AppShell.qml` + the animation set
  (Section 15) run acceptably on the deployment machine, with the
  software-rendering fallback documented and tested if the GPU path is
  inadequate.

**R5 — PHP hardening regressions.**
The Phase-6 backend hardening (Section 17) could drift a request/response
contract the frozen client depends on (Section 16.2).
- **Likelihood:** Medium (nine hardening items, H1-H9, touch live endpoints).
- **Impact:** High (a contract drift silently breaks a shipped 2.0 screen).
- **Mitigation:** **contract-tests-first** (Section 17.1) — capture golden
  request/response fixtures (synthetic data only) *before* touching any
  endpoint; a hardening change that alters a JSON shape is out of scope by
  definition. All of this is **Phase 6**, after the client migration, so it
  never blocks it.
- **Owner-check:** every H1-H9 item's golden fixture still matches after the
  change; the Phase 6 gate runs the fixture suite green.

**R6 — Two apps in one repo.**
The legacy Widgets app and the new Quick app coexist during the rebuild
(Section 7.4), risking confusion over which is authoritative or accidental
edits to the wrong surface.
- **Likelihood:** Low-Medium (a process/discipline risk, not a technical one).
- **Impact:** Low (bounded by the build flag; both link the same `witscore`).
- **Mitigation:** `-DBUILD_LEGACY_WIDGETS=ON` (default) keeps the rollback
  alive and clearly delimited until **Phase 6 removes it** (Sections 6.6/7.6;
  the flag default flips to `OFF` only at the Phase 6 deletion milestone —
  never a "Phase 5 deletes Widgets" step).
- **Owner-check:** at Phase 6, the flag defaults `OFF`, the legacy sources are
  deleted, and the 12 ctest targets (now on `core/` paths) plus the new
  ViewModel/Quick tests are all green with the Widgets app gone.

### 19.2 New risks surfaced in Sections 2-18

| # | Risk | Likelihood | Impact | Mitigation | Owner-check |
|---|---|---|---|---|---|
| N1 | **Zero-config default palette changes** from today's blue/green to maroon/gold (Section 12.4) — a school that never opens Settings gets a different out-of-the-box look than the old Widgets app. | Low | Low | Deliberate, reasoned decision (Section 12.4): 2.0 is a full presentation rebuild with no shipped-look continuity to protect for a never-branded install; `fallbackPalette()` itself is unchanged, only `ThemeViewModel`'s seeding differs. | Owner confirms in the Phase 0 sign-off that maroon/gold is the intended unbranded default; Phase 2 kiosk walkthrough shows it. |
| N2 | **Department-list duplication does not fully resolve** — shrinks from three fetchers to two (`DashboardViewModel`/`ReportsViewModel` via `ReportController`, `StudentsViewModel` via `StudentController`), not one (Sections 3.6/9). | Low | Low | Accepted and documented (Section 9): a single shared department service is a plausible later consolidation, not mandated by this Phase 0 architecture; `adminWindow::loadDepartments`'s hand-rolled third fetcher disappears when `adminWindow` is deleted in Phase 6. | No owner action required in 2.0; noted so a future contributor does not read it as an oversight. |
| N3 | **Extrapolated tokens have no design reference** — `Theme.elevation.modal` (Section 12.7), `toastIn`/`toastOut` motion (Section 15.1), and the `LToast`/`LDialog` treatments derived from them. | Medium | Low | Flagged as **Phase-1 human-review items** at the point the components are built, same posture as dark mode (R3); values are extrapolated from the existing one-shot easing curve, not invented wholesale. | Phase 1 component-library walkthrough signs off the modal/toast look explicitly. |
| N4 | **Login has no controller today** — `KioskViewModel` owns a **net-new `AuthController`** (Section 10.1) wrapping the three login endpoints that are ad hoc `QNetworkAccessManager` calls in `MainWindow` today (`mainwindow.cpp:180-235,299-342`). This is the one module with no existing controller to reuse. | Medium | Medium | Build `AuthController` in Phase 2 following the established injected-`QNetworkAccessManager` + async-signals pattern (Section 2.2); it wraps existing frozen endpoints (`student_login.php`/`admin_login.php`/`rfid_login.php`, Section 16.2), so there is no new wire contract. | Phase 2 adds a `tst_authcontroller`-style unit test using synthetic payloads (the no-live-network house rule, Section 10.8); kiosk parity checklist covers all three login paths. |
| N5 | **Student-attendance Visit Logs backend is deferred** — the design's time-in/time-out Visit Logs screen ships in Phase 3 presenting **guest-log data** until a net-new additive endpoint lands (Sections 5.9/16.4/18.8). | Medium | Low-Medium | Explicitly a product decision made in Section 18.8: surface (a) (guest log, `get_visitors.php`) ships now; surface (b) (student attendance) is additive Phase-6-scoped backend work behind the same `ApiConfig::endpoint(...)` seam, **not a 2.0 blocker**; the same `VisitLogsViewModel` widens when it lands (no second ViewModel). | Owner acknowledges in Phase 0 sign-off that the Visit Logs screen initially shows guest sign-ins; the student-attendance endpoint is tracked as additive backend work, not a migration dependency. |
| N6 | **`Theme.success` (`#10B981`) has no design reference** — carried from `fallbackPalette()` (`brandtheme.cpp:47`), tonally green against a maroon/gold palette (Section 12.1). | Low | Low | Flagged for the Phase-5 a11y/dark-mode review round to revisit if a success/confirmation state actually ships; the recommended warm error role (`#A33B34`, Section 12.1) already addresses the analogous error case. | Phase 5 review confirms or replaces the success color if any `LToast severity: Success` surface is built. |

The recurring theme across N3/N5/N6 and R3 is honest: wherever the design
references do not answer a question (dark mode, modal/toast chrome, a success
color, a student-attendance backend), this document names the gap and routes
it to a **specific later gate** (Phase 1 component review, Phase 5 a11y round,
or additive Phase-6 backend work) rather than papering over it with an
invented value — consistent with the whole document's "cite it or flag it"
discipline.

## 20. Phased implementation roadmap

This reproduces and elaborates the spec §9 roadmap — **Phase 0 → 1 → 2 → 3 →
4 → 5 → 6**, in that exact numbering — giving each phase its deliverables, its
gate, and its parity/verification criteria. The phase boundaries are the spec's
verbatim; this section adds the screen-to-phase mapping (Section 18), the
risk-retirement mapping (Section 19), and the per-phase parity detail.

**The per-phase gate is the same four checks at every boundary** (spec §8),
stated once here and referenced as "the gate" below: (1) Debug **and** Release
builds clean with no new warnings; (2) full `ctest` green — the **12 existing
targets in every phase** (Section 2.7/8) plus that phase's new ViewModel/Quick
tests; (3) `/claude-review` to APPROVE or the round cap; (4) a **human visual
walkthrough** (premium look is a human judgment, spec §8). Phase 0 is the
exception — its gate is owner approval, below.

### Phase 0 — this document (hard stop at owner approval)

- **Deliverable:** this comprehensive modernization proposal (Sections 1-20:
  repo/debt/design/UX assessments, migration strategy, architecture, folder
  structure, ViewModels, component library, design/theme/navigation/animation
  systems, backend reuse + hardening plan, screen-by-screen redesigns, risks,
  this roadmap). **No implementation code** (spec §2, "Process").
- **Gate:** **owner approval — a hard stop.** Phase 1 does not begin until the
  owner approves this document. This is the single gate that is not the
  four-check build/test/review/walkthrough gate, because Phase 0 produces a
  document, not a build.
- **Verification criteria:** every codebase claim cites a real `file:line`
  (the document's own standing rule); the locked decisions (spec §2) are
  elaborated within, not relitigated; the open questions the design references
  cannot answer are each routed to a named later gate (Section 19).

### Phase 1 — `witscore` extraction + AppShell/Theme + RFID spike

- **Deliverables:** (a) extract the Widgets-free core into the `witscore`
  static library (Section 7.1) so **both apps build and 12/12 ctest stays
  green** — the core move is `git mv` + CMake path update, not a rewrite
  (Sections 6.2/7.6); (b) stand up `AppShell.qml`, the `Theme.qml` singleton
  (light/dark + brand-engine wiring, Sections 12/13) and `ThemeViewModel`, and
  the ten `L*` core components (Section 11); (c) the **RFID-in-QML spike** —
  reuse `RfidScanDetector`, re-author the install/focus layer against
  `QQuickWindow` (Section 19 R1); (d) the **deployment-hardware render check**
  on the actual library PC, with the software-rendering fallback documented
  (R4).
- **Gate:** the gate. Nothing user-facing ships this phase.
- **Parity/verification:** 12/12 green after the move with each target still
  compiling its own controller `.cpp` directly (`tst_reportcontroller` still
  omits `reportrenderer.cpp`, Section 7.6); the RFID spike demonstrates
  detector behavior under Quick focus; render performance signed off on the
  deployment machine. Retires the two highest risks (R1, R4) before any screen
  depends on them, plus the N3 extrapolated-token review at component build.

### Phase 2 — Kiosk surface parity

- **Deliverables:** the kiosk surface — Kiosk login (18.1), Live attendance
  feed + hero + stat tiles (18.2), Guest flow (18.3) — built on
  `KioskViewModel` + `VisitLogModel`, including the net-new `AuthController`
  (Section 10.1/19 N4) over the three frozen login endpoints.
- **Gate:** the gate, **plus the kiosk parity checklist**.
- **Parity/verification:** the checklist enumerates today's kiosk behaviors and
  checks each against the new screen — student-ID vs. admin-key routing, the
  guest toggle (off by default), the live feed, and above all **"RFID behaves
  exactly as today" verified as observable behavior** (R1's owner-check), not
  assumed from code reuse. Adds `KioskViewModel`/`AuthController` unit tests
  (synthetic payloads, Section 10.8) on top of the 12-target baseline.

### Phase 3 — Admin part 1: dashboard, visit logs, search

- **Deliverables:** Admin dashboard (18.4, net-new over existing report data,
  `DashboardViewModel` + `LStatTile`/`LBarChart`), Visit logs with the
  Today/This-Week `LSegmented` (18.8, `VisitLogsViewModel` over the guest log —
  surface (a)), and Search with Ctrl+K (18.7, `StudentsViewModel` search-only
  instance + `Navigator` shortcut).
- **Gate:** the gate, plus the per-module parity checklists.
- **Parity/verification:** dashboard figures match `ReportController`'s existing
  aggregates; the Visit Logs guest-log fetch/filter/export matches
  `VisitorController` behavior with the segmented control replacing the
  four-mode combo; Ctrl+K reaches Search from anywhere (the gap Section 5.8
  confirms is absent today). **Surface (b)** — student-attendance Visit Logs —
  is *not* a Phase 3 deliverable: it depends on the additive, Phase-6-scoped
  endpoint (18.8/19 N5) and does not block this phase.

### Phase 4 — Admin part 2: database + import, reports, settings

- **Deliverables:** Database/students with import-as-action (18.5,
  `StudentsViewModel` Database instance + `ImportController`), Reporting with
  `reportrenderer` export reuse and the new all-departments path (18.6,
  `ReportsViewModel`), and Settings with **live logo re-theming** (18.9,
  `SettingsViewModel` + `ThemeViewModel`).
- **Gate:** the gate, plus the per-module parity checklists.
- **Parity/verification:** bulk import + duplicate check + photo-ZIP upload and
  inline edit preserved; PDF/Excel exports match `ReportRenderer`'s current
  output; logo import re-themes the whole UI live via property bindings with no
  restart (Section 13.2). At this point every in-scope screen exists in Quick;
  the Widgets app is still the rollback (R6).

### Phase 5 — Dark-mode polish + a11y pass

- **Deliverables:** the derived dark palette finalized (Sections 6.5/13.4), the
  accessibility pass across the component library (`Accessible.*` roles,
  keyboard navigation, reduce-motion, Sections 11/14.4/15.2).
- **Gate:** the gate, **with the human walkthrough weighted heavily** — this is
  the budgeted human-review round for the items with no design reference.
- **Parity/verification:** dark-mode values pass the contrast thresholds
  (`MinContrast = 3.0`; 4.5 for body text) and read as maroon/gold at night
  (R3's owner-check); full keyboard navigation on admin (spec §7); the N6
  success-color decision is confirmed or replaced. **This phase does not delete
  the Widgets app** — deletion is Phase 6.

### Phase 6 — Backend hardening + Widgets deletion + final review

- **Deliverables:** (a) the Section 17 targeted backend hardening (H1-H9 +
  §17.3), **contract-tests-first** across the endpoints — parameterize
  `get_visitors.php`, add server-side `requireAdminAuth` to the destructive
  endpoints, remove the debug scripts (`phpinfo.php`/`testupload.php`), resolve
  `hash_admin.php` as a no-op, and revisit the hardcoded base URL; (b) **delete
  the Widgets app** — `-DBUILD_LEGACY_WIDGETS` default flips to `OFF` and the
  legacy sources (`mainwindow.*`, `adminwindow.*`, `guestwindow.*`,
  `attachfilesdialog.*`, `rfidkeyboardfilter.*`, `theme.h`, `wits.qss`) are
  removed (Sections 6.6/7.6); (c) final whole-branch `/claude-review`. The
  optional `witscore`/`witscore_reports` split (Section 7.1 option (b)) becomes
  available here once the second `Qt::Widgets` consumer is gone. If the
  additive student-attendance endpoint (19 N5) is built, it lands in this
  phase's additive backend scope.
- **Gate:** the gate, plus the human walkthrough, plus **every H1-H9 golden
  fixture still matching** (R5's owner-check).
- **Parity/verification:** hardening is provably behavior-preserving (no
  contract drift, Section 17.1); the 12 ctest targets (now on `core/` paths)
  plus all ViewModel/Quick tests are green **with the Widgets app deleted**
  (R6's owner-check) — the exit condition of Strategy A (spec §2).

### Roadmap at a glance

| Phase | Screens / deliverables (Section 18) | Gate | Risks retired (Section 19) |
|---|---|---|---|
| **0** | This proposal (Sections 1-20); no code | **Owner approval — hard stop** | Documents all; routes open questions to later gates |
| 1 | `witscore` extraction (12/12 green); AppShell + Theme + `L*` components; RFID spike; hardware render check | the gate | R1, R4, N3 |
| 2 | Kiosk: login (18.1), feed/hero/stats (18.2), guest (18.3); `AuthController` | the gate + kiosk parity checklist | R1 (owner-check), N4 |
| 3 | Admin pt 1: dashboard (18.4), visit logs — guest surface (18.8a), search (18.7) | the gate + parity checklists | — (N5 deferred, not blocking) |
| 4 | Admin pt 2: database + import (18.5), reports (18.6), settings + live re-theme (18.9) | the gate + parity checklists | — |
| 5 | Dark-mode polish + a11y pass | the gate + weighted human walkthrough | R3, N6 |
| 6 | Backend hardening (Section 17); **delete Widgets app**; final whole-branch review | the gate + fixtures + walkthrough | R5, R6; N5 additive endpoint (optional) |

Phase 0 ends at owner approval before Phase 1 begins (spec §2/§9). Widgets
deletion is **Phase 6**, never Phase 5; the legacy app survives behind
`-DBUILD_LEGACY_WIDGETS` as the rollback until then (Sections 6.6/7.6).

---

**Closing — the Phase 0 hard stop.** This document is the entire Phase 0
deliverable, and it contains no implementation code by design. **No
implementation begins — no `witscore` extraction, no QML, no CMake surgery, no
backend change — until the owner approves this proposal.** Approval is the
single gate between this document and Phase 1; it is a hard stop, not a
formality. Once approved, work proceeds one phase at a time through the roadmap
above, each phase clearing its four-check gate, with the Widgets app retained as
the rollback until Phase 6.
