# RFID Input-Doubling Fix — Design

**Status:** Draft for review (design-spec mode)
**Date:** 2026-06-24
**Branch:** `feat/responsive-ui`
**Supersedes:** the Phase-2 fix options in
`docs/superpowers/specs/2026-06-24-rfid-code-mismatch-design.md` (its
value-mismatch hypotheses were ruled out by the Phase-1 diagnostic — see below).

---

## Goal

A registered student who taps their RFID card at the kiosk is logged in. Today
the scan is rejected as "Card not registered" because the kiosk **doubles every
character** of the scanned code before sending it to the backend.

## Root cause (confirmed by the Phase-1 diagnostic)

The Phase-1 diagnostic (`RfidCodeInspect::describe` wired into `handleRfidLogin`)
captured a scan of a card whose stored `students.code` is `0003241370`:

```
RFID SCAN: value="00000033224411337700" len=20 hex=30 30 30 30 30 30 33 33 32 32 44 44 ...
RFID RESPONSE: {"status":"error","message":"RFID not registered"}
```

`00000033224411337700` is `0003241370` with **every character duplicated**
(len 20 = 2 × 10). Decisive follow-up: the **same card scanned into Notepad
produces `0003241370` (single)**. So the reader and the OS are correct — **WITS
itself double-counts each keystroke.**

Mechanism, established by code inspection + Qt semantics:

- The RFID key filter is a single instance installed **application-global** on
  `qApp` ([`qt-app/mainwindow.cpp:85-86`](../../../qt-app/mainwindow.cpp)), and
  `RfidKeyboardFilter::eventFilter` feeds the detector exactly **one character
  per invocation** ([`qt-app/rfidkeyboardfilter.cpp:44`](../../../qt-app/rfidkeyboardfilter.cpp)).
  There is exactly one filter and one `rfidScanned` connection (no double-install).
- Therefore the doubling means `eventFilter` is **invoked twice per physical
  key**. Qt invokes an **application-global** event filter once for **every
  recipient** as an unconsumed key event propagates up the focused widget's
  parent chain (`QCoreApplicationPrivate::notify_helper` runs the app filters for
  each widget it delivers to). During a scan the focused widget is **not** one
  that consumes the digit keys (the username `QLineEdit` is not focused when a
  card is tapped), so each `KeyPress` is delivered to the focus target **and**
  propagated to its parent — two filter invocations, two `feedKey` calls, every
  character doubled.
- Manual typing does **not** double because then the `QLineEdit` is focused and
  *consumes* each key (no propagation) — the same reason Notepad shows single.

This is purely an input-path defect. The database value is correct; normalization,
backfill, and data-entry (the prior spec's Phase-2 options) are all irrelevant.

## Design

### The fix

Make the filter act on each physical key **once**, by processing only the key's
*primary target* delivery and ignoring the propagated ancestor copies. In
`RfidKeyboardFilter::eventFilter`, after the existing type-check and the
active-window gate, add:

```cpp
// A qApp-global event filter is invoked once per recipient as an unconsumed key
// propagates up the parent chain — which double-counts each scanned character.
// Act only on the primary target (the focus widget, or the active window when
// nothing is focused); skip the propagated ancestor deliveries.
QObject *target = QApplication::focusWidget();
if (!target)
    target = QApplication::activeWindow();
if (watched != target)
    return QObject::eventFilter(watched, event);
```

Properties:
- Targets the actual cause (propagation), not a symptom. No timing heuristics.
- In the app: focus null → `target` = active login window = first (and only,
  top-level) recipient → processed once. Focus on a non-consuming widget →
  processed on that widget, skipped on its parent. Focus on the consuming
  `QLineEdit` → no propagation anyway → processed once. All cases yield one
  `feedKey` per key.
- Pure, synchronous, no new state, no ownership changes.

### Alternatives considered and rejected

- **Consume printable keys when active (return `true`).** Stops propagation, but
  the filter is active whenever the login window is active, so it would also
  swallow **manual** typing into the username field — breaking manual login.
- **De-duplicate by `QEvent*` pointer.** Propagation re-delivers the *same*
  event object, but distinct successive key events frequently **reuse the freed
  address** (the existing tests construct a stack `QKeyEvent` per char at the
  same address). Pointer equality would wrongly drop legitimately-distinct
  characters. Unsafe.
- **De-duplicate by (char + same-millisecond timestamp).** Fragile: relies on
  two distinct characters never landing in the same millisecond, which a fast
  reader can violate; obscures the real cause.
- **Install the filter on the window instead of `qApp`.** A window-scoped filter
  only sees events sent to the window; it would miss scans while a child widget
  holds focus and consumes/propagates differently. The global install is
  intentional (capture scans regardless of focus); keep it and fix the
  double-count.

## Testing

`tst_rfidkeyboardfilter` already drives `eventFilter` directly via a `PublicFilter`
subclass, controlling `QApplication::activeWindow()` with `setActiveWindow`. The
fix is testable in the same style (no real windowing needed):

- **RED — reproduce the doubling and assert it's gone.** With the login window
  active and no focus widget (so `target` == active window), feed each printable
  key event **twice** — once with `watched` == the active window (the primary
  target) and once with `watched` == a *different* widget (the propagated
  ancestor) — then the terminator. Assert `rfidScanned` emits the **single**
  code (e.g. `"12"`), not the doubled code (`"1122"`). Against the unfixed code
  this test sees `"1122"` and fails; with the gate it passes.
  - **Pin the target deterministically.** The gate resolves `target` as
    `focusWidget()` first, falling back to `activeWindow()`. Under the
    `offscreen` QPA used by the test, a stray focus widget would make `target`
    something other than the active window and silently flip which `watched`
    the test must pass — turning RED/GREEN flaky. So the test must assert its
    starting state, not assume it: confirm `QApplication::focusWidget()` is
    null (call `clearFocus()` on any widget that might hold it, or never give
    one focus) so `target` provably resolves to the active window, and pass
    the active window as the "primary target" `watched`. Make this explicit in
    the test body rather than relying on the platform default.
- **Update the existing behavioral tests** so their `watched` is the primary
  target (the active window) rather than an unrelated dummy widget, matching how
  Qt actually delivers the first copy — otherwise the gate would skip them.
  (`isTerminator` unit tests are unaffected.)
- Verify each new/changed test is meaningful (RED before the fix, GREEN after).
- **Build + full ctest** green on the MinGW/Ninja toolchain.
- **Manual end-to-end (the real verification):** with the temporary `RFID SCAN`
  diagnostic still in place, re-scan the sample card and confirm it now logs
  `value="0003241370" len=10` and the student logs in.

## Security / PII

- No new logging. The temporary card-code `qDebug` from the Phase-1 diagnostic
  stays **only** until the manual re-scan confirms the fix, then is removed
  (Task 4 of `docs/superpowers/plans/2026-06-24-rfid-code-mismatch.md`, which
  remains the owner of that cleanup). No card codes in committed code or
  persistent logs.
- Keep the existing `http://localhost/loams_api/` transport as-is (accepted,
  out of scope).

## Non-goals

- Barcode scanning (later phase).
- The responsive-UI / full-screen work and the invisible-input dark-mode bug
  (separate branches).
- Re-opening normalization/backfill/operational fixes — the measured root cause
  rules them out.

## Open questions

1. During a kiosk scan, which widget actually holds focus (or none)? The fix is
   correct regardless (it processes the first/target delivery in every case), but
   knowing it would let us optionally also direct focus to the username field.
2. Should the username field visibly echo the scanned code at all? Out of scope
   here; the fix preserves current behavior (printable keys still reach the
   focused widget).
