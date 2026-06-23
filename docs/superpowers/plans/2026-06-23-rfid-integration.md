# RFID Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire an HID keyboard-emulator RFID scanner into the WITS kiosk login screen so a card tap logs a student library visit.

**Architecture:** A pure, unit-testable `RfidScanDetector` state machine recognizes a fast, Enter-terminated keystroke burst (vs. human typing). A thin `RfidKeyboardFilter` (a `QObject` event filter installed on `qApp`, active only when the login window is the active window) feeds key events to the detector and emits `rfidScanned(code)`. `MainWindow` routes that to `handleRfidLogin`, which POSTs to `rfid_login.php` and shows the student via a `displayStudent()` helper shared with the existing manual login.

**Tech Stack:** C++17, Qt 6 (Widgets, Network, Test), CMake, QtTest/ctest. PHP backend already exists (`deliverables/loams_api/rfid_login.php`).

**Spec:** [docs/superpowers/specs/2026-06-23-rfid-integration-design.md](../specs/2026-06-23-rfid-integration-design.md)

## Global Constraints

- **Language/standard:** C++17; Qt 6 (`qt_add_executable`, `QT_VERSION_MAJOR` ≥ 6 path).
- **No `QSerialPort`** anywhere — the scanner is a HID keyboard wedge, not a serial device.
- **Always-on:** no admin enable/disable toggle, no `rfid/enabled` QSettings.
- **Endpoint:** `http://localhost/rfid_login.php`, POST field `rfid_id`. Plaintext `http://localhost` is the pre-existing app-wide pattern — keep it; do **not** add HTTPS/configurable-URL work here.
- **Detector is pure:** no Qt widgets, no network, no Qt keycodes — only `QString`/`QChar`/`std::optional`. The terminator reaches it as the character `'\n'`/`'\r'`, never as a `Qt::Key_*`.
- **Kiosk feedback must be non-modal** — never `QMessageBox` on the RFID path (a modal steals `QApplication::activeWindow()` and deafens the scan filter until dismissed).
- **Security-hygiene:** no secrets/credentials/PII in code or tests; use **synthetic** card codes in all tests; keep card codes out of persistent logs.
- **Commits:** use the project `commit` skill for every commit (the bash `git commit` shown in steps is the literal effect; route it through the skill). Never commit `qt-app/build/`, `*.user`, or `moc_*`/`ui_*` output.

---

## File Structure

| File | Responsibility | Task |
|------|----------------|------|
| `qt-app/rfidscandetector.h` / `.cpp` | Pure burst-detection state machine | 1 |
| `qt-app/tests/CMakeLists.txt` | Builds the QtTest executables, registers them with ctest | 1, 2 |
| `qt-app/tests/tst_rfidscandetector.cpp` | Unit tests for the detector | 1 |
| `qt-app/rfidkeyboardfilter.h` / `.cpp` | Qt event-filter glue: key events → detector → `rfidScanned` | 2 |
| `qt-app/tests/tst_rfidkeyboardfilter.cpp` | Unit test for terminator-key classification | 2 |
| `qt-app/mainwindow.h` / `.cpp` | Install filter; `handleRfidLogin`; shared `displayStudent`; non-modal status | 3 |
| `qt-app/CMakeLists.txt` | Drop dead `rfidreader`; add new sources; `enable_testing` + `add_subdirectory(tests)` | 1, 2 |
| `qt-app/rfidreader.h` / `.cpp` | **Deleted** (dead serial code) | 1 |

---

## Task 1: Pure `RfidScanDetector` + QtTest harness + remove dead serial reader

**Files:**
- Delete: `qt-app/rfidreader.h`, `qt-app/rfidreader.cpp`
- Modify: `qt-app/CMakeLists.txt`
- Create: `qt-app/rfidscandetector.h`, `qt-app/rfidscandetector.cpp`
- Create: `qt-app/tests/CMakeLists.txt`, `qt-app/tests/tst_rfidscandetector.cpp`

**Interfaces:**
- Produces:
  - `class RfidScanDetector` with `explicit RfidScanDetector(qint64 maxInterKeyGapMs = 60, int minCodeLength = 3, int maxBufferLength = 64);`
  - `std::optional<QString> RfidScanDetector::feedKey(QChar character, qint64 timestampMs);` — feed one printable char, or `'\n'`/`'\r'` as the terminator. Returns the code iff a fast (all inter-key gaps ≤ `maxInterKeyGapMs`), terminator-ended burst of length ≥ `minCodeLength` was seen; else `std::nullopt`.
  - `void RfidScanDetector::reset();`

- [ ] **Step 1: Update CMake — drop the dead reader, add the detector source, add Qt Test, enable tests**

In `qt-app/CMakeLists.txt`, change the `find_package` lines (currently lines 12–13) to add `Test`:

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Charts Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test)
```

Replace the `set(PROJECT_SOURCES ...)` block (currently lines 15–25) with (removes `rfidreader.*`, adds the detector — the filter is added in Task 2):

```cmake
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    adminwindow.cpp
    adminwindow.h
    adminwindow.ui
    rfidscandetector.cpp
    rfidscandetector.h
)
```

At the **end** of `qt-app/CMakeLists.txt`, after the `qt_finalize_executable(WITS)` block, append:

```cmake
enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 2: Delete the dead serial reader files**

```bash
git rm qt-app/rfidreader.h qt-app/rfidreader.cpp
```

- [ ] **Step 3: Create the detector header `qt-app/rfidscandetector.h`**

```cpp
#ifndef RFIDSCANDETECTOR_H
#define RFIDSCANDETECTOR_H

#include <optional>
#include <QString>
#include <QChar>

// Pure, widget-free state machine that recognizes a fast, terminator-ended
// keystroke burst (an HID RFID scan) and distinguishes it from human typing.
// Fed (character, timestampMs) pairs by RfidKeyboardFilter; the terminator
// arrives as the character '\n' or '\r' (never a Qt keycode).
class RfidScanDetector
{
public:
    explicit RfidScanDetector(qint64 maxInterKeyGapMs = 60,
                              int minCodeLength = 3,
                              int maxBufferLength = 64);

    // Returns the completed code on a recognized scan; std::nullopt otherwise.
    std::optional<QString> feedKey(QChar character, qint64 timestampMs);
    void reset();

private:
    QString m_buffer;
    qint64 m_lastTimestampMs;
    const qint64 m_maxInterKeyGapMs;
    const int m_minCodeLength;
    const int m_maxBufferLength;
};

#endif // RFIDSCANDETECTOR_H
```

- [ ] **Step 4: Create a STUB `qt-app/rfidscandetector.cpp` (so the test compiles and fails)**

```cpp
#include "rfidscandetector.h"

RfidScanDetector::RfidScanDetector(qint64 maxInterKeyGapMs, int minCodeLength, int maxBufferLength)
    : m_lastTimestampMs(-1)
    , m_maxInterKeyGapMs(maxInterKeyGapMs)
    , m_minCodeLength(minCodeLength)
    , m_maxBufferLength(maxBufferLength)
{
}

std::optional<QString> RfidScanDetector::feedKey(QChar, qint64)
{
    return std::nullopt; // stub — implemented in Step 7
}

void RfidScanDetector::reset()
{
    m_buffer.clear();
    m_lastTimestampMs = -1;
}
```

- [ ] **Step 5: Create the test CMake `qt-app/tests/CMakeLists.txt`**

```cmake
qt_add_executable(tst_rfidscandetector
    tst_rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.h
)
target_include_directories(tst_rfidscandetector PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_rfidscandetector PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_rfidscandetector COMMAND tst_rfidscandetector)
```

- [ ] **Step 6: Write the first failing test `qt-app/tests/tst_rfidscandetector.cpp`**

```cpp
#include <QtTest>
#include "rfidscandetector.h"

class TestRfidScanDetector : public QObject
{
    Q_OBJECT
private slots:
    void fastBurstReturnsCode();
};

void TestRfidScanDetector::fastBurstReturnsCode()
{
    RfidScanDetector d(60, 3, 64);
    QVERIFY(!d.feedKey('A', 0).has_value());
    QVERIFY(!d.feedKey('B', 5).has_value());
    QVERIFY(!d.feedKey('C', 10).has_value());
    QVERIFY(!d.feedKey('1', 15).has_value());
    auto result = d.feedKey('\n', 20); // terminator
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABC1"));
}

QTEST_APPLESS_MAIN(TestRfidScanDetector)
#include "tst_rfidscandetector.moc"
```

- [ ] **Step 7: Configure, build, and run — expect RED**

```bash
cmake -S qt-app -B qt-app/build
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: `tst_rfidscandetector` FAILS at `QVERIFY(result.has_value())` (the stub returns `nullopt`). This also proves the harness builds and ctest discovers the test, and that WITS still builds without `rfidreader`.

- [ ] **Step 8: Implement the detector (core: buffering + dual terminator + min length)**

Replace `feedKey` in `qt-app/rfidscandetector.cpp`:

```cpp
std::optional<QString> RfidScanDetector::feedKey(QChar character, qint64 timestampMs)
{
    if (character == QChar('\n') || character == QChar('\r')) { // terminator
        std::optional<QString> result;
        if (m_buffer.length() >= m_minCodeLength)
            result = m_buffer;
        reset();
        return result;
    }

    m_buffer.append(character);
    m_lastTimestampMs = timestampMs;
    return std::nullopt;
}
```

- [ ] **Step 9: Build and run — expect GREEN**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: PASS.

- [ ] **Step 10: Commit**

```bash
git add qt-app/CMakeLists.txt qt-app/tests/CMakeLists.txt qt-app/tests/tst_rfidscandetector.cpp qt-app/rfidscandetector.h qt-app/rfidscandetector.cpp
git rm --cached qt-app/rfidreader.h qt-app/rfidreader.cpp 2>/dev/null; true
git commit -m "feat(rfid): add RfidScanDetector core + QtTest harness, drop dead serial reader"
```

- [ ] **Step 11: Add the timing-discrimination tests (human-paced + slow-then-fast)**

Add to the `private slots:` list and the body of `tst_rfidscandetector.cpp`:

```cpp
    void humanPacedReturnsNullopt();
    void slowThenFastReturnsFastTail();
```

```cpp
void TestRfidScanDetector::humanPacedReturnsNullopt()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('1', 0);
    d.feedKey('2', 200);   // 200ms gap > 60 -> human pace
    d.feedKey('3', 400);
    d.feedKey('4', 600);
    auto result = d.feedKey('\n', 800);
    QVERIFY(!result.has_value());
}

void TestRfidScanDetector::slowThenFastReturnsFastTail()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('X', 0);
    d.feedKey('Y', 300);   // slow
    d.feedKey('A', 600);   // slow gap -> burst restarts here
    d.feedKey('B', 605);   // fast
    d.feedKey('C', 610);
    d.feedKey('D', 615);
    auto result = d.feedKey('\n', 620);
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABCD"));
}
```

- [ ] **Step 12: Build and run — expect RED**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: both new tests FAIL — the current impl ignores timing, so it returns `"1234"` / `"XYABCD"`.

- [ ] **Step 13: Add the gap-reset timing logic**

In `feedKey`, insert the gap check immediately before `m_buffer.append(character);`:

```cpp
    if (m_lastTimestampMs >= 0 && (timestampMs - m_lastTimestampMs) > m_maxInterKeyGapMs)
        reset(); // a human-pace gap breaks the burst; start a fresh candidate
```

- [ ] **Step 14: Build and run — expect GREEN**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: all PASS.

- [ ] **Step 15: Add overflow + regression edge tests**

Add to `private slots:` and the body:

```cpp
    void overflowResetsBuffer();
    void burstWithoutTerminatorEmitsNothing();
    void belowMinLengthRejected();
    void carriageReturnIsTerminator();
```

```cpp
void TestRfidScanDetector::overflowResetsBuffer()
{
    RfidScanDetector d(60, 3, 8); // small cap for the test
    qint64 t = 0;
    for (int i = 0; i < 9; ++i) // 9 fast chars > cap of 8
        d.feedKey('A', t++);
    auto result = d.feedKey('\n', t);
    QVERIFY(!result.has_value()); // overflow emptied the buffer
}

void TestRfidScanDetector::burstWithoutTerminatorEmitsNothing()
{
    RfidScanDetector d(60, 3, 64);
    QVERIFY(!d.feedKey('A', 0).has_value());
    QVERIFY(!d.feedKey('B', 5).has_value());
    QVERIFY(!d.feedKey('C', 10).has_value());
}

void TestRfidScanDetector::belowMinLengthRejected()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('A', 0);
    d.feedKey('B', 5);
    auto result = d.feedKey('\n', 10); // only 2 chars, min is 3
    QVERIFY(!result.has_value());
}

void TestRfidScanDetector::carriageReturnIsTerminator()
{
    RfidScanDetector d(60, 3, 64);
    d.feedKey('A', 0);
    d.feedKey('B', 5);
    d.feedKey('C', 10);
    auto result = d.feedKey('\r', 15); // CR, not LF
    QVERIFY(result.has_value());
    QCOMPARE(*result, QString("ABC"));
}
```

- [ ] **Step 16: Build and run — expect RED on overflow only**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: `overflowResetsBuffer` FAILS (no cap yet); the other three PASS (they exercise already-working branches — these lock in the behavior).

- [ ] **Step 17: Add the overflow guard**

In `feedKey`, immediately after `m_lastTimestampMs = timestampMs;` and before `return std::nullopt;`:

```cpp
    if (m_buffer.length() > m_maxBufferLength)
        reset();
```

- [ ] **Step 18: Build and run — expect GREEN (all detector tests)**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: all PASS.

- [ ] **Step 19: Commit**

```bash
git add qt-app/rfidscandetector.cpp qt-app/tests/tst_rfidscandetector.cpp
git commit -m "feat(rfid): add timing discrimination, overflow guard, edge-case coverage to detector"
```

---

## Task 2: `RfidKeyboardFilter` (Qt event-filter glue)

**Files:**
- Create: `qt-app/rfidkeyboardfilter.h`, `qt-app/rfidkeyboardfilter.cpp`
- Modify: `qt-app/CMakeLists.txt` (add the filter to `PROJECT_SOURCES`)
- Create: `qt-app/tests/tst_rfidkeyboardfilter.cpp`
- Modify: `qt-app/tests/CMakeLists.txt` (add the filter test executable)

**Interfaces:**
- Consumes: `RfidScanDetector` (Task 1).
- Produces:
  - `class RfidKeyboardFilter : public QObject` with `explicit RfidKeyboardFilter(QWidget *loginWindow, QObject *parent = nullptr);`
  - `static bool RfidKeyboardFilter::isTerminator(int key);` — true for `Qt::Key_Return` and `Qt::Key_Enter`.
  - signal `void rfidScanned(const QString &code);`
  - Install via `qApp->installEventFilter(filter);`. Acts only while `QApplication::activeWindow() == loginWindow`.

- [ ] **Step 1: Create `qt-app/rfidkeyboardfilter.h`**

```cpp
#ifndef RFIDKEYBOARDFILTER_H
#define RFIDKEYBOARDFILTER_H

#include <QObject>
#include <QElapsedTimer>
#include "rfidscandetector.h"

class QWidget;

// Application-wide key event filter. While the login window is the active
// window, feeds printable keys (and a normalized terminator) to an
// RfidScanDetector; on a recognized scan it consumes the terminating Enter
// (so QLineEdit::returnPressed / handleLogin never fires) and emits the code.
class RfidKeyboardFilter : public QObject
{
    Q_OBJECT
public:
    explicit RfidKeyboardFilter(QWidget *loginWindow, QObject *parent = nullptr);

    static bool isTerminator(int key);

signals:
    void rfidScanned(const QString &code);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_loginWindow;
    RfidScanDetector m_detector;
    QElapsedTimer m_clock;
};

#endif // RFIDKEYBOARDFILTER_H
```

- [ ] **Step 2: Create a STUB `qt-app/rfidkeyboardfilter.cpp` (so the test compiles and fails)**

```cpp
#include "rfidkeyboardfilter.h"

#include <QApplication>
#include <QKeyEvent>
#include <QWidget>

RfidKeyboardFilter::RfidKeyboardFilter(QWidget *loginWindow, QObject *parent)
    : QObject(parent)
    , m_loginWindow(loginWindow)
{
    m_clock.start();
}

bool RfidKeyboardFilter::isTerminator(int)
{
    return false; // stub — implemented in Step 6
}

bool RfidKeyboardFilter::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event); // stub
}
```

- [ ] **Step 3: Register the filter test in `qt-app/tests/CMakeLists.txt`**

Append:

```cmake
qt_add_executable(tst_rfidkeyboardfilter
    tst_rfidkeyboardfilter.cpp
    ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.cpp
    ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.h
    ${CMAKE_SOURCE_DIR}/rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.h
)
target_include_directories(tst_rfidkeyboardfilter PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_rfidkeyboardfilter PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME tst_rfidkeyboardfilter COMMAND tst_rfidkeyboardfilter)
```

- [ ] **Step 4: Write the failing classification test `qt-app/tests/tst_rfidkeyboardfilter.cpp`**

```cpp
#include <QtTest>
#include "rfidkeyboardfilter.h"

class TestRfidKeyboardFilter : public QObject
{
    Q_OBJECT
private slots:
    void returnKeyIsTerminator();
    void enterKeyIsTerminator();
    void letterKeyIsNotTerminator();
};

void TestRfidKeyboardFilter::returnKeyIsTerminator()
{
    QVERIFY(RfidKeyboardFilter::isTerminator(Qt::Key_Return));
}

void TestRfidKeyboardFilter::enterKeyIsTerminator()
{
    QVERIFY(RfidKeyboardFilter::isTerminator(Qt::Key_Enter)); // keypad Enter
}

void TestRfidKeyboardFilter::letterKeyIsNotTerminator()
{
    QVERIFY(!RfidKeyboardFilter::isTerminator(Qt::Key_A));
}

QTEST_GUILESS_MAIN(TestRfidKeyboardFilter)
#include "tst_rfidkeyboardfilter.moc"
```

- [ ] **Step 5: Configure, build, run — expect RED**

```bash
cmake -S qt-app -B qt-app/build
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: `returnKeyIsTerminator` and `enterKeyIsTerminator` FAIL (stub returns false).

- [ ] **Step 6: Implement `isTerminator` and the real `eventFilter`**

In `qt-app/rfidkeyboardfilter.cpp`, replace `isTerminator` and `eventFilter`:

```cpp
bool RfidKeyboardFilter::isTerminator(int key)
{
    return key == Qt::Key_Return || key == Qt::Key_Enter;
}

bool RfidKeyboardFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    // Only intercept on the kiosk/login window — never the admin window etc.
    if (QApplication::activeWindow() != m_loginWindow)
        return QObject::eventFilter(watched, event);

    auto *ke = static_cast<QKeyEvent *>(event);
    const qint64 ts = m_clock.elapsed();

    if (isTerminator(ke->key())) {
        std::optional<QString> code = m_detector.feedKey(QChar('\n'), ts);
        if (code) {
            emit rfidScanned(*code);
            return true; // consume Enter so returnPressed/handleLogin never fires
        }
        return QObject::eventFilter(watched, event);
    }

    const QString text = ke->text();
    if (text.isEmpty()) // modifiers (Shift for uppercase hex), function keys
        return QObject::eventFilter(watched, event);

    m_detector.feedKey(text.at(0), ts);
    return QObject::eventFilter(watched, event); // do NOT consume printable keys
}
```

- [ ] **Step 7: Build and run — expect GREEN**

```bash
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```
Expected: all tests (detector + filter) PASS.

- [ ] **Step 8: Add the filter to the WITS build**

In `qt-app/CMakeLists.txt`, add the filter to `set(PROJECT_SOURCES ...)` after the detector lines:

```cmake
    rfidkeyboardfilter.cpp
    rfidkeyboardfilter.h
```

- [ ] **Step 9: Build WITS to confirm it links — then commit**

```bash
cmake -S qt-app -B qt-app/build
cmake --build qt-app/build
git add qt-app/rfidkeyboardfilter.h qt-app/rfidkeyboardfilter.cpp qt-app/CMakeLists.txt qt-app/tests/CMakeLists.txt qt-app/tests/tst_rfidkeyboardfilter.cpp
git commit -m "feat(rfid): add RfidKeyboardFilter event-filter glue with terminator tests"
```

---

## Task 3: Wire RFID into `MainWindow`

**Files:**
- Modify: `qt-app/mainwindow.h`
- Modify: `qt-app/mainwindow.cpp`

**Interfaces:**
- Consumes: `RfidKeyboardFilter::rfidScanned` (Task 2); `MainWindow::refreshRightPanel()`, `recentLogins`, `networkManager`, `ui->username`, `ui->studentPhoto` (existing).
- Produces (private to `MainWindow`): `void displayStudent(const QJsonObject &student);`, `slot void handleRfidLogin(const QString &code);`, `void showKioskStatus(const QString &message, bool error);`.

- [ ] **Step 1: Add declarations to `qt-app/mainwindow.h`**

Add near the top includes (after `#include <QMainWindow>`):

```cpp
#include <QElapsedTimer>
#include <QJsonObject>
class RfidKeyboardFilter;
class QLabel;
```

In the `private slots:` section add:

```cpp
    void handleRfidLogin(const QString &code);
```

In the `private:` section add (alongside the existing members):

```cpp
    void displayStudent(const QJsonObject &student);
    void showKioskStatus(const QString &message, bool error);

    RfidKeyboardFilter *rfidFilter = nullptr;
    QLabel *m_statusLabel = nullptr;
    QElapsedTimer m_rfidDebounceClock;
    QString m_lastRfidCode;
    qint64 m_lastRfidMs = -100000;
```

- [ ] **Step 2: Add includes + filter wiring to the `MainWindow` constructor**

In `qt-app/mainwindow.cpp`, add after the existing includes (after line 26):

```cpp
#include <QApplication>
#include <QElapsedTimer>
#include "rfidkeyboardfilter.h"
```

In the constructor, immediately after the existing line `connect(ui->username, &QLineEdit::returnPressed, this, &MainWindow::handleLogin);` (currently line 78), add:

```cpp
    rfidFilter = new RfidKeyboardFilter(this, this);
    qApp->installEventFilter(rfidFilter);
    connect(rfidFilter, &RfidKeyboardFilter::rfidScanned, this, &MainWindow::handleRfidLogin);
    m_rfidDebounceClock.start();
```

- [ ] **Step 3: Extract `displayStudent` from the existing login reply lambda**

In `qt-app/mainwindow.cpp`, in `handleLogin`'s reply lambda, replace the success body (currently lines 277–321: from `if (obj.contains("student")) {` through its matching `else { adminWin->show(); }`) with:

```cpp
                if (obj.contains("student")) {
                    displayStudent(obj["student"].toObject());
                } else {
                    adminWin->show();
                }
```

Then add the new method (place it right after `handleLogin`, before `toggleFullscreen`):

```cpp
void MainWindow::displayStudent(const QJsonObject &student) {
    QString photoUrl = student["photo_url"].toString();

    if (!photoUrl.isEmpty()) {
        QNetworkAccessManager *photoManager = new QNetworkAccessManager(this);
        QNetworkRequest request{QUrl(photoUrl)};

        connect(photoManager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *photoReply) {
            if (photoReply->error() == QNetworkReply::NoError) {
                QByteArray imgData = photoReply->readAll();
                QPixmap pix;
                if (pix.loadFromData(imgData)) {
                    ui->studentPhoto->setPixmap(pix.scaled(
                        ui->studentPhoto->width(),
                        ui->studentPhoto->height(),
                        Qt::KeepAspectRatio,
                        Qt::SmoothTransformation
                        ));

                    QGraphicsOpacityEffect *fadeEffect = new QGraphicsOpacityEffect();
                    ui->studentPhoto->setGraphicsEffect(fadeEffect);

                    QPropertyAnimation *fadeAnim = new QPropertyAnimation(fadeEffect, "opacity");
                    fadeAnim->setDuration(700);
                    fadeAnim->setStartValue(0.0);
                    fadeAnim->setEndValue(1.0);
                    fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
                }
            }
            photoReply->deleteLater();
        });
        photoManager->get(request);
    }

    recentLogins.prepend(student);
    while (recentLogins.size() > 9)
        recentLogins.removeLast();
    refreshRightPanel();
}
```

- [ ] **Step 4: Build to confirm the refactor is behavior-neutral**

```bash
cmake --build qt-app/build
```
Expected: builds clean. (Manual login behavior unchanged — verified in Task 4.)

- [ ] **Step 5: Commit the refactor on its own**

```bash
git add qt-app/mainwindow.h qt-app/mainwindow.cpp
git commit -m "refactor(login): extract displayStudent() from handleLogin reply handler"
```

- [ ] **Step 6: Add `showKioskStatus` (non-modal, auto-dismissing)**

Add after `displayStudent` in `qt-app/mainwindow.cpp`:

```cpp
void MainWindow::showKioskStatus(const QString &message, bool error) {
    if (!m_statusLabel) {
        m_statusLabel = new QLabel(this);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->hide();
    }
    m_statusLabel->setStyleSheet(error
        ? "background:#C0392B; color:white; padding:14px; border-radius:8px; font-size:18px;"
        : "background:#27AE60; color:white; padding:14px; border-radius:8px; font-size:18px;");
    m_statusLabel->setText(message);
    m_statusLabel->adjustSize();
    m_statusLabel->move((width() - m_statusLabel->width()) / 2,
                        height() - m_statusLabel->height() - 60);
    m_statusLabel->raise();
    m_statusLabel->show();
    QTimer::singleShot(2500, m_statusLabel, [this]() {
        if (m_statusLabel) m_statusLabel->hide();
    });
}
```

- [ ] **Step 7: Add the `handleRfidLogin` slot**

Add after `showKioskStatus` in `qt-app/mainwindow.cpp`:

```cpp
void MainWindow::handleRfidLogin(const QString &code) {
    // Debounce: ignore the same card re-scanned within 2.5s (one tap = one visit).
    const qint64 now = m_rfidDebounceClock.elapsed();
    if (code == m_lastRfidCode && (now - m_lastRfidMs) < 2500)
        return;
    m_lastRfidCode = code;
    m_lastRfidMs = now;

    ui->username->clear(); // remove the brief flash of the scanned code

    QUrl url("http://localhost/rfid_login.php");
    QUrlQuery postData;
    postData.addQueryItem("rfid_id", code);

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            showKioskStatus("Network error. Please try again.", true);
            reply->deleteLater();
            return;
        }

        QByteArray response = reply->readAll();
        reply->deleteLater();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        if (!jsonDoc.isObject()) {
            showKioskStatus("Invalid server response.", true);
            return;
        }

        QJsonObject obj = jsonDoc.object();
        if (obj["status"].toString() == "success" && obj.contains("student")) {
            displayStudent(obj["student"].toObject());
        } else {
            showKioskStatus("Card not registered. Please see the librarian.", true);
        }
    });
}
```

- [ ] **Step 8: Build — expect clean compile/link**

```bash
cmake --build qt-app/build
```
Expected: WITS builds with no new warnings/errors.

- [ ] **Step 9: Commit**

```bash
git add qt-app/mainwindow.h qt-app/mainwindow.cpp
git commit -m "feat(rfid): route scans to rfid_login.php via handleRfidLogin with debounce + non-modal status"
```

---

## Task 4: Build + ctest + manual QA verification gate

**Files:** none (verification only).

> REQUIRED SUB-SKILL for this task: `superpowers:verification-before-completion` — run the commands and confirm output before claiming success.

- [ ] **Step 1: Clean configure + build**

```bash
cmake -S qt-app -B qt-app/build
cmake --build qt-app/build
```
Expected: completes with no new warnings/errors; no reference to `rfidreader` or `QSerialPort` remains.

- [ ] **Step 2: Run the full test suite**

```bash
ctest --test-dir qt-app/build --output-on-failure
```
Expected: `tst_rfidscandetector` and `tst_rfidkeyboardfilter` both PASS.

- [ ] **Step 3: Manual QA (GUI — run the `WITS` executable)**

Backend prerequisite: `rfid_login.php` reachable at `http://localhost/`, and at least one student row with a **synthetic** `code` value (e.g. `code='TESTCARD01'`).

Verify each, recording the observed result:
1. **Registered card** — simulate a fast burst of the code + Enter (a real reader, or paste-then-Enter is *not* a valid test since it isn't keystroke-paced; type fast or use the reader). The student's photo + recent-logins panel update; a visit is logged in `library_visits`.
2. **Unregistered card** — a code with no matching row shows the non-modal "Card not registered" banner and recovers; the kiosk still accepts the next tap (i.e., the filter is NOT deafened — confirms no modal).
3. **Manual School ID** — typing a numeric School ID + Enter still logs in via `student_login.php` and is **not** misrouted; an admin key still opens the admin window.
4. **Admin window not hijacked** — open the admin window, focus `codeLineEdit`, simulate a scan: the code lands in the field; it does **not** trigger a login.
5. **Both terminators** — if the reader can be set to keypad-Enter, confirm a scan still works.

- [ ] **Step 4: Record results**

If any manual check fails, treat it as a bug (use `superpowers:systematic-debugging`), fix, and re-run Steps 1–3. Do not proceed until all pass.

---

## Post-Implementation (project workflow, after this plan is green)

Per `.claude/rules/workflow.md`:
1. Run `/codex-review` (and optionally `/claude-review` for a same-family code pass) — fix Critical/Important findings, re-submit until APPROVE or 3 rounds.
2. `superpowers:finishing-a-development-branch`.
3. `create-pr` skill (once a remote is configured).

---

## Self-Review (completed by plan author)

- **Spec coverage:** delete dead serial reader (T1 §2), pure `RfidScanDetector` (T1), `RfidKeyboardFilter` with `activeWindow` gating + dual terminator + empty-text skip + `QElapsedTimer` (T2), `handleRfidLogin` → `rfid_login.php` (T3 §7), shared `displayStudent` (T3 §3), debounce ownership in `handleRfidLogin` (T3 §7), non-modal kiosk feedback (T3 §6), first QtTest target under `qt-app/tests/` (T1), manual QA incl. admin-window non-hijack + manual-login non-misroute (T4). All mapped.
- **Placeholder scan:** none — every code/step is concrete.
- **Type consistency:** `feedKey(QChar, qint64) -> std::optional<QString>`, `isTerminator(int) -> bool`, `rfidScanned(const QString&)`, `displayStudent(const QJsonObject&)`, `handleRfidLogin(const QString&)`, `showKioskStatus(const QString&, bool)` are used identically across tasks. Default `maxInterKeyGapMs = 60` (generous per spec §10 failure-mode mitigation).
