# Design Spec: StudentController Extraction

**Date:** 2026-07-03
**Status:** Approved
**Scope:** Step 4 of the WITS admin window structural refactor

---

## Context

`adminWindow` is still a large God Object. Step 1 (PR #8) extracted the
Settings tab into `SettingsController` + `SettingsData`. Step 2 (PR #10)
extracted the Visitor Logs tab into `VisitorController` + `visitordata.h`.
Step 3 (PR #11) extracted the Database Import tab into `ImportController` +
`importdata.h`. This spec extracts the **Student Search tab** (search,
edit-in-place bulk update, and the department/course filter combos) into a
`StudentController` plus value types, making `adminWindow` a pure View for
that tab.

The **live** Student-Search logic currently lives in these places in
`qt-app/adminwindow.cpp`:

- **`performStudentSearch(bool showOverlay)`** (lines 3153–3244): POSTs
  `search_students.php` with `{search, department, course}`, then decodes the
  reply into five distinct outcome branches (network error / not-object /
  status≠success / empty students / results), showing (or suppressing, per
  `showOverlay`) the matching overlay for each and calling
  `displaySearchResults` on success.
- **`displaySearchResults(const QJsonArray&, const QString&)`** (lines
  3246–3274): fills `ui->studentSearchTable` from the `students` array
  (columns 0–8), `blockSignals` around the fill, resets the Select-All button
  label. **Stays View-side.**
- **`onSearchBtnClicked()`** (lines 3277–3280): calls
  `performStudentSearch(true)`.
- **`bulkUpdateStudents(const QJsonArray&)`** (lines 2986–3048): POSTs
  `bulk_update_students.php` with `{"students":[…]}`, decodes the reply,
  shows the success/failure overlay, the delayed `"Some Updates Failed"`
  message box, resets edit-mode UI, and calls `performStudentSearch(false)`
  for a silent refresh.
- **`onEditStudentBtnClicked()`** (lines 2834–2976): the edit-mode state
  machine — enters edit mode (checkboxes, editable cells, overlays) and, in
  update mode, collects the `updates` `QJsonArray` from the **live** table
  and calls `bulkUpdateStudents`. **Stays View-side** (reads live widget
  state; col 2 = school_id is non-editable).
- **`clearCheckboxes()`** (lines 2978–2984): removes the checkbox cell
  widgets. **Stays View-side.**
- **`onCancelEditBtnClicked()`** (lines 3051–3077): exits edit mode, locks
  cells, shows the cancel overlay, and calls `performStudentSearch(false)`.
  **Stays View-side.**
- **`on_selectAllBtn_clicked()`** (lines 3079–3151): toggles all row
  checkboxes and colors, updates the button label, shows feedback overlays.
  **Stays View-side.**
- **`onStudentTableItemChanged(QTableWidgetItem*)`** (lines 3707–3722):
  highlights an edited row soft-yellow and records it in `editedRows`.
  **Stays View-side.**
- **`populateFilters()`** (lines 3359–3385): GET `get_departments.php`,
  populates `searchDepartmentFilter` with the `"Select Department"`
  placeholder plus each department whose `.toLower() != "all"`. Called from
  the constructor at line 458.
- **`onDepartmentFilterChanged(int)`** (lines 3478–3508): GET
  `get_courses.php?department=<dept>`, repopulates `searchCourseFilter` with
  the `"Select Course"` placeholder plus each course. Connected at line 719.

**Goal:** behavior-identical extraction — no new features, no regressions, no
wire-protocol change. This is the **fourth** controller extraction, following
`SettingsController` (PR #8), `VisitorController` (PR #10), and
`ImportController` (PR #11). Wire endpoints (`search_students.php`,
`bulk_update_students.php`, `get_departments.php`, `get_courses.php`),
payload shapes, every overlay/dialog text (emojis included), and table
population remain byte-identical. Unit tests ship with this step, per the
project workflow rule.

### The one deliberate deviation (decision already made)

Today the constructor wires **two** live handlers to
`searchDepartmentFilter::currentIndexChanged` — an inline lambda at
adminwindow.cpp:460–494 **and** the `onDepartmentFilterChanged` slot
(connected at line 719) — so every department change issues **two**
`get_courses.php` requests. This branch **collapses that to one** (delete the
inline lambda; keep a single rewritten `onDepartmentFilterChanged` that
delegates to `StudentController::loadCourses`). This is the branch's **one
deliberate, annotated deviation** from byte-identical behavior: the observable
end state (the course combo's contents in every case) is preserved exactly,
and only the redundant second network request is removed (request volume
2 → 1). See "Collapsing the double course-fetch" below for the exact
end-state preservation, and the Notes section for the full analysis.

### Dead code deleted in this step (decision already made: "extract live, delete dead")

This branch also **deletes** the never-reached Student-tab code. None of it
is extracted:

- **`deleteStudents(const QStringList&)`** (lines 3282–3324) — DEAD. Its only
  caller is the dead `onDeleteStudentBtnClicked`. `delete_students.php` is
  therefore **not extracted at all**. Deleted.
- **`onDeleteStudentBtnClicked()`** (lines 3326–3357) — DEAD. `deleteStudentBtn`
  exists in `adminwindow.ui` but has **no** `connect()` in `adminwindow.cpp`,
  and the slot name does not match Qt's `on_<widget>_<signal>` auto-connect
  pattern, so it never fires. Deleted.
- **`loadAllStudents()`** (lines 3445–3476) — DEAD (never called anywhere).
  Deleted. Not extracted.
- **`showSearchOverlay()`** (lines 3387–3420) — DEAD (never called). Deleted.
- **`hideSearchOverlay()`** (lines 3422–3443) — DEAD (never called). Deleted.
- The members that become unused once the above are gone:
  `BusyIndicator *searchSpinner` (adminwindow.h:133), `QGraphicsOpacityEffect
  *overlayEffect` (adminwindow.h:135), and their `= nullptr` assignments in
  the constructor (adminwindow.cpp:444–445). Removed. The `BusyIndicator`
  **class** (`busyindicator.h`/`.cpp`) is **not** deleted — it is simply no
  longer used by `adminWindow`. **Careful: `overlayText` (adminwindow.h:134)
  sits between the two deleted members — delete exactly lines 133 and 135,
  not the range. `overlayText` stays** (its liveness is out of scope for
  this branch).
- The now-dead declarations in `adminwindow.h`: `loadAllStudents()` (h:118),
  `deleteStudents(const QStringList&)` (h:125), `showSearchOverlay()` /
  `hideSearchOverlay()` (h:136–137), and `onDeleteStudentBtnClicked()`
  (h:165). Removed.
- **`deleteStudentBtn` stays in `adminwindow.ui`** unwired, exactly as today.
  We do **not** touch the `.ui` (it carries the user's own uncommitted edit;
  NEVER stage/modify it). The button remains present-but-unwired — no behavior
  change, because it never worked.

---

## Architecture

```
                 adminWindow  (View only)
                      │
   ┌──────────┬───────┼─────────────┬──────────────────┐
   │          │       │             │                   │
onSearchBtnClicked  studentSearchTable   edit-mode state   department/course
   │  (search fill/collect)  machine          combos
   │          │       ▲             │                   │
   │ search/dept/course  QList<StudentRecord> QList<StudentRecord>  QStringList
   │          │       │  + SearchOutcome     updates            depts/courses
   ▼          ▼       │             ▼                   ▼
                 StudentController
                   ├── normalizeFilter()          → pure
                   ├── parseSearchResponse()       → pure
                   ├── parseBulkUpdateResponse()   → pure
                   ├── parseDepartments()          → pure
                   ├── parseCourses()              → pure
                   ├── searchStudents()            → POST search_students.php
                   ├── bulkUpdateStudents()        → POST bulk_update_students.php
                   ├── loadDepartments()           → GET  get_departments.php
                   └── loadCourses()               → GET  get_courses.php
                         │
                         ▼
             QNetworkAccessManager (injected, shared)
```

---

## New Files

| File | Purpose |
|------|---------|
| `qt-app/studentdata.h` | Plain value types — `StudentRecord`, `BulkUpdateResult`, `SearchOutcome` |
| `qt-app/studentcontroller.h` | QObject controller — declaration |
| `qt-app/studentcontroller.cpp` | QObject controller — implementation |
| `qt-app/tests/tst_studentcontroller.cpp` | Qt Test target for the controller |

`adminwindow.h`, `adminwindow.cpp`, `qt-app/CMakeLists.txt`, and
`qt-app/tests/CMakeLists.txt` are modified (not replaced).

All new headers use `#ifndef` include guards (`STUDENTDATA_H`,
`STUDENTCONTROLLER_H`) — project convention, matching `visitordata.h` /
`visitorcontroller.h` and `importdata.h` / `importcontroller.h`. **Not**
`#pragma once`.

---

## `studentdata.h` — Value Types

Plain value containers with no logic beyond field storage.

```cpp
// One search-result row. Field names chosen to match the JSON keys the
// backend returns / consumes: code, school_id, name, course, department,
// year_level, gender, status.
struct StudentRecord
{
    QString code;
    QString schoolId;
    QString name;
    QString course;
    QString department;
    QString yearLevel;
    QString gender;
    QString status;
};

// Decoded bulk_update_students.php response.
struct BulkUpdateResult
{
    bool        ok = false;
    int         updatedCount = 0;
    QStringList errors;
    QString     message;
};

// The five distinct branches of performStudentSearch's reply handler, so the
// View can show the exact original overlay for each.
enum class SearchOutcome
{
    Results,          // status success, students non-empty
    Empty,            // status success, students empty
    NotSuccess,       // obj["status"] != "success"
    InvalidResponse,  // body is not a JSON object
    NetworkError      // reply->error() != NoError
};
```

`StudentRecord` maps 1:1 to a `students[]` object from `search_students.php`
and back to the `updates[]` object shape `onEditStudentBtnClicked` builds
(adminwindow.cpp:2942–2950). The struct carries all eight fields so a single
type serves both the search-result direction (fill the table) and the
bulk-update direction (serialize back).

`BulkUpdateResult` is the decoded `bulk_update_students.php` response:
`ok` is `obj["status"] == "success"`, `updatedCount` is
`obj["updated"].toInt()`, `errors` is each entry of `obj["errors"]` array as
a string, and `message` is `obj["message"].toString()` (only populated on the
failure branch). The View uses `errors` to decide whether to fire the delayed
`"Some Updates Failed"` message box.

`SearchOutcome` reproduces the five branches at adminwindow.cpp:3181,
3195, 3206, 3224, and 3236 so the View can drive every overlay branch. There
is **no `DeleteResult` type** — the delete flow is dead and deleted.

---

## `StudentController` — Public API

```cpp
class StudentController : public QObject
{
    Q_OBJECT
public:
    explicit StudentController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeFilter(const QString &comboText);
    static SearchOutcome parseSearchResponse(const QByteArray &raw,
                                             QList<StudentRecord> &outRecords,
                                             QString &outMessage,
                                             QString &outSearchTerm);
    static BulkUpdateResult parseBulkUpdateResponse(const QByteArray &raw);
    static QStringList parseDepartments(const QByteArray &raw);
    static QStringList parseCourses(const QByteArray &raw);

    // Async — result arrives via searchFinished / searchFailed.
    void searchStudents(const QString &search,
                        const QString &department,
                        const QString &course);

    // Async — result arrives via bulkUpdateFinished / bulkUpdateFailed.
    void bulkUpdateStudents(const QList<StudentRecord> &updates);

    // Async — result arrives via departmentsLoaded (empty on error/!success).
    void loadDepartments();

    // Async — result arrives via coursesLoaded (empty on error/!success).
    void loadCourses(const QString &department);

signals:
    void searchFinished(SearchOutcome outcome,
                        const QList<StudentRecord> &records,
                        const QString &message,
                        const QString &searchTerm);
    void searchFailed(const QString &errorString);
    void bulkUpdateFinished(const BulkUpdateResult &result);
    void bulkUpdateFailed(const QString &errorString);
    void departmentsLoaded(const QStringList &departments);
    void coursesLoaded(const QStringList &courses);

private:
    QNetworkAccessManager *m_nam;   // injected, not owned — adminWindow keeps ownership
};
```

### Key design decisions

- **`QNetworkAccessManager` is injected, not owned** — same convention as
  `VisitorController` / `ImportController`. The constructor takes
  `adminWindow`'s existing `networkManager` member.
- **Network error travels via a dedicated signal, not through the parser.**
  `searchStudents` emits `searchFailed(reply->errorString())` on the
  `reply->error() != NoError` path, and `searchFinished(...)` (with a
  non-`NetworkError` outcome) otherwise. The alternative — folding the network
  error into `searchFinished` with `outcome == SearchOutcome::NetworkError` —
  was rejected because the network-error message is `reply->errorString()`,
  which does not fit the parser's out-parameters (`parseSearchResponse` never
  sees a `QNetworkReply`). Splitting network-error from parse-outcome mirrors
  how `VisitorController` splits `visitorsLoaded` from `fetchError` and
  `ImportController` splits `uploadFinished` from `uploadFailed`. The
  `SearchOutcome::NetworkError` enumerator is retained for completeness (it
  documents the branch that `searchFailed` represents) but is **not** emitted
  through `searchFinished`; the network method decides `NetworkError` from
  `reply->error()` **before** calling `parseSearchResponse`, and routes it to
  `searchFailed`.
- **`bulkUpdateStudents` and `searchStudents` split failure the same way** —
  `bulkUpdateFailed(reply->errorString())` on the network-error path,
  `bulkUpdateFinished(BulkUpdateResult)` otherwise.
- **`loadDepartments` / `loadCourses` have no error signal.** Legacy shows
  **no** dialog on a network or non-success response for either combo — it
  simply leaves the combo with only the placeholder (populateFilters:
  3372–3383 always adds `"Select Department"` first, then adds departments
  only inside the `status == "success"` guard; onDepartmentFilterChanged:
  3493–3505 only repopulates inside `error == NoError && status == "success"`).
  The extracted methods preserve that exactly: on error or `!success` they
  emit an **empty** `QStringList`, and the View — which always prepends the
  placeholder before appending the parsed list — produces the identical
  visible result (placeholder only). No error signal is needed or added.
- **The combo placeholders stay View-side.** `parseDepartments` /
  `parseCourses` return only the parsed entries; the View prepends
  `"Select Department"` / `"Select Course"` before appending them (matching
  populateFilters:3373 and onDepartmentFilterChanged:3499).
- **reply lifetime safety:** exactly as `VisitorController::fetchVisitors`,
  every `connect(reply, &QNetworkReply::finished, this, …)` passes the
  controller (`this`) as the 3rd-arg context object, so the connection
  auto-disconnects if the controller is destroyed while a reply — owned by the
  shared `QNetworkAccessManager` — is still in flight. `reply->deleteLater()`
  is called on every path (success, network error, parse error).
- **Purity:** `studentcontroller.cpp` contains no `ui->`, no `QMessageBox`,
  no `QFileDialog`, no `QTableWidget` access, and no `showTemporaryOverlay`
  reference anywhere.

---

## `StudentController` — Responsibilities

### `normalizeFilter(const QString &comboText)` — static

Reproduces the placeholder→`""` normalization at adminwindow.cpp:3171–3172:

```cpp
filters["department"] = (department == "All" || department == "All Departments" || department == "Select Department") ? "" : department;
filters["course"]     = (course == "All" || course == "All Courses" || course == "Select Course") ? "" : course;
```

A single helper handles both, because the two lists overlap on `"All"` and
are otherwise disjoint. `normalizeFilter` returns `""` when `comboText` is any
of the six placeholder strings (`"All"`, `"All Departments"`,
`"Select Department"`, `"All Courses"`, `"Select Course"`), else returns
`comboText` unchanged. Applied to `department` it reproduces line 3171
exactly; applied to `course` it reproduces line 3172 exactly (the union of
both placeholder sets is a strict superset of each, and no real department or
course value equals any of the six placeholders, so the merged predicate is
byte-for-byte equivalent in effect). `searchStudents` calls it on both the
`department` and `course` arguments before building the payload.

### `parseSearchResponse(raw, outRecords, outMessage, outSearchTerm)` — static

Pure port of the reply body of `performStudentSearch` (adminwindow.cpp:
3194–3242), minus the overlay/table side effects (which stay View-side):

- `QJsonDocument::fromJson(raw)`; if `!doc.isObject()` →
  return `SearchOutcome::InvalidResponse` (line 3195).
- `obj["status"].toString() != "success"` →
  `outMessage = obj.contains("message") ? obj["message"].toString() :
  "No students found."` (line 3207), return `SearchOutcome::NotSuccess`
  (line 3206).
- `obj["students"].toArray()` is empty → return `SearchOutcome::Empty`
  (line 3224).
- otherwise → fill `outRecords` from the `students` array, mapping each
  object's keys to a `StudentRecord`:
  `code` ← `code`, `schoolId` ← `school_id`, `name` ← `name`,
  `course` ← `course`, `department` ← `department`,
  `yearLevel` ← `year_level`, `gender` ← `gender`, `status` ← `status`
  (same keys `displaySearchResults` reads at lines 3260–3267);
  `outSearchTerm = obj["searchTerm"].toString()` (line 3242); return
  `SearchOutcome::Results`.

`NetworkError` is **not** produced by this function — the network method
decides it from `reply->error()` before calling the parser.

> **Note — `searchTerm` is vestigial but preserved.** Legacy
> `displaySearchResults` (adminwindow.cpp:3246–3274) receives the
> `searchTerm`/`highlightTerm` argument and never uses it — no highlighting
> is implemented. `outSearchTerm` is threaded through
> `parseSearchResponse` → `searchFinished` purely for parity with the legacy
> signature. Do **not** "restore" highlighting during this extraction — it
> never existed; that would be a behavior change.

### `parseBulkUpdateResponse(const QByteArray &raw)` — static

Pure port of the reply body of `bulkUpdateStudents` (adminwindow.cpp:
3017–3046):

- `QJsonDocument::fromJson(raw).object()`.
- `obj["status"].toString() == "success"` → `ok = true`,
  `updatedCount = obj["updated"].toInt()` (line 3021), and — when
  `obj.contains("errors") && !obj["errors"].toArray().isEmpty()` — `errors`
  = each entry of `obj["errors"]` array as `.toString()` (lines 3030–3035).
  `message` left empty.
- otherwise → `ok = false`, `message = obj["message"].toString()`
  (line 3045), `updatedCount = 0`, `errors` empty.

### `parseDepartments(const QByteArray &raw)` — static

Pure port of the `get_departments.php` handling in `populateFilters`
(adminwindow.cpp:3369–3383): if `obj["status"].toString() == "success"`,
collect every `obj["departments"]` entry whose `.toLower() != "all"`
(line 3379); otherwise return an empty list. The returned list is what the
View appends **after** its `"Select Department"` placeholder.

### `parseCourses(const QByteArray &raw)` — static

Pure port of the `get_courses.php` handling in `onDepartmentFilterChanged`
(adminwindow.cpp:3495–3503): if `obj["status"].toString() == "success"`,
return every `obj["courses"]` entry as `.toString()`; otherwise return an
empty list. The View appends this **after** its `"Select Course"` placeholder.

### `searchStudents(const QString &search, const QString &department, const QString &course)`

Port of the network half of `performStudentSearch` (adminwindow.cpp:
3164–3243):

- Builds `QNetworkRequest` to `ApiConfig::endpoint("search_students.php")`
  with `ContentTypeHeader = "application/json"` (lines 3165–3167).
- Builds the JSON body `{"search": search, "department":
  normalizeFilter(department), "course": normalizeFilter(course)}`
  (lines 3169–3172) — the raw `search` string is sent verbatim (already
  trimmed View-side, see the seam below); `department`/`course` are
  normalized via `normalizeFilter`.
- POSTs via the injected manager (line 3175).
- On `QNetworkReply::finished`:
  - `reply->error() != QNetworkReply::NoError` → emit
    `searchFailed(reply->errorString())` (maps to line 3185's
    `"❌ Network Error\n%1"` overlay, which the View reconstructs).
  - otherwise → run `parseSearchResponse(reply->readAll(), …)` and emit
    `searchFinished(outcome, records, message, searchTerm)`.
- `reply->deleteLater()` on every path (lines 3187, 3192).
- The `searchLoadingBar` show/hide/value plumbing (lines 3156–3157, 3179) is
  **View-owned** and set around the call — see the first seam below.

### `bulkUpdateStudents(const QList<StudentRecord> &updates)`

Port of the network half of `bulkUpdateStudents` (adminwindow.cpp:
2986–3047):

- Builds `QNetworkRequest` to
  `ApiConfig::endpoint("bulk_update_students.php")` with
  `ContentTypeHeader = "application/json"` (lines 2988–2990).
- Serializes each `StudentRecord` back into a JSON object with the **same key
  shape** `onEditStudentBtnClicked` built (lines 2942–2950):
  `school_id`, `code`, `name`, `department`, `course`, `year_level`,
  `gender`, `status`. The array is wrapped as `{"students": [ … ]}`
  (line 2993) and POSTed (lines 2995–2996).
- On `QNetworkReply::finished`:
  - `reply->error() != QNetworkReply::NoError` → emit
    `bulkUpdateFailed(reply->errorString())` (maps to line 3009's
    `"❌ Network Error\n%1"` overlay).
  - otherwise → run `parseBulkUpdateResponse(reply->readAll())` and emit
    `bulkUpdateFinished(result)`.
- `reply->deleteLater()` on every path (lines 3010, 3015).
- The View owns the edit-mode UI reset (lines 3000–3005), the success/failure
  overlay, the delayed `"Some Updates Failed"` box, and the silent refresh —
  see the second seam below.

### `loadDepartments()`

Port of the `get_departments.php` GET in `populateFilters` (adminwindow.cpp:
3362–3384):

- `networkManager->get(ApiConfig::endpoint("get_departments.php"))`.
- On `QNetworkReply::finished`: emit
  `departmentsLoaded(parseDepartments(reply->readAll()))` — an empty list on
  error or `!success` (legacy shows no dialog on either path; the View's
  placeholder-only combo is the identical visible result).
- `reply->deleteLater()` on every path.

> **Name-collision warning for the implementer:** two *other* live functions
> also GET `get_departments.php` and are **out of scope — do not extract,
> rename, or delete them**: `adminWindow::loadDepartments()`
> (adminwindow.cpp:1629, a different combo, still called from the ctor at
> line 457) and `adminWindow::loadFilterDepartments()` (adminwindow.cpp:1837).
> Only `populateFilters()` (cpp:3359, the Search-tab loader called from the
> ctor at line 458) is extracted into
> `StudentController::loadDepartments()`. After the refactor, both
> `adminWindow::loadDepartments()` (ctor:457) and
> `m_studentController->loadDepartments()` (via the rewritten
> `populateFilters`, ctor:458) run at startup — same as today's two distinct
> requests. The identical method name across the two classes is legal but a
> footgun: extract `populateFilters` and nothing else.

### `loadCourses(const QString &department)`

Port of the `get_courses.php` GET in `onDepartmentFilterChanged`
(adminwindow.cpp:3483–3507):

- Builds `ApiConfig::endpoint("get_courses.php")` with query item
  `department=<department>` (lines 3483–3488).
- `networkManager->get(request)`.
- On `QNetworkReply::finished`: emit
  `coursesLoaded(parseCourses(reply->readAll()))` — empty on error or
  `!success` (legacy shows no dialog on either path; only repopulates inside
  the `error == NoError && status == "success"` guard).
- `reply->deleteLater()` on every path (line 3506).

---

## The two seams (critical)

### 1. The `showOverlay` suppression flag stays View-side

Legacy `performStudentSearch(bool showOverlay)` suppresses **all** overlays on
silent refreshes — the `if (showOverlay)` guards at lines 3183, 3196, 3213,
3228, and 3236. `onSearchBtnClicked` calls it with `true`; the two
refresh-after-mutation callers (`bulkUpdateStudents`:3042 and
`onCancelEditBtnClicked`:3076) call it with `false`.

In the split, **the controller always emits `searchFinished` / `searchFailed`**
— the flag never enters the controller. The View keeps an internal `bool`
member (e.g. `m_studentSearchShowOverlay`) that it sets **immediately before**
calling `searchStudents`, and its `onSearchFinished` / `onSearchFailed` slots
honor that flag when deciding whether to call `showTemporaryOverlay`. Outcomes
are byte-identical to the legacy guards.

Two subtleties the View must preserve:

- **Table clearing on `NotSuccess` and `Empty` happens regardless of the
  flag.** Legacy clears the table (`clearContents(); setRowCount(0);`) on both
  branches (lines 3209–3210 and 3225–3226) **outside** the `if (showOverlay)`
  guard. Only the overlay text is gated. So the View clears the table
  unconditionally on those two outcomes and gates only the overlay.
- **`searchLoadingBar` is View-owned progress plumbing.** The View sets it
  visible + value 0 (lines 3156–3157) before calling `searchStudents`, and
  hides it (line 3179) in **both** its `onSearchFinished` and `onSearchFailed`
  slots (the legacy hide runs at the top of the `finished` lambda, before the
  error branch).

### 2. Refresh-after-mutation seam

Legacy `bulkUpdateStudents` (line 3042) and `onCancelEditBtnClicked`
(line 3076) both call `performStudentSearch(false)` — a silent refresh after a
successful update or a cancel.

In the split, the controller emits `bulkUpdateFinished(result)` and returns.
The **View owns the chaining**, exactly as `ImportController`'s
`duplicatesResolved` seam. The View's `onBulkUpdateFinished(result)` slot:

1. Resets the edit-mode button/checkbox UI (lines 3000–3005:
   `editStudentBtn` text `"Edit"` + enabled; `cancelEditBtn` /
   `selectAllBtn` hidden; `NoEditTriggers`; `clearCheckboxes()`).
2. If `result.ok`:
   - shows the success overlay `"✅ %1 student%2 updated successfully"`
     (lines 3024–3027);
   - if `!result.errors.isEmpty()`, fires the delayed box —
     `QTimer::singleShot(2500, this, [=]{ QMessageBox::warning(this,
     "Some Updates Failed", errorList); })` where `errorList` is the errors
     joined with a trailing `"\n"` per entry (lines 3030–3039);
   - then calls `performStudentSearch(false)` for the silent refresh
     (line 3042) — the surviving thin helper sets the show-overlay flag
     `false` and delegates to `searchStudents(...)` (see Task 3).
3. Else (`!result.ok`): shows `"⚠️ Update Failed\n%1"` with `result.message`
   (line 3045) and does **not** refresh.

`onBulkUpdateFailed(errorString)` shows `"❌ Network Error\n%1"` (line 3009)
after the same UI reset (the legacy reset at 3000–3005 runs before the
error-branch check at 3007).

`onCancelEditBtnClicked` stays entirely View-side (it never talked to the
network); its final `performStudentSearch(false)` call is unchanged — the
surviving thin helper sets the show-overlay flag `false` and delegates to
`searchStudents(...)` (see Task 3).

The View owns the chaining; `StudentController` never constructs a
`QMessageBox`, touches `ui->`, or calls `showTemporaryOverlay`.

---

## Collapsing the double course-fetch (the deliberate deviation)

Two legacy handlers fire on `searchDepartmentFilter::currentIndexChanged`, and
they behave **differently** on the `"Select Department"` (index 0) case — so
collapsing to one is not a blind delete; the surviving handler must reproduce
the **union** of both handlers' observable end state.

- **Inline lambda (adminwindow.cpp:460–494):** unconditionally
  `searchCourseFilter->clear()` + `addItem("Select Course")` (lines 462–463),
  then `if (index <= 0) return;` (line 465); otherwise GET `get_courses.php`
  and, on reply, `clear()` + `addItem("Select Course")` + the courses
  (lines 484–491).
- **`onDepartmentFilterChanged` slot (3478–3508):**
  `if (department.isEmpty() || department == "Select Department") return;`
  (line 3481) — returns **without** clearing the course combo; otherwise GET
  and, on reply, `clear()` + `addItem("Select Course")` + the courses
  (lines 3498–3503).

Net legacy behavior for the two cases:

- **Selecting `"Select Department"` (index 0):** the lambda clears the course
  combo down to `["Select Course"]`; the slot does nothing → end state
  `["Select Course"]`.
- **Selecting a real department (index > 0):** both fire, two `get_courses.php`
  GETs, last reply wins → end state `["Select Course", <courses…>]`.

**The collapsed handler (rewritten `onDepartmentFilterChanged`, inline lambda
deleted):**

```cpp
void adminWindow::onDepartmentFilterChanged(int)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");   // preserves the index-0 end state

    const QString dept = ui->searchDepartmentFilter->currentText();
    if (dept.isEmpty() || dept == "Select Department")
        return;                                          // index-0 / placeholder: combo already = ["Select Course"]

    m_studentController->loadCourses(dept);              // single GET (was two)
}

void adminWindow::onCoursesLoaded(const QStringList &courses)
{
    ui->searchCourseFilter->clear();
    ui->searchCourseFilter->addItem("Select Course");
    ui->searchCourseFilter->addItems(courses);
}
```

This reproduces `["Select Course"]` on the placeholder case (via the up-front
clear that the legacy *lambda* provided but the *slot* did not) and
`["Select Course", <courses…>]` on a real selection — **identical end state in
both cases**, with exactly **one** `get_courses.php` request instead of two.
The deviation is the removed second request only; nothing user-visible changes.
This must be annotated in the Task 3 commit and called out at review time as the
branch's single accepted deviation.

- **`onEditStudentBtnClicked`** — the entire edit-mode state machine: entering
  edit mode (checkboxes, per-row highlight lambdas, unlocking all columns
  **except col 2 = school_id** at lines 2882–2890, the `"📝 Edit Mode Active…"`
  overlay), and, in update mode, **collecting the `updates` from the LIVE
  table** (lines 2929–2953). This deliberately reads current widget state via
  the local `getText` lambda (`item->text().trimmed()`), so a hand-edit to a
  cell is picked up at update time. The View builds a
  `QList<StudentRecord>` (not a `QJsonArray`) and passes it to
  `m_studentController->bulkUpdateStudents(...)`. The empty-selection guard
  (`updates.isEmpty()` → `"⚠️ No students selected or edited."` overlay +
  reset, lines 2962–2971) stays View-side.
- **`clearCheckboxes`** (lines 2978–2984) — pure widget cleanup.
- **`on_selectAllBtn_clicked`** (lines 3079–3151) — all checkbox/color
  toggling and the `"✅ Selected all %1 student%2"` /
  `"✔ All students deselected"` overlays.
- **`onCancelEditBtnClicked`** (lines 3051–3077) — edit-mode teardown, cell
  locking, the `"❌ Edit mode cancelled\nNo changes were saved"` overlay, and
  the silent refresh (now a `searchStudents` call, flag `false`).
- **`onStudentTableItemChanged`** (lines 3707–3722) — soft-yellow highlight of
  an edited row and the `editedRows.insert(row)` bookkeeping.
- **`displaySearchResults`** (lines 3246–3274) — the table fill. Columns:
  0 = Select (empty `QTableWidgetItem("")`), 1 = code, 2 = school_id,
  3 = name, 4 = course, 5 = department, 6 = year_level, 7 = gender,
  8 = status. `blockSignals(true)` around the fill (lines 3249, 3270);
  `clearContents()` + `setRowCount(students.size())`; final
  `selectAllBtn->setText("Select All")` (line 3273). The View's
  `onSearchFinished` slot, on `SearchOutcome::Results`, iterates the
  `QList<StudentRecord>` and fills the table with the same column order
  (reading struct fields instead of JSON keys — byte-identical cell text).
- **The `searchLoadingBar` progress plumbing** (visible/value, lines
  3156–3157, 3179) — set around the `searchStudents` call.
- **The `showTemporaryOverlay` calls** — every overlay string listed in the
  Message/Dialog Inventory, shown by the View's slots.
- **The `"Select Department"` / `"Select Course"` combo placeholders** —
  prepended View-side before appending the parsed `departmentsLoaded` /
  `coursesLoaded` lists.
- **The `showOverlay` flag** (`m_studentSearchShowOverlay` bool member) and the
  existing `isSettingUpEditMode` / `editedRows` members.

---

## Message / Dialog Inventory

Every user-visible overlay/dialog in the **live** Student-Search flow, its
exact text (emojis verbatim), and which side owns it after extraction. All
overlays are `showTemporaryOverlay(ui->studentSearchPage, …)` calls; the one
`QMessageBox` is the delayed "Some Updates Failed" box.

| # | Trigger | Exact text | Kind | Owner after extraction |
|---|---|---|---|---|
| 1 | Edit clicked, table empty | `⚠️ No students to edit\nPlease search for students first` | overlay | View (`onEditStudentBtnClicked`) |
| 2 | Enter edit mode | `📝 Edit Mode Active\nDouble-click cells to edit (yellow highlight).\nChecked rows are selected for update (red highlight).` | overlay | View |
| 3 | Update clicked, nothing selected/edited | `⚠️ No students selected or edited.` | overlay | View |
| 4 | Bulk update: network error | `❌ Network Error\n%1` (`%1` = `errorString`) | overlay | View, from `bulkUpdateFailed(errorString)` |
| 5 | Bulk update: success | `✅ %1 student%2 updated successfully` (`%2` = `""`/`"s"`) | overlay | View, from `bulkUpdateFinished` where `result.ok` |
| 6 | Bulk update: success with errors[] | `Some Updates Failed` (body = errors joined, one per line) | `QMessageBox::warning`, delayed 2500 ms | View, from `bulkUpdateFinished` where `result.ok && !result.errors.isEmpty()` |
| 7 | Bulk update: status != success | `⚠️ Update Failed\n%1` (`%1` = `result.message`) | overlay | View, from `bulkUpdateFinished` where `!result.ok` |
| 8 | Cancel edit clicked | `❌ Edit mode cancelled\nNo changes were saved` | overlay | View (`onCancelEditBtnClicked`) |
| 9 | Select All | `✅ Selected all %1 student%2` (`%2` = `""`/`"s"`) | overlay | View (`on_selectAllBtn_clicked`) |
| 10 | Deselect All | `✔ All students deselected` | overlay | View |
| 11 | Search: network error (showOverlay) | `❌ Network Error\n%1` (`%1` = `errorString`) | overlay | View, from `searchFailed(errorString)` (gated by flag) |
| 12 | Search: body not a JSON object (showOverlay) | `⚠️ Invalid server response` | overlay | View, from `searchFinished(InvalidResponse, …)` (gated) |
| 13 | Search: status != success (showOverlay) | `🔍 %1` (`%1` = `message`, else `"No students found."`) | overlay | View, from `searchFinished(NotSuccess, …, message, …)` (gated; **table cleared regardless of flag**) |
| 14 | Search: students empty (showOverlay) | `🔍 No students found\nTry adjusting your search filters` | overlay | View, from `searchFinished(Empty, …)` (gated; **table cleared regardless of flag**) |
| 15 | Search: results (showOverlay) | `✅ Found %1 student%2` (`%2` = `""`/`"s"`) | overlay | View, from `searchFinished(Results, records, …)` (gated) |

Notes on the `%2` plural suffix: legacy uses `.arg(count == 1 ? "" : "s")`
at lines 3027, 3121, 3240 — reproduced verbatim (rows 5, 9, 15).
`departmentsLoaded` / `coursesLoaded` intentionally have **no** dialog on
error or non-success (legacy shows none) — see the design decision above.

---

## Data Flow

### On Search clicked
```
onSearchBtnClicked()  (or searchLineEdit returnPressed)
  → performStudentSearch(true)   [surviving thin helper — the next three
    View steps are its body]
  → View: searchLoadingBar visible + value 0
  → View: m_studentSearchShowOverlay = true
  → m_studentController->searchStudents(
        searchLineEdit.text().trimmed(),
        searchDepartmentFilter.currentText(),
        searchCourseFilter.currentText())
        → POST search_students.php {"search":…, "department":norm, "course":norm}
        → [network error]  emit searchFailed(errorString)
        → [otherwise]      parseSearchResponse(body)
                           emit searchFinished(outcome, records, message, searchTerm)
  → View onSearchFailed:   searchLoadingBar hidden;
                           if (m_studentSearchShowOverlay) overlay "❌ Network Error\n…" (row 11)
  → View onSearchFinished: searchLoadingBar hidden; branch on outcome:
        InvalidResponse → if (flag) overlay row 12
        NotSuccess      → clearContents+setRowCount(0);  if (flag) overlay row 13
        Empty           → clearContents+setRowCount(0);  if (flag) overlay row 14
        Results         → if (flag) overlay row 15; displaySearchResults(records, searchTerm)
```

### On Update clicked (edit mode → update mode)
```
onEditStudentBtnClicked()  [button text == "Update"]
  → View: selectAllBtn hidden
  → View: collect QList<StudentRecord> from LIVE studentSearchTable
          (checked rows, skip empty school_id, read current widget text)
  → [updates empty] overlay row 3 + reset edit UI + clearCheckboxes, return
  → m_studentController->bulkUpdateStudents(updates)
        → POST bulk_update_students.php {"students":[ {school_id,code,name,
          department,course,year_level,gender,status}, … ]}
        → [network error]  emit bulkUpdateFailed(errorString)
        → [otherwise]      parseBulkUpdateResponse(body)
                           emit bulkUpdateFinished(result)
  → View onBulkUpdateFailed:   reset edit UI; overlay row 4
  → View onBulkUpdateFinished: reset edit UI (rows editStudentBtn/cancelEditBtn/…)
        [result.ok]      overlay row 5;
                         if (!result.errors.isEmpty()) QTimer::singleShot(2500) row 6;
                         performStudentSearch(false)  [thin helper: flag=false,
                         loading bar, searchStudents(…)] — silent refresh
        [!result.ok]     overlay row 7 (no refresh)
```

### On Cancel Edit clicked
```
onCancelEditBtnClicked()
  → View: reset edit UI, lock cells, reset colors
  → overlay row 8
  → performStudentSearch(false)  [thin helper: flag=false, loading bar,
    searchStudents(…)] — silent refresh
```

### On department combo changed / initial filter population
```
constructor → m_studentController->loadDepartments()
        → GET get_departments.php
        → emit departmentsLoaded(parseDepartments(body))  (empty on error/!success)
  → View onDepartmentsLoaded: searchDepartmentFilter.clear();
        addItem("Select Department"); addItems(departments)

onDepartmentFilterChanged(index)   [SINGLE handler; inline lambda 460–494 deleted]
  → View: searchCourseFilter.clear(); addItem("Select Course")   // preserves index-0 end state
  → [dept == "" || dept == "Select Department"] return  (combo already = ["Select Course"])
  → m_studentController->loadCourses(department)
        → GET get_courses.php?department=<dept>   (ONE request; was two)
        → emit coursesLoaded(parseCourses(body))  (empty on error/!success)
  → View onCoursesLoaded: searchCourseFilter.clear();
        addItem("Select Course"); addItems(courses)
```

---

## Tests — `tst_studentcontroller`

New Qt Test target in `qt-app/tests/`. No live network — all response parsers
use synthetic `QByteArray` payloads with synthetic school IDs and names only
(e.g. `"2023-00123"`, `"Juan Dela Cruz"`, `"BSIT"`,
`"College of Computing Studies"`), per the project workflow and
security-hygiene rules. No real student PII, no personal machine paths, no
backend URLs.

Coverage:

- **`normalizeFilter`**: each of the six placeholders
  (`"All"`, `"All Departments"`, `"Select Department"`, `"All Courses"`,
  `"Select Course"`) → `""`; a real value (`"College of Computing Studies"`,
  `"BSIT"`) passes through unchanged.
- **`parseSearchResponse`**:
  - Results with N synthetic rows → `SearchOutcome::Results`, `outRecords`
    size N, every `StudentRecord` field asserted against the payload keys
    (`code`/`school_id`/`name`/`course`/`department`/`year_level`/`gender`/
    `status`), and `outSearchTerm` equal to the payload's `searchTerm`.
  - Empty `students` array with `status:"success"` → `SearchOutcome::Empty`.
  - `status != "success"` **with** a `message` → `SearchOutcome::NotSuccess`,
    `outMessage` = that message.
  - `status != "success"` **without** a `message` → `SearchOutcome::NotSuccess`,
    `outMessage == "No students found."`.
  - A non-object body (bare JSON array or malformed JSON) →
    `SearchOutcome::InvalidResponse`.
- **`parseBulkUpdateResponse`**:
  - `status:"success"` with `updated` and a non-empty `errors` array →
    `ok == true`, `updatedCount` = the count, `errors` = the array entries.
  - `status:"success"` with no `errors` → `ok == true`, `errors` empty.
  - `status != "success"` with a `message` → `ok == false`, `message`
    populated, `updatedCount == 0`.
- **`parseDepartments`**: `status:"success"` with a `departments` array
  containing `"all"`/`"All"` mixed with real values → the returned list skips
  `"all"`/`"All"` case-insensitively and keeps the rest;
  `status != "success"` → empty list.
- **`parseCourses`**: `status:"success"` with a `courses` array → the full
  list; `status != "success"` → empty list.

**Accepted limitation (matches the Import/Visitor precedent):** automated
coverage exercises only the five pure statics. The four network methods
(`searchFailed`-vs-`searchFinished` routing, emit ordering,
`reply->deleteLater()` on every path) have no `QSignalSpy`/mock-NAM tests —
the same convention as `tst_importcontroller` — and are verified by the
Task 3 purity/grep gates plus manual QA against the live backend.

**No `QApplication`/`QGuiApplication` is required.** The controller's tested
surface is pure (`QString`/`QByteArray`/`QJsonDocument`/`QList`/`QStringList`)
and links only `Qt::Core` + `Qt::Network`. `QTEST_MAIN` for a Core+Network
target instantiates a `QCoreApplication`, which needs **no** platform plugin,
so `QT_QPA_PLATFORM=offscreen` is **not** needed (see the CMake section for
the confirmation against the existing targets).

---

## Files to Modify in CMake

### `qt-app/CMakeLists.txt`

Add to the `qt_add_executable(WITS …)` source list, next to the import/visitor
files:

```cmake
studentdata.h
studentcontroller.h studentcontroller.cpp
```

No new Qt modules — `Qt::Network` is already linked to `WITS`.

### `qt-app/tests/CMakeLists.txt`

Register `tst_studentcontroller` — the **8th** ctest target — following the
existing controller pattern, but **without** QXlsx and **without** the
`QT_QPA_PLATFORM=offscreen` env:

```cmake
qt_add_executable(tst_studentcontroller
    tst_studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/studentcontroller.cpp
    ${CMAKE_SOURCE_DIR}/studentcontroller.h
    ${CMAKE_SOURCE_DIR}/studentdata.h
)
set_target_properties(tst_studentcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_studentcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_studentcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
)
add_test(NAME tst_studentcontroller COMMAND tst_studentcontroller)
set_tests_properties(tst_studentcontroller PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

**Why no `offscreen` here (verified against the existing targets):**
`tst_visitorcontroller` and `tst_importcontroller` set
`QT_QPA_PLATFORM=offscreen` **only because they link `QXlsx`**, which pulls in
`Qt::Gui`/`Qt::GuiPrivate`, causing `QTEST_MAIN` to instantiate a
`QGuiApplication` that needs a platform plugin on a headless runner (see
those targets' `target_link_libraries` including `QXlsx` at
`tst_visitorcontroller` lines 76–80 and `tst_importcontroller` lines 94–98,
each paired with the `offscreen` env). The two targets that link **only**
`Qt::Test` with no Gui/Widgets dependency — `tst_rfidscandetector` (lines
1–12) and `tst_apiconfig` (lines 29–38) — set **only**
`QT_FORCE_STDERR_LOGGING=1` and **no** `offscreen`. `StudentController` has
no QXlsx and no widget dependency, so it belongs with the latter group.
`QTEST_MAIN` with a Core+Network target uses `QCoreApplication`, which has no
platform-plugin requirement — `offscreen` is unnecessary and is deliberately
omitted. (The `tst_rfidkeyboardfilter` / `tst_theme` / `tst_responsive_ui`
targets set `offscreen` because they link `Qt::Widgets`/`Qt::UiTools`, which
is likewise not our case.)

---

## Task Decomposition

Mirrors the three-task shape used for the Import and Visitor extractions:

- **Task 1** — `studentdata.h` + `studentcontroller.h`/`.cpp` skeleton: value
  types (`StudentRecord`, `BulkUpdateResult`, `SearchOutcome`) and **all**
  pure statics (`normalizeFilter`, `parseSearchResponse`,
  `parseBulkUpdateResponse`, `parseDepartments`, `parseCourses`) implemented
  for real; the four network methods (`searchStudents`, `bulkUpdateStudents`,
  `loadDepartments`, `loadCourses`) stubbed (declared, empty/trivial bodies);
  CMake wiring for both the `WITS` target and `tst_studentcontroller`;
  `tst_studentcontroller` red → green on the statics.
- **Task 2** — implement the four network methods with their signal emissions
  and reply-lifetime handling (`this` as `finished` context object,
  `reply->deleteLater()` on every path).
- **Task 3** — wire `StudentController` into `adminWindow`: add the
  `m_studentController` member, construct it in the constructor **after**
  `networkManager` (alongside `m_visitorController` / `m_importController`,
  adminwindow.cpp:424–442), add the six slots
  (`onSearchFinished` / `onSearchFailed` / `onBulkUpdateFinished` /
  `onBulkUpdateFailed` / `onDepartmentsLoaded` / `onCoursesLoaded`), add the
  `m_studentSearchShowOverlay` bool member, and rewrite
  `performStudentSearch`/`onSearchBtnClicked`/`onEditStudentBtnClicked`
  (update branch)/`bulkUpdateStudents`/`onCancelEditBtnClicked`/
  `populateFilters`/`onDepartmentFilterChanged` to delegate to the controller.
  **`performStudentSearch(bool showOverlay)` survives as a thin private View
  helper** (its decl at adminwindow.h:113 stays): it sets
  `m_studentSearchShowOverlay = showOverlay`, shows `searchLoadingBar`
  (visible + value 0), reads the three filter widgets, and calls
  `m_studentController->searchStudents(…)` — so its three live call sites
  (`onSearchBtnClicked` → `true`; `bulkUpdateStudents`' success refresh and
  `onCancelEditBtnClicked` → `false`) remain literally unchanged. Keep
  `displaySearchResults`/`clearCheckboxes`/
  `on_selectAllBtn_clicked`/`onStudentTableItemChanged` as View. **Delete the
  redundant inline department-filter lambda at adminwindow.cpp:460–494** and
  rewrite `onDepartmentFilterChanged` as the single collapsed handler (see
  "Collapsing the double course-fetch") — this is the branch's one deliberate
  annotated deviation and must be called out in the commit body. **Delete** the
  dead code (`deleteStudents`, `onDeleteStudentBtnClicked`, `loadAllStudents`,
  `showSearchOverlay`, `hideSearchOverlay`, the `searchSpinner`/`overlayEffect`
  members + their ctor `= nullptr` lines at 444–445, and the dead
  `adminwindow.h` decls). Finish with a purity grep gate
  (`studentcontroller.cpp` has no `ui->`/`QMessageBox`/`QFileDialog`) and a
  no-dead-code grep gate (the five deleted functions are gone from
  `adminwindow.cpp`/`.h`, and only **one** `get_courses.php` call site remains).

**Branch:** `feat/student-controller-extraction` (off `master`).

---

## Out of Scope (This Step)

- The Reports tab extraction (it follows this template next).
- Any change to `search_students.php`, `bulk_update_students.php`,
  `get_departments.php`, or `get_courses.php` or their wire protocols —
  payload shapes are unchanged.
- Extracting `delete_students.php` — the delete flow is dead and **deleted**,
  not extracted.

  > **Scope amendment (2026-07-04, post-QA):** during manual QA the user
  > confirmed the Delete-Selected button had never functioned (dead on
  > `master`: no `connect()`, wrong slot name for auto-connect, no
  > show-on-edit), and explicitly chose to add the feature to **this** branch
  > rather than defer it. So `delete_students.php` *is* now wired — implemented
  > fresh through `StudentController` (`parseDeleteResponse` + `deleteStudents`
  > + `deleteFinished`/`deleteFailed`), not resurrected inline. This is a
  > deliberate deviation from the original "dead and deleted" decision above;
  > the extraction of the search/bulk-update flow is unchanged.
- UI layout changes to the Student Search tab, and any change to
  `adminwindow.ui` (it carries the user's own uncommitted edit; the
  `deleteStudentBtn` widget stays present-but-unwired).
- Backend URL configurability (separate concern; `ApiConfig` is used as-is).
- The two *other* `get_departments.php` callers —
  `adminWindow::loadDepartments()` (adminwindow.cpp:1629) and
  `adminWindow::loadFilterDepartments()` (adminwindow.cpp:1837) — belong to
  other tabs and are **untouched** despite the former sharing a name with
  `StudentController::loadDepartments()` (see the name-collision warning in
  the `loadDepartments()` section).

**In scope by explicit decision:** collapsing the duplicated department-filter
wiring from two `get_courses.php` requests to one — the branch's single
deliberate annotated deviation (see "Collapsing the double course-fetch"). The
course-combo end state is preserved exactly.

---

## Verification Criteria

The extraction is complete when:

1. The app builds with no new warnings or errors, and
   `ctest --test-dir qt-app/build --output-on-failure` is green — all 7
   existing targets plus `tst_studentcontroller` (8 total).
2. The Student Search tab behaves identically to before:
   - Searching populates `studentSearchTable` with the same columns, row
     count, and cell contents (rows 11–15 overlays fire on the same
     conditions, gated by the show-overlay flag; the table clears on the
     `NotSuccess`/`Empty` branches regardless of the flag).
   - A silent refresh after a successful bulk update or a cancel shows **no**
     search overlay (flag `false`), exactly as before.
   - Edit mode enters/exits with the same overlays (rows 1–3, 8), the same
     checkbox/color behavior, and school_id (col 2) non-editable.
   - Bulk update shows the success overlay (row 5), the delayed
     `"Some Updates Failed"` box when `errors` is non-empty (row 6), the
     failure overlay (row 7), or the network-error overlay (row 4), all with
     identical text.
   - Select All / Deselect All show rows 9–10 with identical counts and text.
   - The department combo populates with `"Select Department"` + departments
     (skipping `"all"` case-insensitively); selecting a department populates
     the course combo with `"Select Course"` + courses; selecting the
     `"Select Department"` placeholder resets the course combo to just
     `"Select Course"`; an error/non-success on either leaves the combo with
     only its placeholder (no dialog), exactly as before. **Only one**
     `get_courses.php` request fires per department change (the deliberate
     2 → 1 deviation), and the redundant inline lambda (old lines 460–494) is
     gone.
3. `adminWindow` contains no `QNetworkAccessManager` POST/GET calls and no
   JSON parsing for the Student tab (the network halves of
   `performStudentSearch`, `bulkUpdateStudents`, `populateFilters`, and
   `onDepartmentFilterChanged` are gone — they delegate to the controller).
4. `StudentController` contains no `ui->` access, no `QMessageBox`, no
   `QFileDialog`, no `QTableWidget` access, and no `showTemporaryOverlay`.
5. The five dead functions (`deleteStudents`, `onDeleteStudentBtnClicked`,
   `loadAllStudents`, `showSearchOverlay`, `hideSearchOverlay`) and the
   `searchSpinner`/`overlayEffect` members are gone from `adminwindow.cpp` and
   `adminwindow.h`; `adminwindow.ui` is untouched and `deleteStudentBtn`
   remains present-but-unwired.

---

## Global Constraints

- Qt 6.11, C++17, CMake+Ninja+MinGW. `#ifndef` header guards (NOT
  `#pragma once`). `m_` prefix for new members. Functor-based `connect`
  (no `SIGNAL`/`SLOT` macros). `QNetworkAccessManager` injected, not owned.
  `reply->deleteLater()` on every path; `finished` connected with the
  controller (`this`) as the context object. **This test target links only
  `Qt::Test` + `Qt::Network` — no QXlsx, no `QT_QPA_PLATFORM=offscreen`** (the
  tested surface is Core+Network-only; see the CMake section).
- Security-hygiene (binding): no secrets/admin keys/credentials/backend URLs
  in source, tests, CMake, or this spec; use placeholders. No real student PII
  in fixtures — synthetic only. No hardcoded `C:\Users\<name>` personal paths
  anywhere in committed files. NEVER stage/commit `qt-app/adminwindow.ui`
  (carries the user's own uncommitted edit) or `qt-app/build/`.

---

## Notes / discrepancies (code vs. brief)

Read against the actual `adminwindow.cpp`, all overlay/dialog strings,
payload keys, and JSON-decode branches the brief lists match the code
character-for-character (emojis included). Three points worth flagging:

1. **Two live wirings drive the course combo, not one — RESOLVED: collapse to
   one.** The constructor installs an inline lambda on
   `searchDepartmentFilter::currentIndexChanged` at lines 460–494 **in addition
   to** the `onDepartmentFilterChanged` slot (connected at line 719), so today
   every department change issues **two** `get_courses.php` requests. **Decision
   (made during spec review): collapse to a single request** — delete the inline
   lambda, keep one rewritten `onDepartmentFilterChanged` that delegates to
   `StudentController::loadCourses`. This is the branch's **one deliberate
   annotated deviation**; the course-combo end state is preserved exactly in
   both the placeholder (index 0) and real-department cases (see "Collapsing the
   double course-fetch"). Task 3 must annotate this in the commit body and flag
   it at review time.

2. **`onStudentTableItemChanged` overlaps the edit-mode lambda.** The
   constructor connects `studentSearchTable::itemChanged` to the
   `onStudentTableItemChanged` slot at line 718, and `onEditStudentBtnClicked`
   *also* `disconnect(...)`s all `itemChanged` handlers and installs its own
   inline lambda (lines 2898–2917) that does very similar soft-yellow
   highlighting plus auto-checking the row. Both are View-side and both stay;
   this is noted only so the implementer does not "unify" them and change
   edit-mode highlight behavior. No extraction action required.

3. **`loadAllStudents` is genuinely dead** (confirmed: no caller in
   `adminwindow.cpp`), and it is the only other place `search_students.php` is
   POSTed besides `performStudentSearch`. Deleting it does not affect the live
   search path. Its distinct payload (`department:"All"`, `course:"All"`) is
   not reproduced anywhere and is not needed — the live path normalizes
   `"All"` to `""` via `normalizeFilter`.
