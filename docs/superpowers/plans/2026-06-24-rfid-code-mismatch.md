# RFID Card-Code Mismatch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a registered student's RFID tap log them in, by first capturing the exact bytes the reader emits versus what is stored in `students.code`, then applying the narrowest evidence-selected fix.

**Architecture:** Diagnostic-first. Phase 1 (this plan) replaces broken kiosk instrumentation with a *pure, unit-tested* code-inspection helper and wires it into the scan path so the running app prints the scanned value + length + hex. The operator captures that against the stored `code`, and a decision gate routes to exactly one Phase 2 fix (authored as a follow-up plan once the byte-level difference is known — the normalization rule cannot be written before it is measured).

**Tech Stack:** Qt 6.11 Widgets (C++17), QtTest + ctest, CMake + Ninja (MinGW kit). PHP/mysqli backend (read-only for this plan).

## Global Constraints

- Build toolchain — tools are NOT on PATH; prepend in **every** PowerShell build command:
  `$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH`
  Configure: `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`
  Build: `cmake --build qt-app/build` · Test: `ctest --test-dir qt-app/build --output-on-failure`
- **PII / security (binding):** the runtime `qDebug` that prints a card code and the server response is **temporary diagnostic only** and MUST be removed before this branch merges. No card codes or student PII in persistent logs or committed code. Keep using the existing `http://localhost/loams_api/` transport as-is (accepted pattern, out of scope).
- C++17; Qt parent-ownership for QObjects; UI logic pushed out of `QMainWindow` into pure testable units; header-only pure helpers follow the `qt-app/apiconfig.h` precedent.
- Commit via the `commit` skill only (never raw `git add`/`git commit`). Never commit `qt-app/build/`, `*.user`, or `moc_*`/`ui_*`.
- Branch: `feat/responsive-ui`. Spec: `docs/superpowers/specs/2026-06-24-rfid-code-mismatch-design.md`.

---

## File Structure

| File | Responsibility | Change |
| --- | --- | --- |
| `qt-app/rfidcodeinspect.h` | Pure, header-only helper that renders a scanned code as a human-diff-able diagnostic string (`value`, `len`, space-separated `hex`). No Qt Widgets, no network — links into the test target and the app. | **Create** |
| `qt-app/tests/tst_rfidcodeinspect.cpp` | QtTest unit tests for the helper — leading zeros preserved, length, hex of hidden chars. | **Create** |
| `qt-app/tests/CMakeLists.txt` | Register the `tst_rfidcodeinspect` ctest target (mirror the existing header-only `tst_apiconfig` block at lines 29–38). | **Modify** |
| `qt-app/mainwindow.cpp` | `handleRfidLogin` — remove the broken empty-`postData` log (`:372`); print the real scanned code via the helper; keep a clearly-flagged response log so the operator can read the literal server body. | **Modify (`:359-405`)** |

Phase 2 touches `register_student.php`, `rfid_login.php`, and/or a normalization helper — **not specified here** (gated on Phase 1 evidence; see "Phase 2 — Decision Gate").

---

## Why Task 1 is TDD but Task 2 is not

The *diagnostic format* is real logic with a contract worth locking down, and the project convention is to push such logic out of the window into a pure, testable unit — so Task 1 is a genuine red→green cycle. Task 2 is the throwaway wiring (a `qDebug` call) whose only "test" is the evidence it prints at runtime; it is verified by Task 3's manual capture, not by an automated test. This split is deliberate, not an omission.

---

### Task 1: Pure RFID code-inspection helper (`rfidcodeinspect.h`)

**Files:**
- Create: `qt-app/rfidcodeinspect.h`
- Create: `qt-app/tests/tst_rfidcodeinspect.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (add target after the `tst_apiconfig` block, line 38)

**Interfaces:**
- Consumes: nothing (pure `QString` in, `QString` out).
- Produces: `QString RfidCodeInspect::describe(const QString &code)` returning exactly
  `value="<code>" len=<n> hex=<b0 b1 ...>` where `hex` is the space-separated lowercase
  Latin-1 byte dump of `code`. Task 2 calls this.

- [ ] **Step 1: Write the failing test**

Create `qt-app/tests/tst_rfidcodeinspect.cpp`:

```cpp
#include <QtTest>
#include "rfidcodeinspect.h"

class TstRfidCodeInspect : public QObject
{
    Q_OBJECT
private slots:
    void preservesLeadingZeros();
    void reportsLengthAndHex();
    void exposesHiddenWhitespace();
};

void TstRfidCodeInspect::preservesLeadingZeros()
{
    // The whole point: a 10-digit code with leading zeros must survive verbatim.
    QCOMPARE(RfidCodeInspect::describe("0003241370"),
             QStringLiteral("value=\"0003241370\" len=10 hex=30 30 30 33 32 34 31 33 37 30"));
}

void TstRfidCodeInspect::reportsLengthAndHex()
{
    QCOMPARE(RfidCodeInspect::describe("12"),
             QStringLiteral("value=\"12\" len=2 hex=31 32"));
}

void TstRfidCodeInspect::exposesHiddenWhitespace()
{
    // A stray leading space or trailing CR is the kind of thing the hex dump must reveal.
    QCOMPARE(RfidCodeInspect::describe(" 12\r"),
             QStringLiteral("value=\" 12\r\" len=4 hex=20 31 32 0d"));
}

QTEST_APPLESS_MAIN(TstRfidCodeInspect)
#include "tst_rfidcodeinspect.moc"
```

- [ ] **Step 2: Register the test target (so it can build)**

In `qt-app/tests/CMakeLists.txt`, after the `tst_apiconfig` block (immediately after line 38, before the `tst_theme` block), add:

```cmake
qt_add_executable(tst_rfidcodeinspect
    tst_rfidcodeinspect.cpp
)
set_target_properties(tst_rfidcodeinspect PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_rfidcodeinspect PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_rfidcodeinspect PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_rfidcodeinspect COMMAND tst_rfidcodeinspect)
set_tests_properties(tst_rfidcodeinspect PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)
```

- [ ] **Step 3: Run the test to verify it fails**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build
```
Expected: **build FAILS** at compile/link of `tst_rfidcodeinspect` — `rfidcodeinspect.h: No such file or directory` (the header does not exist yet). This is the red state.

- [ ] **Step 4: Write the minimal implementation**

Create `qt-app/rfidcodeinspect.h`:

```cpp
#ifndef RFIDCODEINSPECT_H
#define RFIDCODEINSPECT_H

#include <QString>

// Pure, header-only diagnostic for an RFID scan value. Renders the raw string,
// its length, and a space-separated Latin-1 hex dump so leading zeros and
// otherwise-invisible characters (stray spaces, a trailing CR, encoding quirks)
// are directly comparable against the stored students.code. Header-only and
// dependency-free (no Widgets/network) so it links into both the app and the
// unit-test target. See apiconfig.h for the same header-only pattern.
namespace RfidCodeInspect {

inline QString describe(const QString &code)
{
    const QString hex = QString::fromLatin1(code.toLatin1().toHex(' '));
    return QStringLiteral("value=\"%1\" len=%2 hex=%3")
        .arg(code)
        .arg(code.length())
        .arg(hex);
}

} // namespace RfidCodeInspect

#endif // RFIDCODEINSPECT_H
```

- [ ] **Step 5: Run the test to verify it passes**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_rfidcodeinspect --output-on-failure
```
Expected: `100% tests passed, 0 tests failed out of 1` — `tst_rfidcodeinspect` PASS (3 functions).

- [ ] **Step 6: Confirm the rest of the suite still builds/passes**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```
Expected: all existing targets still pass; one new test (`tst_rfidcodeinspect`) added.

- [ ] **Step 7: Commit (via the `commit` skill)**

Stage `qt-app/rfidcodeinspect.h`, `qt-app/tests/tst_rfidcodeinspect.cpp`, `qt-app/tests/CMakeLists.txt`.
Suggested message: `feat(rfid): add pure RfidCodeInspect::describe diagnostic helper`

---

### Task 2: Wire the diagnostic into the scan path; remove the broken log

**Files:**
- Modify: `qt-app/mainwindow.cpp:359-405` (`MainWindow::handleRfidLogin`)

**Interfaces:**
- Consumes: `RfidCodeInspect::describe(const QString&)` from Task 1.
- Produces: at runtime, two diagnostic lines per scan — the inspected scanned code, and the literal server response body. **Both are temporary and removed before merge (Task 4).**

> Not TDD — this is throwaway runtime instrumentation. Its verification is Task 3's manual capture, and its build is verified below.

- [ ] **Step 1: Add the include**

Near the top of `qt-app/mainwindow.cpp`, with the other project includes (it already includes `apiconfig.h`), add:

```cpp
#include "rfidcodeinspect.h"
```

- [ ] **Step 2: Replace the broken log and keep an explicit, flagged response log**

In `handleRfidLogin`, the current lines 369–373 are:

```cpp
    QUrl url = ApiConfig::endpoint("rfid_login.php");
    qDebug() << "RFID URL:" << url.toString();
    QUrlQuery postData;
    qDebug() << "POST DATA:" << postData.toString();   // <-- BUG: prints "" (rfid_id not added yet)
    postData.addQueryItem("rfid_id", code);
```

Replace them with (note `.noquote()` so the hex/quotes render cleanly, and the log moves *after* the value exists):

```cpp
    QUrl url = ApiConfig::endpoint("rfid_login.php");
    QUrlQuery postData;
    postData.addQueryItem("rfid_id", code);
    // TEMP DIAGNOSTIC — remove before merge (PII: prints a card code).
    qDebug().noquote() << "RFID SCAN:" << RfidCodeInspect::describe(code);
```

Then update the existing response log (current lines 388–390) so the operator can read the *literal* server body — the spec's key discriminator between a value mismatch (`"RFID not registered"`) and an insert failure (`"Internal server error"`, returned with HTTP 200). Replace:

```cpp
        QByteArray response = reply->readAll();
        qDebug() << "RFID RESPONSE:";
        qDebug() << response;
```
with:
```cpp
        QByteArray response = reply->readAll();
        // TEMP DIAGNOSTIC — remove before merge (may contain student PII on success).
        qDebug().noquote() << "RFID RESPONSE:" << QString::fromUtf8(response);
```

Leave the rest of `handleRfidLogin` (debounce, request, JSON branch) unchanged. (The `qDebug() << "RFID URL:"` line at old `:370` is removed by the replacement above — the URL is static and not needed.)

- [ ] **Step 3: Build and verify the app links**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build qt-app/build
```
Expected: clean build, `WITS.exe` links, no new warnings (the known harmless QXlsx GuiPrivate warning may still appear).

- [ ] **Step 4: Commit (via the `commit` skill)**

Stage `qt-app/mainwindow.cpp`.
Suggested message: `fix(rfid): log the real scanned code + literal response (temp diagnostic)`

---

### Task 3: Capture evidence and resolve the decision gate (manual — operator)

**Files:** none (runtime + DB inspection).

This is the deliverable that *unblocks Phase 2*. No code changes; it produces a written finding.

- [ ] **Step 1: Run the app and scan the known card**

Launch `qt-app/build/WITS.exe` from a console (so `qDebug` is visible), tap the sample card, and record the `RFID SCAN:` line, e.g. `RFID SCAN: value="0003241370" len=10 hex=30 30 30 33 32 34 31 33 37 30`.

- [ ] **Step 2: Read the literal server response**

Record the `RFID RESPONSE:` line. **Branch:**
  - Body contains `"RFID not registered"` → genuine zero-row value mismatch → continue to Step 3.
  - Body contains `"Internal server error"` → NOT a value mismatch; the `library_visits` INSERT is throwing on a registered card. **Stop this gate** — the fix is server-side (the insert/transaction in `rfid_login.php`), and Phase 2 should be re-scoped around that. Capture the PHP error log (`deliverables/loams_api/logs/`) and hand off.

- [ ] **Step 3: Read back the stored value (read-only SQL, run by the operator)**

```sql
SELECT id, school_id, name, code, LENGTH(code) AS code_len
FROM students
WHERE code IS NOT NULL AND code <> ''
ORDER BY id;
```
Also confirm the column type once: `SHOW COLUMNS FROM students LIKE 'code';` (expect a `varchar`; an integer type would have coerced and matched, so the bug implies textual).

- [ ] **Step 4: Compare and record the finding**

Write one explicit line stating the byte-level relationship between the scanned `value`/`len`/`hex` and the stored `code`/`code_len`, and pick the gate outcome below. Record it in the Phase-2 follow-up plan's header (and optionally append to the spec's open-questions).

---

## Phase 2 — Decision Gate (authored as a follow-up plan)

Phase 2 is intentionally **not** broken into bite-sized tasks here: the normalization rule is unknowable until Task 3 measures the difference, and writing speculative code would violate the spec's evidence gate. Task 3's finding selects exactly one branch, each already designed in the spec (`docs/superpowers/specs/2026-06-24-rfid-code-mismatch-design.md`, §"Phase 2 — Fix"):

| Task 3 finding | Phase 2 option (from spec) | Follow-up plan scope |
| --- | --- | --- |
| Scanned `value` ≠ stored `code` by a **format difference** (missing leading zeros, stray space/CR, case) | **Option A — canonical normalization** at the 3 `code` seams: `handleRfidLogin`, `register_student.php` (sole writer), `rfid_login.php` lookup. Rule chosen from the measured diff; normalize losslessly (e.g. zero-pad to the reader's fixed width), unit-test in `wits_tests`. | TDD a pure `normalizeRfidCode` helper + apply at seams; re-register or migrate existing rows. |
| Stored `code` is **already canonical** but historical rows are wrong | **Option B — data backfill** (reviewed one-off SQL, backed up) + close recurrence on the registration path (scan-into-field). | Migration script + registration-path hardening. |
| Stored `code` is **empty/NULL or a different card** for that student | **Option C — operational**: re-register by scanning the card into the admin Code field; add admin-form validation that the typed code matches a fresh scan. | Admin-form UX/validation change. |
| Response was `"Internal server error"` (Step 2 branch) | **Out of this spec's scope** — server-side insert/transaction bug in `rfid_login.php`. | New debugging spec scoped to the `library_visits` INSERT. |

**Author the Phase 2 plan** by re-running `superpowers:writing-plans` against the spec once the finding is recorded.

---

### Task 4: Remove the temporary diagnostic before merge (gate)

**Files:** Modify `qt-app/mainwindow.cpp` (`handleRfidLogin`).

This task MUST be completed before the branch is finished / PR'd (binding PII rule). It runs after Phase 2's fix is verified, so the diagnostic has served its purpose.

- [ ] **Step 1: Remove both `TEMP DIAGNOSTIC` lines** added in Task 2 (the `RFID SCAN:` and `RFID RESPONSE:` `qDebug` calls), and the `#include "rfidcodeinspect.h"` if `describe` is no longer referenced from `mainwindow.cpp`.

- [ ] **Step 2: Decide on the helper.** If Phase 2 Option A reused `RfidCodeInspect` (or a normalizer beside it), keep `rfidcodeinspect.h` + its test. If nothing else uses it (YAGNI), remove `qt-app/rfidcodeinspect.h`, `qt-app/tests/tst_rfidcodeinspect.cpp`, and its `tst_rfidcodeinspect` block in `qt-app/tests/CMakeLists.txt`.

- [ ] **Step 3: Build + full ctest green**, then grep to prove no card-code logging remains:

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
```bash
grep -rn "RFID SCAN\|RFID RESPONSE\|TEMP DIAGNOSTIC" qt-app/   # expect: no matches
```
Expected: clean build, all tests pass, grep returns nothing.

- [ ] **Step 4: Commit (via the `commit` skill)**

Suggested message: `chore(rfid): remove temporary scan diagnostics (PII) before merge`

---

## Definition of Done (Phase 1)

- `RfidCodeInspect::describe` exists, is unit-tested (3 cases), and the full ctest suite is green.
- The broken empty-`postData` log is gone; the running app prints the real scanned code (len + hex) and the literal server response.
- Task 3's byte-level finding is recorded and has selected exactly one Phase 2 branch.
- The temporary card-code `qDebug` lines are tracked for removal (Task 4) and will not reach `master`.
