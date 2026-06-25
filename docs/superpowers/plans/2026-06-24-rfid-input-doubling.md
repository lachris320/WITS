# RFID Input-Doubling Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop the kiosk RFID login from rejecting registered cards by fixing the application-global key filter that double-counts each scanned character.

**Architecture:** The RFID key filter is installed once on `qApp`. Qt invokes a qApp-global event filter once per recipient as an unconsumed key propagates up the focused widget's parent chain, so each scanned character is fed to the detector twice. The fix gates `RfidKeyboardFilter::eventFilter` to act only on the key's *primary target* (the focus widget, or the active window when nothing is focused) and ignore the propagated ancestor copies. One physical key → one `feedKey`.

**Tech Stack:** C++17, Qt 6.11.1 (Widgets), QtTest, CMake + Ninja, MinGW kit.

**Design spec:** `docs/superpowers/specs/2026-06-24-rfid-input-doubling-design.md` (claude-review APPROVE, round 1).

## Global Constraints

- Keep the `qApp`-global install of the filter (`qt-app/mainwindow.cpp:85-86`) — it must capture scans regardless of focus. Fix the double-count, do not move the filter.
- Do NOT consume printable keys (the filter must still let manual typing reach the focused `QLineEdit`); only the recognized-scan terminator is consumed, exactly as today.
- Keep the existing `http://localhost/loams_api/` transport as-is (out of scope, accepted).
- No real student PII, card codes, or secrets in committed code, tests, or logs. Use synthetic codes (`"123"`, `"ABC1"`) in tests.
- TDD: red → green. Commit the regression test together with the fix via the `commit` skill — never raw `git add`/`git commit`.
- Build toolchain (MinGW kit; tools NOT on PATH — prepend in every build command):
  - `$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH`
  - Configure: `cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`
  - Build: `cmake --build qt-app/build`
  - Test (this target only): `ctest --test-dir qt-app/build -R tst_rfidkeyboardfilter --output-on-failure`
  - Full suite: `ctest --test-dir qt-app/build --output-on-failure`

---

## Task 1: Gate the event filter on the key's primary target

**Files:**
- Modify: `qt-app/rfidkeyboardfilter.cpp:19-46` (add the primary-target gate inside `eventFilter`)
- Test: `qt-app/tests/tst_rfidkeyboardfilter.cpp` (add the doubling regression test; update the 4 existing `eventFilter` behavioral tests so their `watched` is the primary target)

No CMake change: the `tst_rfidkeyboardfilter` target is already registered at `qt-app/tests/CMakeLists.txt:14-27` and links `Qt::Widgets`.

**Interfaces:**
- Consumes (unchanged, already in the codebase):
  - `bool RfidKeyboardFilter::eventFilter(QObject *watched, QEvent *event)` — protected; the test reaches it via the existing `PublicFilter::callEventFilter(QObject*, QEvent*)` subclass.
  - `RfidScanDetector::feedKey(QChar, qint64)` → `std::optional<QString>`; defaults `minCodeLength = 3`, so a recognized code must be ≥ 3 chars. Terminator (`'\n'`/`'\r'`) returns the buffer if length ≥ 3, else `std::nullopt`, and resets.
  - Test helpers already in the file: `makeActive(QWidget&)`, `feedPrintable(PublicFilter&, QWidget& watched, Qt::Key, const QString& text)` → `bool`, `feedTerminator(PublicFilter&, QWidget& watched)` → `bool`.
- Produces: no new public API. Behavior change only — one `feedKey` per physical key.

---

- [ ] **Step 1: Update the existing `eventFilter` tests to pass `watched` = the primary target**

The existing tests pass a throwaway `QWidget watched;` that is *not* the active window. After the gate lands, `watched != target` would make the filter early-return and the tests would no longer exercise the scan path. Repoint each to the active window (the primary target) so they stay meaningful. These edits keep the tests GREEN against the *current* (unfixed) code too — the unfixed filter ignores `watched`.

Replace the four behavioral test functions in `qt-app/tests/tst_rfidkeyboardfilter.cpp` (currently lines 81-163) with these versions:

```cpp
// ---- Test 1: recognized scan on the active login window ----
void TestRfidKeyboardFilter::recognizedScanEmitsAndConsumesEnter()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    // Feed printable keys to the primary target (the active window) — none
    // should be consumed.
    QCOMPARE(feedPrintable(filter, w, Qt::Key_A, QStringLiteral("A")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_B, QStringLiteral("B")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_C, QStringLiteral("C")), false);
    QCOMPARE(feedPrintable(filter, w, Qt::Key_1, QStringLiteral("1")), false);

    // Feed terminator — must be consumed and signal emitted
    bool consumed = feedTerminator(filter, w);
    QVERIFY(consumed);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("ABC1"));
}

// ---- Test 2: login window is NOT the active window — scan is ignored ----
void TestRfidKeyboardFilter::inactiveLoginWindowIgnoresScan()
{
    QWidget loginWindow;
    loginWindow.show();

    QWidget other;
    other.show();
    makeActive(other); // different window is active

    PublicFilter filter(&loginWindow);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    // Keys are delivered to the active window (`other`); the active-window gate
    // blocks them before the primary-target gate is reached.
    feedPrintable(filter, other, Qt::Key_A, QStringLiteral("A"));
    feedPrintable(filter, other, Qt::Key_B, QStringLiteral("B"));
    feedPrintable(filter, other, Qt::Key_C, QStringLiteral("C"));
    feedPrintable(filter, other, Qt::Key_1, QStringLiteral("1"));

    bool consumed = feedTerminator(filter, other);

    // Gate not open — Enter must NOT be consumed, no signal emitted
    QVERIFY(!consumed);
    QCOMPARE(spy.count(), 0);
}

// ---- Test 3: printable key is never consumed even on the active window ----
void TestRfidKeyboardFilter::printableKeyNotConsumed()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);

    bool consumed = feedPrintable(filter, w, Qt::Key_X, QStringLiteral("X"));
    QVERIFY(!consumed);
}

// ---- Test 4: empty-text event (e.g. Shift) is skipped — not consumed ----
void TestRfidKeyboardFilter::emptyTextEventSkipped()
{
    QWidget w;
    w.show();
    makeActive(w);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Shift, Qt::NoModifier, QString());
    bool consumed = filter.callEventFilter(&w, &ev);

    QVERIFY(!consumed);
    QCOMPARE(spy.count(), 0);
}
```

- [ ] **Step 2: Add the failing doubling regression test**

Add a new private slot declaration. In the `private slots:` block (currently lines 25-36), add after `void emptyTextEventSkipped();`:

```cpp
    void propagatedKeyCountedOnce();
```

Then add this test function immediately before the `QTEST_MAIN(...)` line at the bottom of the file:

```cpp
// ---- Test 5: a key propagated to an ancestor is counted only once ----
void TestRfidKeyboardFilter::propagatedKeyCountedOnce()
{
    QWidget w;
    w.show();
    makeActive(w);

    // The fix resolves a key's "primary target" as focusWidget() else
    // activeWindow(). With nothing focused, target == &w, so a delivery to &w
    // is the primary copy and a delivery to any other widget is a propagated
    // ancestor copy to be ignored. Assert the precondition so the test stays
    // deterministic under the offscreen QPA.
    QVERIFY(QApplication::focusWidget() == nullptr);

    PublicFilter filter(&w);
    QSignalSpy spy(&filter, &RfidKeyboardFilter::rfidScanned);

    QWidget ancestor; // a different recipient — stands in for the propagated copy

    // Simulate Qt invoking the app-global filter twice per physical key: once
    // for the focus target (&w) and once as the key propagates to an ancestor.
    // Code "123" is >= the detector's minCodeLength (3) in its single form, so
    // the FIXED behavior still produces a recognized scan.
    feedPrintable(filter, w,        Qt::Key_1, QStringLiteral("1"));
    feedPrintable(filter, ancestor, Qt::Key_1, QStringLiteral("1"));
    feedPrintable(filter, w,        Qt::Key_2, QStringLiteral("2"));
    feedPrintable(filter, ancestor, Qt::Key_2, QStringLiteral("2"));
    feedPrintable(filter, w,        Qt::Key_3, QStringLiteral("3"));
    feedPrintable(filter, ancestor, Qt::Key_3, QStringLiteral("3"));

    // The terminator is delivered to the primary target; on a recognized scan
    // the filter consumes it, so propagation stops (no ancestor copy).
    QVERIFY(feedTerminator(filter, w));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("123"));
}
```

- [ ] **Step 3: Build and run the test target to verify the doubling test FAILS (RED)**

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_rfidkeyboardfilter --output-on-failure
```

Expected: `tst_rfidkeyboardfilter` FAILS on `propagatedKeyCountedOnce` with a mismatch like:
```
FAIL!  : TestRfidKeyboardFilter::propagatedKeyCountedOnce() Compared values are not the same
   Actual   (spy.at(0).at(0).toString()): "112233"
   Expected (QStringLiteral("123"))      : "123"
```
The four updated behavioral tests (Step 1) PASS. If `propagatedKeyCountedOnce` PASSES here, the test is not reproducing the bug — stop and fix the test before implementing. If instead it FAILS at `QVERIFY(QApplication::focusWidget() == nullptr)` (rather than at the `"112233"` vs `"123"` value comparison shown above), the offscreen QPA gave the widget a focus widget and the test's precondition is wrong — investigate that before treating the failure as the expected RED.

(If `cmake --build` reports the build dir is missing, configure first:
`cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"`.)

- [ ] **Step 4: Implement the primary-target gate**

In `qt-app/rfidkeyboardfilter.cpp`, inside `eventFilter`, insert the gate **after** the active-window check (current line 26) and **before** `auto *ke = ...` (current line 28). The function becomes:

```cpp
bool RfidKeyboardFilter::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    // Only intercept on the kiosk/login window — never the admin window etc.
    if (QApplication::activeWindow() != m_loginWindow)
        return QObject::eventFilter(watched, event);

    // A qApp-global event filter is invoked once per recipient as an unconsumed
    // key propagates up the parent chain — which double-counts each scanned
    // character. Act only on the key's primary target (the focus widget, or the
    // active window when nothing is focused); skip the propagated ancestor
    // deliveries.
    QWidget *target = QApplication::focusWidget();
    if (!target)
        target = QApplication::activeWindow();
    if (watched != target)
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

`<QApplication>` and `<QWidget>` are already included at the top of the file — no new includes.

- [ ] **Step 5: Rebuild and run the test target to verify GREEN**

```powershell
cmake --build qt-app/build
ctest --test-dir qt-app/build -R tst_rfidkeyboardfilter --output-on-failure
```

Expected: `tst_rfidkeyboardfilter` PASSES — all five behavioral tests plus the three `isTerminator` tests green.

- [ ] **Step 6: Run the full suite to confirm no regressions**

```powershell
ctest --test-dir qt-app/build --output-on-failure
```

Expected: every registered test passes (`tst_rfidscandetector`, `tst_rfidkeyboardfilter`, `tst_apiconfig`, `tst_rfidcodeinspect`, `tst_theme`, `tst_responsive_ui`). No new compiler warnings from the changed files.

- [ ] **Step 7: Commit (via the `commit` skill)**

Stage exactly the two changed files and commit the regression test together with the fix. Suggested message:

```
fix(rfid): count each scanned key once in the kiosk login filter

The qApp-global RFID key filter was invoked once per recipient as an
unconsumed key propagated up the focused widget's parent chain, so every
scanned character was fed to the detector twice — a registered card
0003241370 arrived as 00000033224411337700 and the backend rejected it
as "not registered". Gate eventFilter to act only on the key's primary
target (focus widget, else active window) and ignore the propagated
ancestor copies, so one physical key yields one feedKey. Manual typing
is unaffected (the focused QLineEdit consumes its keys, so they never
propagate). Adds a regression test driving a propagated key and asserting
a single emitted code.
```

---

## Out of scope / handoff (NOT part of this plan)

- **Manual end-to-end verification (user).** With the temporary `RFID SCAN` diagnostic still in `mainwindow.cpp`, the user re-scans the sample card and confirms it now logs `value="0003241370" len=10` and the student logs in. This is the real-world confirmation the unit test cannot give.
- **Temporary diagnostic removal (PII).** The temporary card-code `qDebug` from the Phase-1 diagnostic is removed before merge — owned by Task 4 of `docs/superpowers/plans/2026-06-24-rfid-code-mismatch.md`, run after the manual verification above passes.
- **User's leftover working-tree WIP** (`deliverables/loams_api/rfid_login.php`, `qt-app/adminwindow.cpp`, the stray `mainwindow.cpp:~278` qDebug) — the user owns these; do not stage them.
