# LOAMS 2.0 — Modernization Proposal (Phase 0)

**Date:** 2026-07-07
**Status:** Draft — pending owner approval
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

*[to be written in Task 7]*

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
   (e.g. `#1E293B`, `#F1F5F9`, `#E2E8F0` at `resources/wits.qss:6-12`) that
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
exactly the kind of duplication a single `DepartmentsViewModel`/shared
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
is a natural `DatabaseViewModel` responsibility (Section 10) — track "am I
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

*[to be written in Task 6]*

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
shrinks accordingly: the eleven business-logic file-pairs currently listed
inline (`settingscontroller.h/.cpp` through `brandingcontroller.h/.cpp`,
`qt-app/CMakeLists.txt:38-52`) are removed from the `WITS` target's source
list and replaced by the `witscore` link; only the genuinely Widgets-bound
files (`mainwindow.*`, `adminwindow.*`, `guestwindow.*`,
`attachfilesdialog.*`, `busyindicator.*`, `rfidkeyboardfilter.*`, the four
`.ui` files, `resources.qrc`) remain directly in `WITS`'s own source list.
Nothing about this changes `WITS`'s runtime behavior — it is a pure
"move the compilation unit, keep the include path working" change, backed by
the fact that all eleven moved files already build as standalone units today
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

*[to be written in Task 4]*

## 12. Design system

*[to be written in Task 4]*

## 13. Theme system

*[to be written in Task 4]*

## 14. Navigation architecture

*[to be written in Task 4]*

## 15. Animation guidelines

*[to be written in Task 4]*

## 16. Backend reuse plan

*[to be written in Task 5]*

## 17. Backend refactoring plan

*[to be written in Task 5]*

## 18. Screen-by-screen redesign recommendations

*[to be written in Task 6]*

## 19. Risk analysis

*[to be written in Task 6]*

## 20. Phased implementation roadmap

*[to be written in Task 6]*
