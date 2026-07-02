# Design Spec: ImportController Extraction

**Date:** 2026-07-02
**Status:** Approved
**Scope:** Step 3 of the WITS admin window structural refactor

---

## Context

`adminWindow` is still a large God Object. Step 1 (PR #8) extracted the
Settings tab into `SettingsController` + `SettingsData`. Step 2 (PR #10)
extracted the Visitor Logs tab into `VisitorController` + `visitordata.h`.
This spec extracts the **Database Import tab** (bulk student upload from
CSV/Excel, with optional photo ZIP) into an `ImportController` plus value
types, making `adminWindow` a pure View for that tab.

The import logic currently lives in six places in `qt-app/adminwindow.cpp`:

- **`onAttachFileBtnClicked()`** (lines 1090–1121): opens `AttachFilesDialog`,
  validates the returned paths, dispatches to the CSV or Excel preview
  loader, and builds a (currently unused/commented) summary string.
- **`onUpdateDatabaseBtnClicked()`** (lines 1123–1367): the big flow —
  validates attachments, collects school IDs from `ui->bulkTable`, POSTs
  `check_duplicates.php`, shows the duplicate dialog (first-3 preview +
  "Show More" + cancellation paths), builds a `QHttpMultiPart` upload to
  `upload_students_zip.php`, tracks progress, and parses the response.
- **`onCancelUploadBtnClicked()`** (lines 1369–1374): sets `cancelUpload`,
  shows an info box, clears the table.
- **`static normalizeHeader(const QString&)`** (lines 1376–1382): trims,
  lowercases, and strips space/underscore/hyphen from a header string.
- **`loadExcelToTable(const QString&)`** (lines 1384–1463): QXlsx parse →
  headers + rows into `ui->bulkTable`; builds `bulkHeaderIndex`.
- **`loadCSVtoTable(const QString&)`** (lines 1465–1542): text parse →
  headers + rows into `ui->bulkTable`; builds `bulkHeaderIndex`.

The header→field mapping if/else chain is **duplicated verbatim** between
`loadExcelToTable` and `loadCSVtoTable` (lines 1424–1443 and 1500–1519). This
extraction unifies it into one function.

**Goal:** behavior-identical extraction — no new features, no regressions, no
wire-protocol change. This is the **third** controller extraction, following
the `SettingsController` (PR #8) and `VisitorController` (PR #10) pattern.
Wire endpoints (`check_duplicates.php`, `upload_students_zip.php`), payload
shapes, every dialog text and severity, progress-bar behavior, and table
population remain byte-identical. Unit tests ship with this step, per the
project workflow rule.

---

## Architecture

```
                 adminWindow  (View only)
                      │
   ┌─────────┬────────┼─────────────┬──────────────────┐
   │         │        │             │                   │
onAttachFileBtnClicked  bulkTable populate   duplicate dialog   onUpdateDatabaseBtnClicked
   │         │        ▲             │                   │
   │  file path  ParsedTable │uploadProgress/    QStringList skipIds
   │         │        │      uploadFinished              │
   ▼         ▼        │             ▼                   ▼
              ImportController
                ├── parseCsv(rawText)              → pure
                ├── parseExcel(filePath)            → QXlsx
                ├── mapHeaders() / normalizeHeader() → pure
                ├── checkDuplicates(schoolIds)       → POST check_duplicates.php
                ├── uploadStudents(excel, zip, skip) → POST upload_students_zip.php
                └── parseDuplicateResponse() / parseUploadResponse() → pure
                      │
                      ▼
          QNetworkAccessManager (injected, shared)
```

---

## New Files

| File | Purpose |
|------|---------|
| `qt-app/importdata.h` | Plain value types — `ParsedTable`, `UploadResult` |
| `qt-app/importcontroller.h` | QObject controller — declaration |
| `qt-app/importcontroller.cpp` | QObject controller — implementation |
| `qt-app/tests/tst_importcontroller.cpp` | Qt Test target for the controller |

`adminwindow.h`, `adminwindow.cpp`, `qt-app/CMakeLists.txt`, and
`qt-app/tests/CMakeLists.txt` are modified (not replaced).

All new headers use `#ifndef` include guards (`IMPORTDATA_H`,
`IMPORTCONTROLLER_H`) — project convention, matching `visitordata.h` and
`visitorcontroller.h`. **Not** `#pragma once`.

---

## `importdata.h` — Value Types

Plain value containers with no logic beyond field storage.

```cpp
enum class ImportSeverity { Warning, Critical };

// Distinguishes the three legacy Excel-load failure dialogs so the View can
// show the exact original message + severity. None = a usable table was read.
enum class ExcelParseError { None, OpenFailed, NoSheets, EmptySheet };

struct ParsedTable
{
    QStringList headers;
    QList<QStringList> rows;
    QMap<QString, int> headerIndex;
};

struct UploadResult
{
    bool    ok = false;
    QString message;
    int     successCount = 0;
    int     errorCount = 0;
    bool    plainText = false;   // true when the response was not a JSON object
    QString rawText;             // populated only when plainText is true
};
```

`ParsedTable` is the parse result the View renders into `ui->bulkTable`:
`headers` becomes the horizontal header labels, `rows` becomes the table
cells (one `QStringList` per row, in file order), and `headerIndex` is the
zero-based column map the View caches for later (`school_id` collection at
upload time).

`UploadResult` is the decoded `upload_students_zip.php` response.
`plainText`/`rawText` preserve the current plain-text-fallback branch: when
the response body is not a JSON object, legacy code shows
`QMessageBox::information(this, "Upload Complete",
QString::fromUtf8(response))` (adminwindow.cpp:1353) instead of parsing
`status`/`message`/counts. `ok` is meaningless when `plainText` is true; the
View branches on `plainText` first.

`ImportSeverity` carries the icon severity for `ImportController::importError`
(see "Message / Dialog Inventory" below for why a title alone is not
enough to infer it, unlike `VisitorController::fetchError`).

---

## `ImportController` — Public API

```cpp
class ImportController : public QObject
{
    Q_OBJECT
public:
    explicit ImportController(QNetworkAccessManager *nam, QObject *parent = nullptr);

    // Pure, unit-testable statics
    static QString normalizeHeader(const QString &raw);
    static void    mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut);
    static ParsedTable parseCsv(const QString &rawText);

    // Synchronous QXlsx parse. Requires a QGuiApplication (see Testing).
    // errorOut (when non-null) reports which of the three legacy failure
    // cases occurred so the View can show the exact original dialog.
    ParsedTable parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr);

    // Async — result arrives via duplicatesResolved / importError.
    void checkDuplicates(const QStringList &schoolIds);

    // Async — result arrives via uploadProgress / uploadFinished / uploadFailed.
    void uploadStudents(const QString &excelPath, const QString &zipPath,
                        const QStringList &skipIds);

    // Pure response parsers
    static bool parseDuplicateResponse(const QByteArray &raw,
                                       QStringList *duplicatesOut,
                                       QString *errorOut);
    static UploadResult parseUploadResponse(const QByteArray &raw);

signals:
    void duplicatesResolved(const QStringList &duplicates);   // empty = none found
    void importError(const QString &title, const QString &message, ImportSeverity severity);
    void uploadProgress(int percent);
    void uploadFinished(const UploadResult &result);
    void uploadFailed(const QString &message);   // always critical, title "Upload Failed"
};
```

### Key design decisions

- **`QNetworkAccessManager` is injected, not owned** — same convention as
  `VisitorController`. The constructor takes `adminWindow`'s existing
  `networkManager` member.
- **The header→field mapping is unified.** Today `loadExcelToTable` (lines
  1424–1443) and `loadCSVtoTable` (lines 1500–1519) carry byte-identical
  if/else chains. `mapHeaders()` replaces both call sites; there is exactly
  one copy of the alias rules from here on.
- **`parseCsv` and `parseExcel` are asymmetric in mutability.** `parseCsv` is
  a pure static — it only needs a `QString`. `parseExcel` is a regular
  (non-static) method because `QXlsx::Document` construction and the
  `Qt::GuiPrivate` link it drags in are heavier; keeping it non-static costs
  nothing (the controller instance is cheap to hold) and matches the
  `exportToExcel` precedent in `VisitorController`, which is also
  non-static.
- **`checkDuplicates` does not decide what happens with the duplicates.** It
  only reports them. The View owns the entire duplicate-resolution dialog
  and decides the `skipIds` list passed to `uploadStudents` — see "The async
  dialog seam" below. This mirrors how `VisitorController::fetchVisitors`
  never decides what the View does with `visitorsLoaded`.
- **`importError(title, message, severity)` carries a title, a message, and
  an explicit severity.** `VisitorController::fetchError` gets away with two
  arguments because its two titles map 1:1 to severity (`"Network Error"` →
  critical, `"Error"` → warning); `ImportController` reuses the title
  `"Error"` across both a critical case (network failure on
  `check_duplicates.php`, and the fatal excel-open failure before upload)
  and warning cases (invalid/failed duplicate-check response), so severity
  needs its own parameter — see the "Message / Dialog Inventory" table. It
  is used for the duplicate-check failure cases and the two upload
  file-open cases. All other legacy dialogs stay View-owned literal text —
  see "What stays in the View".
- **reply lifetime safety**: exactly as `VisitorController::fetchVisitors`,
  every `connect(reply, &QNetworkReply::finished, ...)` passes the
  controller (`this`) as the 3rd-arg context object, so the connection
  auto-disconnects if the controller is destroyed while a reply — owned by
  the shared `QNetworkAccessManager` — is still in flight. `reply->deleteLater()`
  is called on every path (success, network error, parse error).
- **Purity**: `importcontroller.cpp` contains no `ui->`, no `QMessageBox`, no
  `QFileDialog`, and no `AttachFilesDialog` reference anywhere.

---

## `ImportController` — Responsibilities

### `normalizeHeader(const QString &raw)` — static

Direct port of the free function at adminwindow.cpp:1376–1382 (currently
`static QString normalizeHeader(const QString &h)` at file scope, not a
member): trims, lowercases, then removes all spaces, underscores, and
hyphens — `s.remove(' '); s.remove('_'); s.remove('-');` in that order.

### `mapHeaders(const QStringList &headers, QMap<QString, int> &indexOut)` — static

Unifies the two duplicated if/else chains (adminwindow.cpp:1424–1443 and
1500–1519) into one function. For each header at zero-based column index
`tableCol`, `indexOut` is populated by evaluating `normalizeHeader(header)`
against these aliases, in this exact order (first match wins, `else`
chain — not independent checks):

| Condition on `n = normalizeHeader(header)` | Key written |
|---|---|
| `n.contains("schoolid") \|\| n.contains("studentid") \|\| n == "id"` | `"school_id"` |
| `n.contains("name") \|\| n.contains("fullname") \|\| n.contains("full")` | `"name"` |
| `n.contains("code") \|\| n.contains("studentcode")` | `"code"` |
| `n.contains("course")` | `"course"` |
| `n.contains("year")` | `"year_level"` |
| `n.contains("department") \|\| n.contains("dept")` | `"department"` |
| `n.contains("gender")` | `"gender"` |
| `n.contains("status")` | `"status"` |
| `n.contains("visit")` | `"visits"` |
| *(none of the above)* | `QString("col_%1").arg(tableCol)` |

`indexOut` is cleared by the caller before population (both `parseCsv` and
`parseExcel` clear their local `headerIndex` map before calling
`mapHeaders`, matching the legacy `bulkHeaderIndex.clear()` at the top of
each loader).

### `parseCsv(const QString &rawText)` — static

Pure port of `loadCSVtoTable` (adminwindow.cpp:1465–1542), minus file I/O —
the View reads the file and passes the text in.

- Splits `rawText` into lines the way `QTextStream::readLine()` iteration
  did: split on `"\n"`, matching the line-by-line accumulation in the
  original (`QStringList lines; while (!in.atEnd()) lines.append(in.readLine());`).
  Trailing `\r` from CRLF files is stripped per line (the original relied on
  `QTextStream`'s newline translation; `parseCsv` reproduces the same
  end-user-visible result by trimming a trailing `'\r'` if present before
  further processing).
- If `lines` is empty, returns a `ParsedTable` with empty `headers`/`rows`
  and an empty `headerIndex`. `parseCsv` never shows a dialog (purity
  rule) — it only signals "no data" via the empty result. The View is
  responsible for picking the right one of the two legacy messages before
  it even calls `parseCsv`, exactly as legacy does: it checks
  `rawText.isEmpty()` (or, equivalently, whether the first line split
  yields zero headers) itself and shows `QMessageBox::information(this,
  "CSV Preview", "File is empty.")` (line 1483) or
  `QMessageBox::warning(this, "CSV Error", "CSV file has no headers.")`
  (line 1493) accordingly — the latter branch is already dead in practice
  in legacy (it only fires when a non-empty header line still splits to
  zero headers), and this extraction preserves that same dead branch
  rather than removing it.
- Header line = `lines.first()`, split on `","` with `Qt::SkipEmptyParts`,
  each entry `.trimmed()` (line 1488–1490) — this is the exact split used
  today; it is **not** RFC-4180 CSV (no quoted-comma handling), and this
  extraction does not change that.
- `headerIndex` built via `mapHeaders(headers, headerIndex)`.
- Data rows: for `i` from `1` to `lines.size()-1`, split `lines[i]` on
  `","` **without** `Qt::SkipEmptyParts` (line 1524, matching legacy exactly
  — this is a deliberate asymmetry from the header split). The empty-line
  guard is preserved verbatim: a line is skipped when
  `rowData.isEmpty() || (rowData.size() == 1 && rowData.first().trimmed().isEmpty())`
  (lines 1527–1529). Surviving rows are appended to `ParsedTable::rows` as
  `QStringList` with each cell `.trimmed()` (matching
  `rowData[j].trimmed()` at line 1535) — **ragged rows are preserved
  as-is**: a row with fewer or more cells than `headers.size()` is stored
  with its own natural length; the legacy code's own raggedness handling
  happens only at table-population time (`j < ui->bulkTable->columnCount()`,
  line 1534), which is a View concern documented below, not a `parseCsv`
  concern. `parseCsv` does not pad or truncate rows.
- `ParsedTable::headers` is exactly the parsed header list (the View's
  `qMax(ui->bulkTable->columnCount(), headers.size())` column-count
  reconciliation at line 1539 is View-side table plumbing, not part of the
  pure parse result).

### `parseExcel(const QString &filePath, ExcelParseError *errorOut = nullptr)`

Port of `loadExcelToTable` (adminwindow.cpp:1384–1463), minus all `ui->`
and `QMessageBox` calls — the View is responsible for showing the
failure/empty dialogs listed in the message table below, driven off the
`ExcelParseError` the parse reports:

- Each of the three legacy failure points returns a default-constructed
  empty `ParsedTable` **and** sets `*errorOut` (when non-null) to the
  matching case, so the View reproduces the exact original dialog for each
  (rows 3–5 in the message table) — byte-identical behavior, no narrowing:
  - `!xlsx.isLoadPackage()` → `ExcelParseError::OpenFailed`
    (View: `"Error"` / `"Failed to open Excel file."`, warning).
  - `sheetNames().isEmpty()` → `ExcelParseError::NoSheets`
    (View: `"Error"` / `"This workbook has no sheets."`, warning).
  - `!xlsx.dimension().isValid()` → `ExcelParseError::EmptySheet`
    (View: `"Excel"` / `"Sheet is empty."`, information).
  - A usable table sets `*errorOut = ExcelParseError::None`.
  `errorOut` is optional (`nullptr` default) so unit tests that only care
  about the parsed table need not pass it; when non-null it is always
  written exactly once. `parseExcel` itself shows no dialog (purity rule).
- Sheet selection (`xlsx.selectSheet(xlsx.sheetNames().first())`) and the
  dimension check (`xlsx.dimension().isValid()`) only run when the
  respective prior step succeeded, matching lines 1392–1406. The legacy
  code additionally clears/resizes `ui->bulkTable` to zero rows/columns on
  the empty-sheet path (lines 1402–1404) — View-side table plumbing the
  View still performs whenever it receives an empty `ParsedTable`.
- Otherwise: `firstRow`/`lastRow`/`firstCol`/`lastCol` from `rng`. Header
  row = `firstRow`, read cell-by-cell via `xlsx.read(firstRow, c).toString()`
  for `c` in `[firstCol, lastCol]`; `headerIndex` cleared and built via
  `mapHeaders(headers, headerIndex)` using `tableCol = c - firstCol` as the
  zero-based column index (matching line 1422).
  `rows = lastRow - firstRow` (excludes the header row, matching line
  1446 — note this is **not** `lastRow - firstRow + 1`; this off-by-one
  convention is preserved verbatim). Data rows read from `r = firstRow + 1`
  to `lastRow`, each cell `xlsx.read(r, c).toString()`, appended to
  `ParsedTable::rows`.
- The View still performs `ui->bulkTable->resizeColumnsToContents()`
  (line 1462) after populating from the returned `ParsedTable` — pure table
  plumbing, not part of the controller.

### `checkDuplicates(const QStringList &schoolIds)`

- Builds `{"school_ids": [...]}"` (matching adminwindow.cpp:1169–1174) and
  POSTs to `ApiConfig::endpoint("check_duplicates.php")` with
  `ContentTypeHeader = "application/json"`, via the injected manager.
- On `QNetworkReply::finished`:
  - network error → emits `importError("Error", reply->errorString(),
    ImportSeverity::Critical)` (matching `QMessageBox::critical(this,
    "Error", dupReply->errorString())` at line 1180 — legacy uses
    `"Error"` as the title here, **not** `"Network Error"`; this differs
    from `VisitorController::fetchError`'s `"Network Error"` title and is
    preserved exactly as legacy, not "fixed" to match the sibling
    controller).
  - body is not a JSON object → emits `importError("Error", "Invalid
    duplicate check response.", ImportSeverity::Warning)` (line 1190).
  - `obj["status"] != "success"` → emits `importError("Error", "Duplicate
    check failed.", ImportSeverity::Warning)` (line 1196 — note this is a
    **fixed string**, unlike `VisitorController`'s
    `obj["message"].toString()`; legacy does not read a server-supplied
    message on this path, and this extraction does not add one).
  - otherwise → parses `obj["duplicates"]` into a `QStringList` and emits
    `duplicatesResolved(duplicates)` (empty list when no duplicates found,
    matching the legacy `if (!duplicates.isEmpty())` gate at line 1205
    collapsing to "proceed straight to upload" when empty).
- `reply->deleteLater()` on every path (matching lines 1181, 1186).
- Uses `parseDuplicateResponse` internally to keep the parsing logic
  unit-testable.

### `parseDuplicateResponse(raw, duplicatesOut, errorOut)` — static

Pure function mirroring the body of the `check_duplicates.php` handling
above:
- Not a JSON object → returns `false`, `*errorOut = "Invalid duplicate check response."`.
- `obj["status"] != "success"` → returns `false`, `*errorOut = "Duplicate check failed."`.
- Otherwise → `*duplicatesOut` filled from `obj["duplicates"]` (each array
  entry `.toString()`, matching line 1203), returns `true`.

### `uploadStudents(const QString &excelPath, const QString &zipPath, const QStringList &skipIds)`

Port of the upload half of `onUpdateDatabaseBtnClicked` (adminwindow.cpp:
1250–1366), starting from "Step 4":

- Builds `QNetworkRequest` to `ApiConfig::endpoint("upload_students_zip.php")`
  and a `QHttpMultiPart(QHttpMultiPart::FormDataType)`.
- **Excel part (mandatory, fatal on failure):** `QHttpPart` named `"excel"`
  with `ContentDispositionHeader` `form-data; name="excel";
  filename="<basename>"` (line 1266–1268), body from a `QFile` opened
  `QIODevice::ReadOnly`. **If the excel file fails to open**, this is fatal
  exactly as legacy (lines 1270–1278): the controller does not send the
  request at all. This maps the legacy `QMessageBox::critical(this, "Error",
  "Cannot open Excel file.")` (line 1271) onto `emit importError("Error",
  "Cannot open Excel file.", ImportSeverity::Critical)` — not `uploadFailed`,
  because `uploadFailed` is reserved for the post-send network-error path
  and always carries the fixed title `"Upload Failed"` (row 22), whereas
  this pre-send failure needs the title `"Error"`. The controller also does
  **not** touch the progress bar or button state in this path (that stayed
  in the View before this request began in legacy too — the
  `bulkProgressBar`/button state was only set *after* a successful
  excel-file open, at lines 1251–1258, so the fatal-file-open case never
  toggled it in the first place; this extraction preserves that ordering by
  only emitting `uploadProgress`/state-relevant signals after the excel
  file opens successfully).
- **ZIP part (optional, non-fatal on failure):** only attempted when
  `zipPath` is non-empty. If the file fails to open, **this is non-fatal**:
  legacy shows `QMessageBox::warning(this, "Warning", "Cannot open ZIP
  file. Proceeding without photos.")` (line 1291) and continues the upload
  without the ZIP part. This maps to `emit importError("Warning", "Cannot
  open ZIP file. Proceeding without photos.", ImportSeverity::Warning)` —
  the controller emits the signal but **continues the upload** (does not
  return), exactly preserving the legacy continue-on-failure behavior. This
  is the one case where `importError` is emitted from `uploadStudents`
  rather than `checkDuplicates`; the View's `onImportError` slot shows the
  same warning box regardless of which controller method raised it.
- **skip_ids part:** only appended when `skipIds` is non-empty — `QHttpPart`
  named `"skip_ids"` with body `skipIds.join(",").toUtf8()` (lines
  1300–1305). Note the legacy code builds this from the *duplicates* list
  captured in the `check_duplicates.php` closure; in the extracted design
  the View passes whatever `skipIds` it decided on (which is empty when the
  user had no duplicates, or the "Yes, skip" duplicates list otherwise) —
  see "The async dialog seam".
- Progress: the controller does not emit any synthetic "start" signal
  before sending — it relies purely on `uploadProgress(int percent)` wired
  to the reply's native `uploadProgress(qint64, qint64)` signal exactly as
  legacy (lines 1358–1365):
  `if (bytesTotal > 0) { percent = (bytesSent * 100) / bytesTotal; emit
  uploadProgress(percent); }` — no signal at all is emitted when
  `bytesTotal <= 0`. The View is responsible for the indeterminate
  progress-bar state (`setMinimum(0); setMaximum(0); setValue(0)`, lines
  1256–1258) **before** calling `uploadStudents`, and for switching the bar
  to determinate (`setMaximum(100)`) on the **first** `uploadProgress`
  signal it receives — matching the legacy lambda which calls
  `ui->bulkProgressBar->setMaximum(100)` unconditionally inside the
  `uploadProgress` handler (line 1362), i.e. every emission re-asserts
  determinate mode, which the View replicates by setting `setMaximum(100)`
  on every `uploadProgress` signal it receives (cheap, idempotent, matches
  legacy exactly).
- `QNetworkReply *uploadReply = m_nam->post(uploadRequest, multiPart);
  multiPart->setParent(uploadReply);` (line 1309–1310).
- On `QNetworkReply::finished`:
  - network error → emits `uploadFailed(reply->errorString())` (maps to
    `QMessageBox::critical(this, "Upload Failed", uploadReply->errorString())`,
    line 1318 — title `"Upload Failed"` fixed View-side).
  - otherwise → runs `parseUploadResponse` on the body and emits
    `uploadFinished(result)`. The View branches on `result.plainText` /
    `result.ok` to choose which of the three legacy dialogs to show (see
    message table).
- `reply->deleteLater()` on every path (matching lines 1319, 1324).
- **The `finished` connection uses the controller (`this`) as the 3rd-arg
  context object**, same rationale as `VisitorController::fetchVisitors`.
- The button-enable/progress-bar-hide statements at the top of the legacy
  `finished` lambda (lines 1313–1315:
  `ui->bulkProgressBar->setVisible(false); ui->updateDatabaseBtn->setEnabled(true);
  ui->cancelUploadBtn->setEnabled(false);`) are **View-owned** — the View
  performs them in its `uploadFinished`/`uploadFailed` slots, not the
  controller.

### `parseUploadResponse(const QByteArray &raw)` — static

Pure function mirroring adminwindow.cpp:1327–1354:
- `QJsonDocument::fromJson(raw)` — if `doc.isObject()`:
  - `status = obj["status"].toString()`, `message = obj["message"].toString()`,
    `successCount = obj["success_count"].toInt()`, `errorCount =
    obj["error_count"].toInt()`.
  - `status == "success"` → `UploadResult{ .ok = true, .message = message,
    .successCount = successCount, .errorCount = errorCount, .plainText =
    false }`.
  - otherwise → `UploadResult{ .ok = false, .message = message, .plainText
    = false }` (successCount/errorCount still populated from the response
    even though the View's "Upload Issue" box only shows `message` — see
    the message table; they are harmless extra data on the struct).
- Not a JSON object → `UploadResult{ .plainText = true, .rawText =
  QString::fromUtf8(raw) }` (matches `QString::fromUtf8(response)` at line
  1353).

---

## The async dialog seam (critical)

`checkDuplicates` emits `duplicatesResolved(duplicates)` and returns
control to the View immediately — it does not know or care what happens
next. **The View owns the entire duplicate-dialog experience**, exactly
reproducing adminwindow.cpp:1205–1248:

- When `duplicates` is empty, the View proceeds **straight to**
  `uploadStudents(excelPath, zipPath, QStringList{})` with no dialog at
  all — matching legacy's `if (!duplicates.isEmpty())` gate.
- When `duplicates` is non-empty:
  1. Build `previewList = duplicates.mid(0, 3).join("\n")` and
     `msg = "The following School IDs already exist:\n\n" + previewList`.
  2. If `duplicates.size() > 3`, append
     `"\n...and more.\n\nDo you want to skip them and continue?"`;
     otherwise append `"\n\nDo you want to skip them and continue?"`.
  3. Show a `QMessageBox` with icon `Question`, title `"Duplicates Found"`,
     the built `msg`, buttons `Yes | No`. If `duplicates.size() > 3`, add a
     third button labelled `"Show More"` with `QMessageBox::ActionRole`.
  4. If the clicked button is `"Show More"`:
     - Show `QMessageBox::information(this, "All Duplicates", "Full list of
       duplicate School IDs:\n\n" + duplicates.join("\n"))`.
     - Re-ask via `QMessageBox::question(this, "Proceed?", "Do you want to
       skip duplicates and continue?", Yes | No)`.
     - If the answer is `No` → show `QMessageBox::information(this,
       "Cancelled", "Bulk upload cancelled.")` and stop (do not call
       `uploadStudents`).
     - If the answer is `Yes` → fall through to step 6.
  5. Else if the clicked button's standard-button role is `No` → show
     `QMessageBox::information(this, "Cancelled", "Bulk upload cancelled.")`
     and stop.
  6. Otherwise (clicked `Yes`, or fell through from the "Show More" re-ask
     with `Yes`) → call
     `m_importController->uploadStudents(selectedExcelPath, selectedZipPath, duplicates)`
     — the **entire** `duplicates` list becomes `skipIds`, matching legacy
     line 1300's `if (!duplicates.isEmpty())` guard which always sends the
     full list, not just a user-curated subset (there is no per-ID
     selection UI in legacy — "skip them" means "skip all reported
     duplicates").

This dialog sequence lives entirely in `adminWindow`'s slot for
`duplicatesResolved`. `ImportController` never constructs a `QMessageBox`.

---

## What stays in the View (adminwindow)

- `AttachFilesDialog` and all of its own internal texts (out of scope —
  the dialog class itself is unmodified).
- The `onAttachFileBtnClicked` validation/dispatch texts: `"Missing Data
  File"` / `"You must select a student data file (Excel or CSV)."`;
  `"Invalid File"` / `"Please select a CSV or Excel file."`; the (currently
  unused/commented-out) summary string
  `"Data: %1 | Photos: %2"`.
- Every `QMessageBox` in the upload flow: the `"No Photo ZIP"` question,
  `"No data to upload."` warning, the entire duplicate dialog sequence
  above, the `"Upload Complete"` / `"Upload Issue"` / `"Upload Failed"`
  boxes, and the parse-loader warning/info boxes (`"Failed to open Excel
  file."`, `"This workbook has no sheets."`, `"Sheet is empty."`, `"File is
  empty."`, `"CSV file has no headers."`, `"Cannot open file: ..."`).
- `bulkTable` population from `ParsedTable` (`headers`,
  `resizeColumnsToContents`, the Excel row-count math) — the View calls
  `ui->bulkTable->clear()`, sets row/column counts from
  `parsedTable.rows.size()` / `parsedTable.headers.size()`, sets
  `setHorizontalHeaderLabels(parsedTable.headers)`, and fills cells from
  `parsedTable.rows`.
- **School-ID collection from the live `bulkTable`** at upload time — the
  View keeps its own `getCell` lambda reading `ui->bulkTable->item(row,
  col)->text().trimmed()` using the cached `headerIndex` (from the last
  `ParsedTable`) with fallback-to-column-1 (`getCell(i, "school_id", 1)`),
  exactly as adminwindow.cpp:1147–1162. **This deliberately reads current
  widget state, not a cached snapshot** — if a user hand-edits a cell in
  `bulkTable` after the preview load (the table is editable, unlike
  `visitorTable`), the edit must be picked up at upload time. Caching a
  `QList<QStringList>` snapshot the way `VisitorController` caches
  `m_visitorRecords` would silently drop user edits and is explicitly
  **not** done here — this is the one place this extraction's caching
  strategy diverges from the Visitor extraction, and it is intentional:
  the two tabs have different editability, so the same
  cache-what-was-last-fetched pattern is wrong for Import.
- Progress bar and button enable/disable state machine (`bulkProgressBar`
  visibility/min/max/value, `updateDatabaseBtn`/`cancelUploadBtn` enabled
  state).
- `selectedExcelPath`, `selectedZipPath`, `cancelUpload`, and
  `bulkHeaderIndex` member state (unchanged names/types in `adminwindow.h`).
- `onCancelUploadBtnClicked()` stays entirely in the View — it never talked
  to the network (`cancelUpload = true; QMessageBox::information(...);
  ui->bulkTable->clearContents();`), so there is nothing to extract. (Note:
  the `cancelUpload` flag is set but not read anywhere in the upload flow in
  the current code — this is a pre-existing dead flag, out of scope to fix.)

---

## Message / Dialog Inventory

Every user-visible dialog in the legacy import flow, its exact text, and
which side owns it after extraction.

| # | Trigger | Title | Body | Severity | Owner after extraction |
|---|---|---|---|---|---|
| 1 | Attach: no excel path chosen | `Missing Data File` | `You must select a student data file (Excel or CSV).` | warning | View |
| 2 | Attach: unrecognized extension | `Invalid File` | `Please select a CSV or Excel file.` | warning | View |
| 3 | Excel loader: package fails to load | `Error` | `Failed to open Excel file.` | warning | View (on `ExcelParseError::OpenFailed`) |
| 4 | Excel loader: no sheets | `Error` | `This workbook has no sheets.` | warning | View (on `ExcelParseError::NoSheets`) |
| 5 | Excel loader: empty dimension | `Excel` | `Sheet is empty.` | information | View (on `ExcelParseError::EmptySheet`) |
| 6 | CSV loader: file won't open | `Error` | `Cannot open file: <path>` | warning | View (View performs the `QFile::open` check itself before calling `parseCsv`) |
| 7 | CSV loader: no lines | `CSV Preview` | `File is empty.` | information | View |
| 8 | CSV loader: header split empty | `CSV Error` | `CSV file has no headers.` | warning | View (dead-in-practice branch, preserved) |
| 9 | Update DB: no excel attached | `Missing Data File` | `Please attach the student Excel/CSV file first.` | warning | View |
| 10 | Update DB: no ZIP attached | `No Photo ZIP` | `No ZIP file was selected. Continue uploading without photos?` | question (Yes/No) | View |
| 11 | Update DB: empty table | `Error` | `No data to upload.` | warning | View |
| 12 | check_duplicates: network error | `Error` | `<reply->errorString()>` | critical | Controller → `importError("Error", errorString, critical)` |
| 13 | check_duplicates: bad response | `Error` | `Invalid duplicate check response.` | warning | Controller → `importError("Error", "Invalid duplicate check response.", warning)` |
| 14 | check_duplicates: status != success | `Error` | `Duplicate check failed.` | warning | Controller → `importError("Error", "Duplicate check failed.", warning)` |
| 15 | Duplicates found (≤3) | `Duplicates Found` | `The following School IDs already exist:\n\n<up to 3, one per line>\n\nDo you want to skip them and continue?` | question (Yes/No) | View |
| 16 | Duplicates found (>3) | `Duplicates Found` | `The following School IDs already exist:\n\n<first 3>\n...and more.\n\nDo you want to skip them and continue?` | question (Yes/No/Show More) | View |
| 17 | Show More clicked | `All Duplicates` | `Full list of duplicate School IDs:\n\n<all, one per line>` | information | View |
| 18 | Re-ask after Show More | `Proceed?` | `Do you want to skip duplicates and continue?` | question (Yes/No) | View |
| 19 | Duplicates: user picks No | `Cancelled` | `Bulk upload cancelled.` | information | View |
| 20 | Upload: excel file won't open | `Error` | `Cannot open Excel file.` | critical | Controller → `importError("Error", "Cannot open Excel file.", critical)` (fatal, upload never sent) |
| 21 | Upload: zip file won't open | `Warning` | `Cannot open ZIP file. Proceeding without photos.` | warning | Controller → `importError("Warning", "Cannot open ZIP file. Proceeding without photos.", warning)` (non-fatal, upload continues) |
| 22 | Upload: network error | `Upload Failed` | `<reply->errorString()>` | critical | Controller → `uploadFailed(errorString)`; View shows it critical with title `"Upload Failed"` |
| 23 | Upload: success | `Upload Complete` | `Bulk upload finished!\n\nSuccessfully inserted: <n>\nErrors/Skipped: <n>\n\n<message>` | information | View (from `uploadFinished(result)` where `result.ok && !result.plainText`) |
| 24 | Upload: status != success | `Upload Issue` | `<message>` | warning | View (from `uploadFinished(result)` where `!result.ok && !result.plainText`) |
| 25 | Upload: plain-text fallback | `Upload Complete` | `<raw response text>` | information | View (from `uploadFinished(result)` where `result.plainText`) |
| 26 | Cancel button clicked | `Cancelled` | `Bulk upload has been cancelled.` | information | View (unchanged, never touched network) |

Rows 12–14 and 20–21 travel through `importError`; row 22 travels through
`uploadFailed`. Legacy uses **critical** severity for rows 12, 20, and 22,
and **warning** for rows 13, 14, and 21 — the severity does not correlate
with which signal carries it, so `importError` needs a severity, not a
fixed icon:

```cpp
enum class ImportSeverity { Warning, Critical };
...
signals:
    void importError(const QString &title, const QString &message, ImportSeverity severity);
```

This is the one place this spec's `importError` shape differs from
`VisitorController::fetchError`'s two-argument signal — `VisitorController`
could infer severity from the title (`"Network Error"` → critical, `"Error"`
→ warning) because it only ever used two titles; `ImportController` reuses
the title `"Error"` for both a critical case (row 12, row 20) and warning
cases (rows 13, 14), so title-sniffing would be ambiguous. Adding the
explicit `severity` parameter avoids that ambiguity outright rather than
working around it.

The View's `onImportError(title, message, severity)` slot shows
`QMessageBox::critical(this, title, message)` when `severity ==
ImportSeverity::Critical`, else `QMessageBox::warning(this, title,
message)` — reproducing all five cases (rows 12–14, 20–21) exactly,
including row 20's critical excel-open failure and row 22's
`uploadFailed`-carried critical network error, which needs no severity
parameter because `uploadFailed` is always critical (`QMessageBox::critical(this,
"Upload Failed", message)`, matching legacy line 1318) — row 20 does not
reuse `uploadFailed` precisely because its title (`"Error"`) differs from
row 22's (`"Upload Failed"`), and `uploadFailed` has no title parameter by
design (the only legacy `uploadFailed`-style failure with a non-"Upload
Failed" title is the excel-open case, which is why it is modeled as
`importError` instead).

---

## Data Flow

### On Attach File clicked
```
onAttachFileBtnClicked()
  → AttachFilesDialog dialog(this); dialog.exec()
  → guard: selectedExcelPath.isEmpty() → "Missing Data File" warning, return
  → dispatch by extension:
        .csv  → View reads file text, guards QFile::open failure itself
                (row 6), else calls ImportController::parseCsv(text)
        .xlsx → ExcelParseError err;
                parsedTable = m_importController->parseExcel(selectedExcelPath, &err);
                if (err != None) → show the matching row 3/4/5 dialog, return
        other → "Invalid File" warning, return
  → View populates ui->bulkTable from the returned ParsedTable
  → View caches bulkHeaderIndex = parsedTable.headerIndex
```

### On Update Database clicked
```
onUpdateDatabaseBtnClicked()
  → guards: selectedExcelPath empty (row 9) / selectedZipPath empty (row 10, Yes/No)
            / bulkTable rowCount == 0 (row 11)
  → collect schoolIds from LIVE bulkTable via getCell(i, "school_id", 1)
  → m_importController->checkDuplicates(schoolIds)
        → POST check_duplicates.php {"school_ids":[...]}
        → parseDuplicateResponse(body)
        → emits duplicatesResolved(duplicates)  or  importError(title, msg)
  → [duplicatesResolved empty] View calls uploadStudents(excelPath, zipPath, {})
  → [duplicatesResolved non-empty] View runs the full duplicate-dialog seam
        → on confirm: uploadStudents(excelPath, zipPath, duplicates)
        → on cancel: "Cancelled" info box, stop
  → View sets progress bar indeterminate + button states before calling uploadStudents
  → m_importController->uploadStudents(excelPath, zipPath, skipIds)
        → builds multipart (excel mandatory/fatal, zip optional/non-fatal,
          skip_ids when non-empty)
        → [zip open fails] importError("Warning", "Cannot open ZIP file. ...", Warning)
          then continues
        → [excel open fails] importError("Error", "Cannot open Excel file.", Critical) — fatal, stop
        → POST upload_students_zip.php
        → emits uploadProgress(percent) repeatedly
        → emits uploadFinished(result)  or  uploadFailed(errorString)
  → View: progress bar hidden, buttons re-enabled
  → View branches on result.plainText / result.ok → rows 23/24/25 dialogs
  → [row 23 only] View clears bulkTable, clears selectedExcelPath/selectedZipPath
```

---

## Tests — `tst_importcontroller`

New Qt Test target in `qt-app/tests/`. No live network — all response
parsers use synthetic `QByteArray` payloads with synthetic school IDs and
names only (e.g. `"2023-00123"`, `"Juan Dela Cruz"`), per the project
workflow and security-hygiene rules. No real student PII, no personal
machine paths.

Coverage:

- **`normalizeHeader`**: `" School ID "` → `"schoolid"`; `"Full_Name"` →
  `"fullname"`; `"Year-Level"` → `"yearlevel"`; already-normalized input is
  idempotent.
- **`mapHeaders`**: one test per alias family (`"School ID"`, `"Student
  ID"`, `"id"` → `school_id`; `"Name"`, `"Full Name"` → `name`; `"Code"`,
  `"Student Code"` → `code`; `"Course"` → `course`; `"Year Level"` →
  `year_level`; `"Department"`, `"Dept"` → `department`; `"Gender"` →
  `gender`; `"Status"` → `status`; `"Visits"` → `visits`); an unrecognized
  header (`"Notes"`) falls back to `"col_<index>"`; a full realistic header
  row asserts all indices at once.
- **`parseCsv`**: header row with all fields + a couple of data rows
  (asserts `headers`, `rows`, `headerIndex`); an empty-line row is skipped
  (the `rowData.isEmpty()` guard); a ragged row (fewer cells than headers)
  is stored as-is with its natural (short) length, not padded; a raw text
  with zero lines returns an empty `ParsedTable`; a `col_N` fallback header
  is asserted end to end.
- **`parseDuplicateResponse`**: success with a `duplicates` array (asserts
  the list); success with an empty `duplicates` array (asserts `true` +
  empty list); `status` missing/not `"success"` → `false` +
  `"Duplicate check failed."`; a non-object body (e.g. a bare JSON array or
  malformed JSON) → `false` + `"Invalid duplicate check response."`.
- **`parseUploadResponse`**: JSON `status: "success"` with
  `success_count`/`error_count`/`message` → `ok == true` and all fields
  populated; `status` not `"success"` → `ok == false`, `message` populated;
  a plain-text (non-JSON-object) body → `plainText == true`, `rawText`
  equals the input decoded as UTF-8.
- **`parseExcel` round-trip**: write a small workbook to a `QTemporaryDir`
  path using `QXlsx::Document` directly in the test (headers row +
  2–3 synthetic data rows), then call `ImportController::parseExcel` on
  that path and assert `headers`, `rows`, and `headerIndex` match what was
  written, and that the `ExcelParseError *` out-param is set to
  `ExcelParseError::None`. A second case writes a workbook with only a header
  row (no data rows) and asserts `rows.isEmpty()` with `headers` still
  populated. A third case calls `parseExcel` on a non-existent / non-package
  path and asserts an empty `ParsedTable` with `*errorOut ==
  ExcelParseError::OpenFailed`.

---

## Files to Modify in CMake

### `qt-app/CMakeLists.txt`

Add to the `qt_add_executable(WITS ...)` source list, next to the visitor
files:

```cmake
importdata.h
importcontroller.h importcontroller.cpp
```

No new Qt modules — `Qt::Network` and `QXlsx` are already linked to `WITS`.

### `qt-app/tests/CMakeLists.txt`

Register a seventh target following the `tst_visitorcontroller` pattern
exactly:

```cmake
qt_add_executable(tst_importcontroller
    tst_importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/importcontroller.h
    ${CMAKE_SOURCE_DIR}/importdata.h
)
set_target_properties(tst_importcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_importcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_importcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    QXlsx
)
add_test(NAME tst_importcontroller COMMAND tst_importcontroller)
set_tests_properties(tst_importcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
```

`QT_QPA_PLATFORM=offscreen` is required for the same reason as
`tst_visitorcontroller`: QXlsx links `Qt::Gui` and propagates it, so
`QTEST_MAIN` instantiates a `QGuiApplication`, which needs a platform
plugin on a headless runner. The `parseExcel` round-trip test needs the
`QGuiApplication` that the Gui link provides.

---

## Task Decomposition

Mirrors the three-task shape used for the Visitor extraction:

- **Task 1** — `importdata.h` + `importcontroller.h`/`.cpp` skeleton: value
  types, all pure statics (`normalizeHeader`, `mapHeaders`, `parseCsv`,
  `parseDuplicateResponse`, `parseUploadResponse`) implemented for real;
  `checkDuplicates`/`uploadStudents`/`parseExcel` stubbed (declared,
  trivial/empty bodies); CMake wiring for both the `WITS` target and
  `tst_importcontroller`; `tst_importcontroller` red → green on the
  statics only.
- **Task 2** — implement `checkDuplicates` and `uploadStudents` (the
  network methods) plus `parseExcel`; add the `parseExcel` round-trip test
  and any remaining response-parser edge cases.
- **Task 3** — wire `ImportController` into `adminWindow`: add
  `m_importController` member, construct it in the constructor after
  `networkManager`, add `onImportDuplicatesResolved` / `onImportError` /
  `onUploadProgress` / `onUploadFinished` / `onUploadFailed` slots, replace
  the bodies of `onAttachFileBtnClicked`, `onUpdateDatabaseBtnClicked`,
  `loadExcelToTable`, `loadCSVtoTable` with the View-side flow described
  above, delete the free-function `normalizeHeader` and the duplicated
  header-mapping chains from `adminwindow.cpp`, keep
  `onCancelUploadBtnClicked` untouched. Finish with a purity grep gate
  (`importcontroller.cpp` has no `ui->`/`QMessageBox`/`QFileDialog`/
  `AttachFilesDialog`) and a no-dead-code grep gate (no leftover duplicate
  header-mapping chain in `adminwindow.cpp`).

**Branch:** `feat/import-controller-extraction` (already created off
`master`).

---

## Out of Scope (This Step)

- Student and Reports tab extractions (they follow this template next).
- Any change to `check_duplicates.php` or `upload_students_zip.php` or
  their wire protocols — payload shapes are unchanged.
- UI layout changes to the Database Import tab.
- Making CSV parsing RFC-4180 compliant (quoted commas) — the naive
  `split(",")` behavior is preserved deliberately.
- Fixing the dead `cancelUpload` flag (set, never read) — preserved as-is.
- Backend URL configurability (separate spec; `ApiConfig` is used as-is).

---

## Verification Criteria

The extraction is complete when:

1. The app builds with no new warnings or errors, and
   `ctest --test-dir qt-app/build --output-on-failure` is green — all 6
   existing targets plus `tst_importcontroller`.
2. The Database Import tab behaves identically to before:
   - Attach File with no data file selected shows "Missing Data File";
     an unrecognized extension shows "Invalid File".
   - A valid `.csv` or `.xlsx` file previews into `bulkTable` with the same
     headers, row count, and cell contents as before, including ragged CSV
     rows and the `col_N` fallback for unrecognized headers.
   - Each Excel-load failure still shows its exact original dialog — rows 3
     (`"Failed to open Excel file."`), 4 (`"This workbook has no sheets."`),
     and 5 (`"Sheet is empty."`) — driven by the `ExcelParseError` out-param,
     with no message collapsed or narrowed.
   - Update Database validates attachments and table contents identically
     (rows 9–11), and the duplicate-dialog seam (rows 15–19) reproduces
     every text, button, and cancellation path verbatim, including the
     "Show More" full-list dialog and its re-ask.
   - Uploading with no ZIP prompts row 10; a ZIP that fails to open shows
     the row 21 warning and proceeds without photos; an Excel file that
     fails to open aborts with row 20's critical box and re-enables the
     buttons without ever showing the progress bar.
   - The progress bar goes indeterminate then tracks percentage exactly as
     before; button enable/disable state matches at every stage.
   - Success, issue, and plain-text-fallback upload outcomes (rows 23–25)
     show the identical dialog text, and only the success case clears the
     table and the two selected-path members.
   - Cancel Upload behaves unchanged (row 26).
3. `adminWindow` contains no `QNetworkAccessManager` POSTs and no JSON
   parsing for the import tab (the network halves of
   `onUpdateDatabaseBtnClicked` are gone).
4. `adminWindow` contains no QXlsx code and no CSV-line-splitting logic for
   the import tab (`loadExcelToTable`/`loadCSVtoTable` bodies are View-only
   table plumbing calling into `ImportController`).
5. There is exactly one copy of the header-alias mapping in the codebase
   (`ImportController::mapHeaders`) — the duplicated if/else chains are
   gone from `adminwindow.cpp`.
6. `ImportController` contains no `ui->` access, no `QFileDialog`, no
   `AttachFilesDialog`, and no `QMessageBox` calls.

---

## Global Constraints

- Qt 6.11, C++17, CMake+Ninja+MinGW. `#ifndef` header guards (NOT
  `#pragma once`). `m_` prefix for new members. Functor-based `connect`
  (no `SIGNAL`/`SLOT` macros). `QNetworkAccessManager` injected, not owned.
  `reply->deleteLater()` on every path; `finished` connected with the
  controller as the context object. QXlsx (vendored, do NOT modify) links
  `Qt::GuiPrivate` → the test target sets `QT_QPA_PLATFORM=offscreen`.
- Security-hygiene (binding): no secrets/admin keys/credentials/backend
  URLs in source, tests, CMake, or this spec; use placeholders. No real
  student PII in fixtures — synthetic only. No hardcoded
  `C:\Users\<name>` personal paths anywhere in committed files. NEVER
  stage/commit `qt-app/adminwindow.ui` (carries the user's own uncommitted
  edit) or `qt-app/build/`.
