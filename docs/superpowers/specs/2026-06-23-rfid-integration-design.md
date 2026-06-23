# WITS RFID Integration — Design

**Date:** 2026-06-23
**Status:** Approved (design); pending implementation plan
**Component:** `qt-app/` (Qt 6 / C++17 desktop client)

## 1. Goal & Scope

Make HID keyboard-emulator RFID scans log a student library visit on the kiosk/login
screen of the WITS app.

The RFID scanner is a **HID/keyboard wedge**: when a card is tapped it *types* the card's
character code into whatever widget currently has focus, followed by an Enter. It is **not**
a serial device — no `QSerialPort`, no COM-port polling.

### In scope
- Capture HID scans on the login/kiosk screen.
- Route a scanned card code to the existing `rfid_login.php` endpoint (lookup by `students.code`).
- On success, display the student and log the visit (reusing the existing manual-login UI path).
- Stand up the repo's first Qt Test target and unit-test the scan-detection logic.

### Out of scope (YAGNI)
- **Barcode scanning** — explicitly a future phase.
- **HTTPS / configurable backend URL** — the app hardcodes `http://localhost` everywhere; that
  is a pre-existing, app-wide concern, not introduced or fixed here.
- **Admin enable/disable toggle** — chosen behavior is *always-on* (no `rfid/enabled` QSettings
  checkbox, despite what `deliverables/docs/RFID_UI_SETUP.md` describes).
- **Scanner prefix/suffix configuration** — the timing-based approach needs no device config.
- **Card registration** — already implemented: `register_student.php` accepts a `code`, the admin
  window has `codeLineEdit` and a "Code" column, edit-student handles `code`, and bulk Excel import
  maps a `code` column.

## 2. Background / Current State

- `qt-app/rfidreader.{h,cpp}` is written for a `QSerialPort` reader. It is **dead code**: nothing
  in `mainwindow`/`adminwindow`/`guestwindow` references `RFIDReader`, and `CMakeLists.txt` does not
  link `Qt::SerialPort` (so the include is an unmet dependency). It will be removed.
- Login flow ([`mainwindow.cpp:234`](../../../qt-app/mainwindow.cpp) `MainWindow::handleLogin`):
  reads `ui->username`, and routes **numeric → `student_login.php` (`school_id`)**,
  **non-numeric → `admin_login.php` (`admin_key`)**, triggered on `returnPressed` or the login button.
- `rfid_login.php` already exists and looks up `students WHERE code = ?`, logs a visit into
  `library_visits`, increments `visits`, and returns the same student JSON shape as `student_login.php`.

### The routing-collision problem
Card codes are commonly hex (e.g. `0123456789AB`), i.e. **non-numeric**. If a scan's keystrokes
reached the `username` field and its `returnPressed`, `handleLogin` would route the code to
`admin_login.php` (the non-numeric branch). Therefore scans must be detected and handled *before*
they reach `handleLogin` — they must never flow through the manual `username` path.

## 3. Approach

Chosen: **timing-based keyboard-wedge detection** (over a configured prefix/suffix sentinel, or a
dedicated always-focused field). Rationale: zero scanner configuration, best kiosk UX ("tap
anywhere"), and it structurally avoids the routing collision because scans are intercepted before
the `username` field. The detection logic is isolated as a pure state machine so it is unit-testable
without widgets.

## 4. Components

### 4.1 `RfidScanDetector` — pure logic (new `rfidscandetector.{h,cpp}`)
No Qt widgets, no network, no timers. A small state machine fed `(character, timestampMs)` pairs.

```cpp
// Returns the completed code when a fast, Enter-terminated burst is recognized;
// otherwise std::nullopt.
std::optional<QString> feedKey(QChar character, qint64 timestampMs);
void reset();
```

Algorithm:
- On a printable character: if the gap since the previous character exceeds `maxInterKeyGapMs`,
  reset the buffer first (human pace breaks the burst); then append and record the timestamp.
- On Enter/Return: if the buffer was built as one fast burst (all inter-key gaps ≤ threshold) **and**
  its length ≥ `minCodeLength`, return the code and reset; otherwise reset and return `nullopt`
  (manual entry — let the normal path handle it).
- Overflow guard: if the buffer exceeds `maxBufferLength`, reset.

Config constants (initial values, tunable):
- `maxInterKeyGapMs ≈ 50`
- `minCodeLength ≈ 3`
- `maxBufferLength ≈ 64`

### 4.2 `RfidKeyboardFilter` — Qt glue (new `rfidkeyboardfilter.{h,cpp}`)
A `QObject` event filter owning an `RfidScanDetector`. Installed on `qApp`, but it **acts only when
the login window is the active window** (`QApplication::activeWindow()` is the `MainWindow`). This
guarantees scans are never intercepted inside the admin window, protecting the `codeLineEdit`
registration flow.

Responsibilities (precise consume contract, to avoid re-injection complexity):
- On `QKeyEvent` key presses, feed `(text, currentMSecsSinceEpoch)` to the detector. **Printable keys
  are not consumed** — they propagate normally to the focused widget (so manual typing is unaffected
  with no re-injection needed). During a scan they momentarily appear in `username`; this is cleared
  on completion (see below) — a sub-100ms flash, acceptable for a kiosk.
- The decision is made at the **Enter/Return** key: if the detector returns a completed code, the
  filter **consumes only that Enter** (returns `true`) so the `QLineEdit`'s `returnPressed` —and thus
  `handleLogin`— never fires, then emits `rfidScanned(QString code)`. If the detector returns
  `nullopt`, the Enter is **not** consumed and the normal manual path runs.
- The filter stays decoupled from `MainWindow`'s widgets: it does not touch `username` directly.
  `MainWindow::handleRfidLogin` clears `username` before POSTing.

Signal:
```cpp
signals:
    void rfidScanned(const QString &code);
```

### 4.3 `MainWindow` changes (`mainwindow.{h,cpp}`)
- Construct and install an `RfidKeyboardFilter`; connect `rfidScanned` → `handleRfidLogin`.
- New slot `void handleRfidLogin(const QString &code)`: POST `rfid_id=code` to
  `http://localhost/rfid_login.php`.
- **Refactor to avoid duplication:** extract the existing reply-handling (parse JSON, download/show
  photo with fade, prepend to `recentLogins`, `refreshRightPanel`) into a shared
  `void displayStudent(const QJsonObject &student)` used by **both** `handleLogin` and
  `handleRfidLogin`. (Without this, the dry-checker flags duplicated response handling.)

## 5. Data Flow

1. Student taps card on the login screen.
2. Scanner types `<code><Enter>` as a fast keystroke burst.
3. The `qApp` event filter (active because the login window is the active window) feeds keys +
   timestamps to `RfidScanDetector`.
4. Printable keys propagate to `username` (a brief flash); at the terminating Enter the detector
   recognizes the burst and returns the code. The filter consumes **only that Enter**
   (`returnPressed`/`handleLogin` never fires) and emits `rfidScanned(code)`.
5. `MainWindow::handleRfidLogin(code)` clears `username`, then POSTs `rfid_id=code` to `rfid_login.php`.
6. Backend looks up `students WHERE code=?`, logs the visit, increments `visits`, returns student JSON.
7. `displayStudent()` shows the photo and updates the recent-logins panel — identical to manual login.

## 6. Error Handling / UX

- **Unregistered card** (`"RFID not registered"`): show an on-screen message; the screen stays ready
  for the next tap; no crash.
- **Network error**: reuse the existing `QMessageBox::critical` pattern from `handleLogin`.
- **Double-tap debounce**: ignore an identical code re-scanned within ~2–3 seconds, so one physical
  tap logs exactly one visit.
- **Garbage / overflow**: the detector caps buffer length and resets on overflow.

## 7. Files Changed

- **Delete:** `qt-app/rfidreader.h`, `qt-app/rfidreader.cpp`; remove from `CMakeLists.txt`
  `PROJECT_SOURCES`. (Also removes the unmet `Qt::SerialPort` reference.)
- **Add:** `qt-app/rfidscandetector.{h,cpp}`, `qt-app/rfidkeyboardfilter.{h,cpp}`.
- **Modify:** `qt-app/mainwindow.{h,cpp}` (install filter, `handleRfidLogin`, extract
  `displayStudent`, add the `rfid_login.php` URL), `qt-app/CMakeLists.txt`.
- **Add test target:** `qt-app/tests/` with a `wits_tests` executable linking `Qt::Test`, registered
  via `add_test(...)` so it runs under `ctest`.

## 8. Testing

This is the **first test target** in the repo; standing it up is the red step of this work
(per the workflow rule).

Unit tests for `RfidScanDetector` (synthetic timestamps, no real timing, synthetic codes only):
- Fast burst + Enter → returns the code.
- Human-paced keystrokes + Enter → returns `nullopt` (manual entry).
- Burst without a terminator → nothing until Enter arrives.
- Slow-then-fast sequence → buffer resets correctly, only the fast tail is considered.
- Below `minCodeLength` → rejected.
- Overflow beyond `maxBufferLength` → reset, no spurious emission.

Manual QA (GUI — clean build is necessary but not sufficient):
- A registered card logs a visit and shows the student.
- An unregistered card shows the message and recovers.
- Manual School-ID typing still works and is **not** misrouted to `admin_login.php`.
- A scan focused in the admin window's `codeLineEdit` is **not** intercepted.

## 9. Security Notes (per `security-hygiene.md`)

- Transport stays plaintext `http://localhost` — pre-existing and app-wide; flagged as a known
  limitation, not changed in this scope.
- No secrets added. `rfid_login.php` already uses prepared statements.
- A card `code` is student-identifying data: keep it out of persistent logs (minimal `qDebug`), and
  use **synthetic** codes in all tests/fixtures.

## 10. Open / Tunable Items

- `maxInterKeyGapMs` (≈50ms) is the scan/human boundary — validate against the real reader during QA.
- Debounce window (≈2–3s) — adjust to the deployment's tap cadence.
