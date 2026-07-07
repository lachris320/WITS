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

*[to be written in Task 2]*

## 5. Product UX assessment

*[to be written in Task 2]*

## 6. Qt Widgets → Qt Quick migration strategy

*[to be written in Task 6]*

## 7. Proposed project architecture

*[to be written in Task 3]*

## 8. Proposed folder structure

*[to be written in Task 3]*

## 9. Feature / module organization

*[to be written in Task 3]*

## 10. ViewModel architecture

*[to be written in Task 3]*

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
