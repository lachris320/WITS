# Design Spec: VisitorController Extraction

**Date:** 2026-07-02
**Status:** Approved
**Scope:** Step 2 of the WITS admin window structural refactor

---

## Context

`adminWindow` is still a ~4,000-line God Object. Step 1 (PR #8) extracted the
Settings tab into `SettingsController` + `SettingsData`. This spec extracts the
**Visitor Logs tab** (~280 lines) into a `VisitorController` plus value types,
making `adminWindow` a pure View for that tab.

The visitor logic currently lives in three places in `qt-app/adminwindow.cpp`:

- **Constructor wiring** (lines 639–714): table header setup, month/year combo
  population, filter-widget visibility toggling (lines 658–677), the search-button
  lambda that computes date ranges (lines 679–708), and the sidebar auto-load
  `loadVisitorLogs("", "all", "", "")` (lines 710–714).
- **`loadVisitorLogs(...)`** (lines 3677–3759): POSTs JSON to
  `ApiConfig::endpoint("get_visitors.php")`, parses the response, and populates
  `ui->visitorTable` and `ui->visitorTotalLabel` directly.
- **`on_extractVisitorBtn_clicked()`** (lines 3761–3881): builds a default
  filename from the filter widgets, opens `QFileDialog`, and writes the QXlsx
  report by reading cells back out of `ui->visitorTable`.

**Goal:** behavior-identical extraction — no new features, no regressions, no
wire-protocol change. This is the **second** controller extraction and it
establishes the template for network-using controllers (`StudentController` and
`ReportsController` follow the same shape next). Unlike Step 1, unit tests ship
with this step: the controller's pure statics are designed to be testable
without a live network, per the project workflow rule.

---

## Architecture

```
                 adminWindow  (View only)
                      │
      ┌───────────────┼────────────────────┐
      │               │                    │
collectVisitorFilter()  populateVisitorTable()  on_extractVisitorBtn_clicked()
      │               ▲                    │
      │  VisitorFilter │ visitorsLoaded    │ QList<VisitorRecord> + VisitorFilter
      ▼               │                    ▼
              VisitorController
                ├── fetchVisitors(filter)      → POST get_visitors.php
                ├── exportToExcel(records, …)  → QXlsx
                ├── wireDateType() / monthRange()
                ├── defaultExportFileName()
                └── parseVisitorsResponse()
                      │
                      ▼
          QNetworkAccessManager (injected, shared)
```

---

## New Files

| File | Purpose |
|------|---------|
| `qt-app/visitordata.h` | Plain value types — `DateFilterType`, `VisitorRecord`, `VisitorFilter` |
| `qt-app/visitorcontroller.h` | QObject controller — declaration |
| `qt-app/visitorcontroller.cpp` | QObject controller — implementation |
| `qt-app/tests/tst_visitorcontroller.cpp` | Qt Test target for the controller |

`adminwindow.h`, `adminwindow.cpp`, `qt-app/CMakeLists.txt`, and
`qt-app/tests/CMakeLists.txt` are modified (not replaced).

All new headers use `#ifndef` include guards (`VISITORDATA_H`,
`VISITORCONTROLLER_H`) — project convention, matching `settingscontroller.h`
and `settingsdata.h`. **Not** `#pragma once`.

---

## `visitordata.h` — Value Types

Plain value containers with no logic, no persistence, and no Qt dependency
beyond `<QString>`.

```cpp
enum class DateFilterType { All, SpecificDay, Month, DateRange };

struct VisitorRecord
{
    QString name;
    QString company;
    QString purpose;
    QString date;   // "yyyy-MM-dd" or "N/A"
    QString time;   // "hh:mm AP"  or "N/A"
};

struct VisitorFilter
{
    QString        search;
    DateFilterType dateType = DateFilterType::All;
    QString        startDate;   // "yyyy-MM-dd", empty for All
    QString        endDate;     // "yyyy-MM-dd", empty for All
};
```

A default-constructed `VisitorFilter{}` means "all visitors, no search" — it is
the exact equivalent of today's `loadVisitorLogs("", "all", "", "")` call
(adminwindow.cpp:713).

---

## `VisitorController` — Public API

```cpp
class VisitorController : public QObject
{
    Q_OBJECT
public:
    explicit VisitorController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Async — result arrives via visitorsLoaded / fetchError.
    void fetchVisitors(const VisitorFilter &filter);

    // Synchronous QXlsx export. Returns xlsx.saveAs(filePath).
    bool exportToExcel(const QList<VisitorRecord> &records,
                       const VisitorFilter &filter,
                       const QString &schoolName,
                       const QString &filePath);

    // Pure, unit-testable statics
    static QString wireDateType(DateFilterType t);
    static QPair<QString, QString> monthRange(int month, int year);
    static QString defaultExportFileName(const VisitorFilter &f);
    static bool parseVisitorsResponse(const QByteArray &json,
                                      QList<VisitorRecord> *out,
                                      int *totalCount,
                                      QString *errorMsg);

signals:
    void visitorsLoaded(const QList<VisitorRecord> &records, int totalCount);
    void fetchError(const QString &title, const QString &message);
};
```

### Key design decisions

- **`QNetworkAccessManager` is injected, not owned.** The constructor takes
  `adminWindow`'s existing `networkManager` member (adminwindow.h:78). The app
  shares one manager across all requests; the controller must not create a
  second one. Ownership stays with `adminWindow` (Qt parent), the controller
  only holds the pointer.
- **`DateFilterType` replaces the strings flowing from the combo box.**
  Critical behavior note: today the search lambda sends the combo box
  **display text verbatim** as the `date_type` wire value — `"Specific Day"`,
  `"Month"`, `"Date Range"`, or the fallback `"all"` (adminwindow.cpp:681,
  700–702). `wireDateType()` must reproduce those exact strings so the wire
  protocol to `get_visitors.php` is unchanged. The View maps display text →
  enum at the edge (in `collectVisitorFilter()`); the controller maps enum →
  wire string.
- **The controller works only with `VisitorRecord`/`VisitorFilter` — never
  `ui->`.** `exportToExcel` takes a `QList<VisitorRecord>`, not the table
  widget. The View caches the last list received from `visitorsLoaded` in a new
  member `m_visitorRecords` and passes that to `exportToExcel`. This is
  behavior-equivalent because `visitorTable` is not user-sortable or editable
  (`NoEditTriggers`, adminwindow.cpp:3742) — the table contents are always
  identical to the last fetch.
- **`fetchError` carries a title and a message.** Today's three failure paths
  use two different titles and two different severities: network error →
  `QMessageBox::critical(this, "Network Error", reply->errorString())`
  (adminwindow.cpp:3694); invalid JSON → `QMessageBox::warning(this, "Error",
  "Invalid server response.")` (line 3704); `status != "success"` →
  `QMessageBox::warning(this, "Error", obj["message"])` (line 3710). A
  single-string signal could not preserve both titles, so `fetchError(title,
  message)` emits `"Network Error"` for the network case and `"Error"` for the
  other two. The View shows `QMessageBox::critical` when the title is
  `"Network Error"` and `QMessageBox::warning` otherwise — pixel-identical to
  today, icon included.
- **All user-facing message text is preserved verbatim** — see the table in
  "Responsibilities" below.

---

## `VisitorController` — Responsibilities

### `fetchVisitors(const VisitorFilter &filter)`
- Builds the JSON payload `{search, date_type, start_date, end_date}` where
  `date_type` is `wireDateType(filter.dateType)` — identical to the payload
  built today at adminwindow.cpp:3684–3688.
- POSTs to `ApiConfig::endpoint("get_visitors.php")` with
  `ContentTypeHeader = "application/json"` via the injected manager.
- On `QNetworkReply::finished`:
  - network error → emits `fetchError("Network Error", reply->errorString())`;
  - otherwise runs `parseVisitorsResponse` on the body — on failure emits
    `fetchError("Error", errorMsg)`, on success emits
    `visitorsLoaded(records, totalCount)`.
- **The `finished` connection MUST pass the controller as the 3rd-arg context
  object**: `connect(reply, &QNetworkReply::finished, this, [this, reply]{...})`
  where `this` is the `VisitorController`. Today's code gets lifetime safety
  from `this` = `adminWindow` (adminwindow.cpp:3692) — the connection
  auto-disconnects when the receiver dies. The controller is destroyed during
  `~adminWindow`, but a reply parented to the *shared* `QNetworkAccessManager`
  can outlive it; a context-less lambda would fire after controller
  destruction and dereference a dangling pointer. The context arg preserves
  the auto-disconnect.
- Calls `reply->deleteLater()` on every path (matching lines 3695, 3700).
- Overlapping in-flight fetches (e.g. rapid Search clicks) are **preserved
  as-is**: each call creates an independent reply and last-to-finish wins the
  table, exactly as today (adminwindow.cpp:3690 has no cancellation either).
  The extraction neither fixes nor worsens this; request cancellation is a
  possible follow-up, not part of this behavior-preserving step.
- Never touches a widget.

### `parseVisitorsResponse(json, out, totalCount, errorMsg)` — static
Pure function over a `QByteArray`; unit tests feed it synthetic payloads.
- Document is not a JSON object → returns `false`, `*errorMsg =
  "Invalid server response."` (exact text from line 3704).
- `obj["status"] != "success"` → returns `false`,
  `*errorMsg = obj["message"].toString()` (line 3710).
- Success → fills `*out` from `obj["visitors"]` and
  `*totalCount = obj["count"].toInt()` (line 3715), returns `true`.
- Per record: `name`/`company`/`purpose` from the same-named fields (missing
  fields become empty strings via `toString()`); `time_in` is parsed with
  `QDateTime::fromString(timeIn, "yyyy-MM-dd HH:mm:ss")` and split into
  `date = dt.toString("yyyy-MM-dd")` and `time = dt.toString("hh:mm AP")`,
  both `"N/A"` when the datetime is invalid (lines 3727–3731).

### `wireDateType(DateFilterType t)` — static
Exact legacy wire strings:

| Enum | Wire string |
|------|-------------|
| `All` | `"all"` |
| `SpecificDay` | `"Specific Day"` |
| `Month` | `"Month"` |
| `DateRange` | `"Date Range"` |

### `monthRange(int month, int year)` — static
Returns `{firstDay, lastDay}` as `"yyyy-MM-dd"` strings, computed exactly as
the search lambda does today (adminwindow.cpp:691–694):
`QDate firstDay(year, month, 1); QDate lastDay(year, month, firstDay.daysInMonth());`.

### `defaultExportFileName(const VisitorFilter &f)` — static
Reproduces the filename logic at adminwindow.cpp:3768–3795, returning the full
`"<base>_<datePart>.xlsx"` string:
- Base is `"VisitorLogs"`; if `f.search` is non-empty (already trimmed by the
  View), spaces are replaced with underscores and the result is appended as
  `"VisitorLogs_<search>"`.
- `datePart` by filter type:
  - `SpecificDay` → `f.startDate` (the specific day, `"yyyy-MM-dd"`).
  - `Month` → `"<MonthName>_<year>"` (e.g. `"January_2026"`). Today this comes
    from the month combo's display text (line 3776); the filter no longer
    carries that text, so the month name is derived from `f.startDate` (always
    `"yyyy-MM-01"` in Month mode): parse the date, format the month with
    `QLocale::c().toString(date, "MMMM")` and take the year from the date. The
    C locale guarantees the same English month names the combo box hardcodes
    (adminwindow.cpp:643–644), regardless of system locale.
  - `DateRange` → `"Range_<startDate>_to_<endDate>"`.
  - `All` → `"All"`.
- An internal static helper `datePartForFilter(const VisitorFilter &)`
  (private) computes `datePart`; it is shared by `defaultExportFileName` and
  `exportToExcel` so the "Filter Applied" cell and the filename can never
  drift apart.

### `exportToExcel(records, filter, schoolName, filePath)`
All QXlsx code moves here, behavior-verbatim from adminwindow.cpp:3810–3875:
- Formats: `boldCenter` (bold, h-centered); `headerFormat` (bold, h-centered,
  `Qt::lightGray` pattern background, thin border); `normalCenter` (h-centered,
  thin border).
- `schoolName` fallback: if the passed string is empty, use `"School Name"`
  (line 3828). The View passes `ui->schoolName->text()` unmodified; the
  fallback lives in the controller.
- Merged cells `A1:E1` (school name) and `A2:E2` (title
  `"Library Visitor Logs Report"`), both `boldCenter`.
- Row 4: `"Generated On:"` / `QDateTime::currentDateTime().toString("MMMM dd, yyyy hh:mm AP")`.
- Row 5 `"Filter Applied:"` / `datePart` — written **only when
  `datePart != "All"`** (line 3842). Note the Month datePart keeps its
  underscore (`"January_2026"`), exactly as today.
- Row 6 `"Search Keyword:"` — written **only when `filter.search` is
  non-empty** (line 3846). Cell value is `filter.search` with underscores
  replaced by spaces. (Today the code space→underscores the term for the
  filename and then underscores→spaces it back for this cell — line 3848 —
  which also turns any *original* underscores into spaces. The replace
  preserves that pre-existing quirk.)
- Table headers at `startRow = 8`, columns 1–5, `headerFormat`, using the
  static list `{"Name", "Company", "Purpose", "Date", "Time"}` — the same
  strings the table headers carry (line 3720).
- Data rows from `records` (name, company, purpose, date, time per column),
  `normalCenter`, starting at row 9.
- Column width 25 for columns 1–5 (line 3869).
- `lastRow = startRow + records.size() + 2`: `"Total Visitors:"` (`boldCenter`)
  in column 1, `records.size()` (`normalCenter`) in column 2 — note this is the
  **row count**, not the server `count` field, matching line 3873.
- Returns `xlsx.saveAs(filePath)`. No dialogs, no message boxes.

---

## `adminWindow` Changes

### New members

```cpp
// In adminwindow.h (private section)
VisitorController *m_visitorController;   // child of this, created in ctor
QList<VisitorRecord> m_visitorRecords;    // cache of the last visitorsLoaded payload
```

Created in the constructor body as
`m_visitorController = new VisitorController(networkManager, this);` —
**after** `networkManager` is initialized, since the controller borrows it.

### New private methods / slots

```cpp
VisitorFilter collectVisitorFilter() const;
void populateVisitorTable(const QList<VisitorRecord> &records, int totalCount); // slot for visitorsLoaded
void onVisitorFetchError(const QString &title, const QString &message);         // slot for fetchError
```

### `collectVisitorFilter()`

Reads the filter widgets and maps display text → enum at the edge, reproducing
the search lambda's date math (adminwindow.cpp:679–705):

- `search` = `ui->visitorSearchLineEdit->text().trimmed()`.
- Combo text `"Specific Day"` → `SpecificDay`; `startDate = endDate =
  ui->visitorDateEdit->date().toString("yyyy-MM-dd")`.
- `"Month"` → `Month`; `{startDate, endDate} =
  VisitorController::monthRange(ui->visitorMonthCombo->currentIndex() + 1,
  ui->visitorYearSpin->value())`.
- `"Date Range"` → `DateRange`; start/end from `ui->visitorStartDate` /
  `ui->visitorEndDate`, formatted `"yyyy-MM-dd"`.
- Anything else (i.e. `"All"`) → `All`; dates empty.

### `populateVisitorTable(records, totalCount)`

Slot connected to `visitorsLoaded`. Contains everything `loadVisitorLogs`
currently does to widgets (adminwindow.cpp:3717–3757), unchanged:
- Caches `m_visitorRecords = records`.
- `setRowCount(records.size())`, `setColumnCount(5)`, header labels
  `{"Name", "Company", "Purpose", "Date", "Time"}` — the per-load table
  re-setup (resize mode `Stretch`, alternating colors, `NoEditTriggers`,
  `SelectRows`, `SingleSelection`) is redundant on repeat loads but is kept
  exactly as today to stay behavior-preserving.
- Fills the five columns straight from the record fields (the controller
  already did the date/time formatting).
- Sets `ui->visitorTotalLabel` to `QString("📊 Total Visitors: %1").arg(totalCount)`
  (the emoji stays — pre-existing, out of scope to change).
- Empty result: `setRowCount(0)`, `showTemporaryOverlay(ui->visitorTable,
  "No visitors found for the selected filters.")`, label forced to
  `"📊 Total Visitors: 0"`.

### `onVisitorFetchError(title, message)`

`QMessageBox::critical(this, title, message)` when `title == "Network Error"`,
otherwise `QMessageBox::warning(this, title, message)` — reproducing the three
current cases exactly (adminwindow.cpp:3694, 3704, 3710).

### Signal wiring (constructor)

```cpp
connect(m_visitorController, &VisitorController::visitorsLoaded,
        this, &adminWindow::populateVisitorTable);
connect(m_visitorController, &VisitorController::fetchError,
        this, &adminWindow::onVisitorFetchError);
```

Same-thread direct connections — no `Q_DECLARE_METATYPE` /
`qRegisterMetaType` needed for `QList<VisitorRecord>`.

### Call-site changes

- The search-button lambda (adminwindow.cpp:679–708) shrinks to
  `m_visitorController->fetchVisitors(collectVisitorFilter());` — its date
  math moves into `collectVisitorFilter()` / `monthRange()`.
- The sidebar auto-load (adminwindow.cpp:713) becomes
  `m_visitorController->fetchVisitors(VisitorFilter{});`.
- The widget-visibility toggle lambda (adminwindow.cpp:658–677) **stays in the
  View untouched** — it is pure widget choreography.
- `on_extractVisitorBtn_clicked()` shrinks to:
  1. Guard: `if (m_visitorRecords.isEmpty())` →
     `QMessageBox::information(this, "No Data", "There are no visitor logs to export.")`
     and return. (Guard moves from `visitorTable->rowCount() == 0` to the
     cache — equivalent, since the table always mirrors the cache.)
  2. `const VisitorFilter filter = collectVisitorFilter();` — read at export
     time, matching today's behavior of deriving filename/filter rows from the
     *current* widget state even if the user changed filters without
     re-searching.
  3. `QFileDialog::getSaveFileName(this, "Export Visitor Logs",
     QDir::homePath() + "/" + VisitorController::defaultExportFileName(filter),
     "Excel Files (*.xlsx)")` — the dialog stays in the View.
  4. Append `".xlsx"` if the chosen path lacks it (line 3807).
  5. `m_visitorController->exportToExcel(m_visitorRecords, filter,
     ui->schoolName->text(), filePath)` — on `true`,
     `QMessageBox::information(this, "Export Successful",
     QString("Visitor logs exported successfully!\n\nSaved as:\n%1").arg(filePath))`;
     on `false`, `QMessageBox::critical(this, "Error",
     "Failed to save the Excel file.")` — both texts verbatim from
     lines 3876–3880.

### What is removed from `adminWindow`

- `loadVisitorLogs(...)` — declaration (adminwindow.h:147) and definition
  (adminwindow.cpp:3677–3759) deleted outright.
- All QXlsx code and the filename-derivation block inside
  `on_extractVisitorBtn_clicked` (moved to the controller).
- The date-range math inside the search-button lambda (moved to
  `collectVisitorFilter()` / `monthRange()`).

The controller never opens a dialog or touches `ui->`. The View never builds a
network request or writes a spreadsheet cell.

---

## Data Flow

### On Visitor Logs sidebar click (auto-load)
```
sidebar lambda
  → stackedWidget->setCurrentWidget(visitorPage); setActiveSidebar(...)
  → m_visitorController->fetchVisitors(VisitorFilter{})        // All, no search
        → POST get_visitors.php {"search":"", "date_type":"all",
                                 "start_date":"", "end_date":""}
        → parseVisitorsResponse(body)
        → emits visitorsLoaded(records, count)   or   fetchError(title, msg)
  → adminWindow::populateVisitorTable(records, count)
        → m_visitorRecords = records; fill table; set total label
```

### On Search clicked
```
search lambda
  → collectVisitorFilter()                 // widgets → VisitorFilter (enum + dates)
  → m_visitorController->fetchVisitors(filter)
        → wireDateType(filter.dateType)    // "Specific Day"/"Month"/"Date Range"/"all"
        → POST, parse, emit — as above
```

### On Extract clicked
```
adminWindow::on_extractVisitorBtn_clicked()
  → guard m_visitorRecords.isEmpty()  → "No Data" info box, return
  → filter = collectVisitorFilter()
  → defaultExportFileName(filter)     → e.g. "VisitorLogs_Juan_Dela_Cruz_January_2026.xlsx"
  → QFileDialog::getSaveFileName(...) → filePath  (return if empty; append ".xlsx")
  → m_visitorController->exportToExcel(m_visitorRecords, filter,
                                       ui->schoolName->text(), filePath)
        → writes workbook, returns xlsx.saveAs(filePath)
  → success info box / failure critical box (texts unchanged)
```

---

## Tests — `tst_visitorcontroller`

New Qt Test target in `qt-app/tests/`. No live network — all
`parseVisitorsResponse` cases use synthetic `QByteArray` payloads with
synthetic names only (e.g. `"Juan Dela Cruz"`, `"Acme Corp"`), per the project
workflow and security-hygiene rules.

Coverage:

- **`parseVisitorsResponse`**: success payload with a valid
  `"time_in": "2026-06-15 14:30:00"` (asserts date `"2026-06-15"`, time
  `"02:30 PM"`); invalid JSON → `false` + `"Invalid server response."`;
  `status != "success"` → `false` + the payload's `message` field; malformed
  `time_in` → date/time both `"N/A"`; missing `name`/`company`/`purpose` →
  empty strings; `count` field propagated to `*totalCount`.
- **`monthRange`**: June 2026 → `{"2026-06-01", "2026-06-30"}`; February of a
  leap year (2024) → `{"2024-02-01", "2024-02-29"}`; December →
  `{"yyyy-12-01", "yyyy-12-31"}`.
- **`wireDateType`**: all four exact strings (`"all"`, `"Specific Day"`,
  `"Month"`, `"Date Range"`).
- **`defaultExportFileName`**: All / SpecificDay / Month / DateRange variants;
  with and without a search term; spaces → underscores
  (`"Juan Dela Cruz"` → `"VisitorLogs_Juan_Dela_Cruz_All.xlsx"`); Month
  variant produces `"January_2026"`-style names from a `"2026-01-01"`
  startDate.
- **`exportToExcel`**: write to a `QTemporaryDir` path, assert the return is
  `true`, then re-open the file with `QXlsx::Document` and assert the school
  name in A1 (including the `"School Name"` fallback when an empty name is
  passed), the `"Library Visitor Logs Report"` title in A2, the header row at
  row 8, a data cell, and the `"Total Visitors:"` row value.

---

## Files to Modify in CMake

### `qt-app/CMakeLists.txt`

Add to the `qt_add_executable(WITS ...)` source list, next to the settings
files:

```cmake
visitordata.h
visitorcontroller.h visitorcontroller.cpp
```

No new Qt modules — `Qt::Network` and `QXlsx` are already linked to `WITS`.

### `qt-app/tests/CMakeLists.txt`

Register the sixth target following the existing pattern (the five current
targets compile the units under test directly from `${CMAKE_SOURCE_DIR}`, the
way `tst_rfidscandetector` pulls in `rfidscandetector.cpp`; header-only units
like `apiconfig.h` come in via `target_include_directories`):

```cmake
qt_add_executable(tst_visitorcontroller
    tst_visitorcontroller.cpp
    ${CMAKE_SOURCE_DIR}/visitorcontroller.cpp
    ${CMAKE_SOURCE_DIR}/visitorcontroller.h
    ${CMAKE_SOURCE_DIR}/visitordata.h
)
set_target_properties(tst_visitorcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_visitorcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_visitorcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    QXlsx
)
add_test(NAME tst_visitorcontroller COMMAND tst_visitorcontroller)
set_tests_properties(tst_visitorcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

`QT_QPA_PLATFORM=offscreen` is required, not optional: QXlsx links `Qt::Gui`
and propagates it, so `QTEST_MAIN` instantiates a `QGuiApplication`, which
needs a platform plugin on a headless runner. The three existing Gui-touching
targets (`tst_rfidkeyboardfilter`, `tst_theme`, `tst_responsive_ui`) set it
for exactly this reason — follow them. The flip side: do NOT "optimize" this
target down to Core-only; `QXlsx::Format` font/color handling in the
`exportToExcel` test needs the `QGuiApplication` that the Gui link provides.

The `QXlsx` target is available because the parent `CMakeLists.txt` runs
`add_subdirectory(libs/QXlsx)` before `add_subdirectory(tests)`.

---

## Out of Scope (This Step)

- Student, Reports, and Database tab extractions (they follow this template).
- Any change to `get_visitors.php` or the wire protocol — `date_type` keeps
  its legacy mixed-case strings.
- UI layout changes to the Visitor Logs tab.
- Fixing pre-existing oddities, which are preserved deliberately:
  - the `"📊"` emoji in the total label;
  - the redundant per-load table re-setup (`setColumnCount`, header labels,
    resize mode, selection flags on every fetch) — kept exactly as today
    inside `populateVisitorTable`;
  - the underscore→space round-trip in the export's "Search Keyword" cell;
  - unmanaged overlapping in-flight fetches (last-to-finish wins the table) —
    see the `fetchVisitors` section.
- Backend URL configurability (separate spec; `ApiConfig` is used as-is).

---

## Verification Criteria

The extraction is complete when:

1. The app builds with no new warnings or errors, and
   `ctest --test-dir qt-app/build --output-on-failure` is green — all 5
   existing targets plus `tst_visitorcontroller`.
2. The Visitor Logs tab behaves identically to before:
   - Opening the tab from the sidebar auto-loads all visitor logs.
   - Each date filter mode ("All", "Specific Day", "Month", "Date Range")
     shows/hides the correct widgets and fetches the correct range on Search.
   - Search by keyword works, combined with any date filter.
   - An empty result shows the "No visitors found for the selected filters."
     overlay and `"📊 Total Visitors: 0"`.
   - Export produces an `.xlsx` with the identical layout (merged title rows,
     Generated On, conditional Filter Applied / Search Keyword rows, headers
     at row 8, column width 25, total row) and the correct default filename
     for every filter/search combination.
   - With the backend down, Search shows the `"Network Error"` critical box
     with the network error string.
3. `adminWindow` contains no `QNetworkAccessManager` POSTs and no JSON parsing
   for the visitor tab (`loadVisitorLogs` is gone).
4. `adminWindow` contains no QXlsx code for the visitor export.
5. `VisitorController` contains no `ui->` access, no `QFileDialog`, and no
   `QMessageBox` calls.
