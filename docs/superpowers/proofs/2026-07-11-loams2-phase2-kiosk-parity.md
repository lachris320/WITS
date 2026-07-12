# LOAMS 2.0 Phase 2 — Kiosk Parity Checklist

**Purpose.** Task 10 assembled the real `KioskScreen` (BrandPanel + KioskMain +
GuestDialog + LToast, `KioskViewModel`/`GuestViewModel`, RFID install,
cache-first branding) and this checklist enumerates every legacy kiosk
behavior (`mainwindow.cpp`, `guestwindow.cpp`) against it.

**Honesty rule for this document.** I (the implementing agent) can run the
automated suite and launch the app to observe startup/console output, but I
cannot drive the GUI (type an ID, scan a physical RFID card, click through the
guest dialog) or judge visual fidelity against the legacy app side-by-side.
Every row below is marked:

- **PASS (automated)** — proven by a cited test or by code inspection.
- **PENDING HUMAN VERIFICATION** — needs a person to drive the running app;
  exact steps are given.
- **DEFERRED (Phase N)** — explicitly out of Phase 2 scope per the brief.

---

## 1. Login flows

| # | Behavior | Legacy reference | New implementation | Status |
|---|---|---|---|---|
| 1.1 | Numeric input → student ID login (`student_login.php`) | `mainwindow.cpp:192-200` (`toLongLong` numeric check) | `LoginParser::classify()` (`core/loginparser.cpp`) called from `KioskViewModel::submitLogin` (`quick/viewmodels/KioskViewModel.cpp:121-135`), posts to `student_login.php` | **PASS (automated)** — `tests/tst_loginparser.cpp::classifyPureDigitsIsStudent`, `parseStudentSuccess`; `quick/tests/tst_kioskviewmodel.cpp::applyStudentSetsCurrentAndBumpsStats` |
| 1.2 | Non-numeric input → admin key login (`admin_login.php`) → admin surface | `mainwindow.cpp:197-200, 222-227` | `LoginParser::classify()` non-`StudentId` path posts to `admin_login.php`; success emits `KioskViewModel::adminRequested()`, wired in `KioskScreen.qml`: `onAdminRequested: Navigator.showAdmin()` | **PASS (automated)** — `tst_loginparser.cpp::classifyNonNumericIsAdmin`, `classifyDashedIdIsAdmin_legacyQuirk`, `parseAdminSuccess`; Navigator routing itself is `quick/tests/tst_navigator.cpp`. The signal→`Navigator.showAdmin()` QML wiring is exercised by `tst_appshell`'s zero-warning load (proves the binding parses/instantiates) but the actual admin-key round trip through the running UI is behavioral — see PENDING row 1.2b |
| 1.2b | Same as 1.2, driven through the real running kiosk window | — | — | **PENDING HUMAN VERIFICATION** — type an admin key (non-numeric string) into the BrandPanel ID field, press Enter, confirm the surface switches to the Admin placeholder |
| 1.3 | RFID login (`rfid_login.php`), 2.5 s same-card debounce, terminating Enter consumed | `mainwindow.cpp:299-342` (`handleRfidLogin`, debounce at 299-305) | `KioskViewModel::handleRfidScan` (`KioskViewModel.cpp:137-152`) using `LoginParser::shouldDebounceRfid` (default window 2500 ms, `core/loginparser.h:44`) and `LoginParser::isValidRfidCode`; Enter-consumption lives in `RfidEventFilterBase::eventFilter` (`core/rfideventfilterbase.cpp:16-42`), shared by both the legacy `RfidKeyboardFilter` and the new `RfidQuickFilter`, installed via `KioskViewModel::installRfid` (`KioskViewModel.cpp:154-162`) | **PASS (automated)** for parsing/debounce/consume logic — `tst_loginparser.cpp::parseRfidStudentSuccess`, `parseRfidUnregistered`, `validRfidCodeAcceptsAlnum`, `invalidRfidCodeRejectsBounds`, `debounceIgnoresSameCodeInWindow`, `debounceAllowsDifferentCode`, `debounceAllowsSameCodeAfterWindow`; `quick/tests/tst_rfidquickfilter.cpp::recognizedScanEmitsAndConsumesEnter`, `closedGateIgnoresScan`, `printableKeyNotConsumed`; `tst_kioskviewmodel.cpp::invalidRfidCodeSetsErrorStatusNoCrash`. **PENDING HUMAN / on-device** for the physical reader — see row 1.3b |
| 1.3b | Physical RFID scan on the running kiosk window | — | — | **PENDING HUMAN VERIFICATION (on-device, needs reader hardware)** — with a reader attached to the deployment/test PC, scan a registered card on the active `WITSQuick.exe` kiosk window; confirm the student logs in exactly as the legacy app, no stray character/submit from the terminating Enter, and re-scanning the same card within 2.5 s is ignored. If no reader is available at review time, defer this line explicitly to on-device testing before go-live |
| 1.4 | Guest login (`guest_login.php`), required fields (name, company, purpose) | `guestwindow.cpp:28-38` | `GuestViewModel::submitGuest`/`validate` (`quick/viewmodels/GuestViewModel.cpp:17-31`), wired from `GuestDialog.qml`'s Submit button; missing-field path emits `guestFailed` without a network call | **PASS (automated)** — `quick/tests/tst_guestviewmodel.cpp::validateRequiresNameCompanyPurpose`, `applyResponseSuccessEmitsSucceeded`, `applyResponseFailureEmitsFailed`, `submitWithMissingFieldsEmitsFailedNoNetwork`. Contact is optional in both legacy and new code (matches `guestwindow.cpp:35`, which only requires name/company/purpose) |
| 1.4b | Guest dialog driven through the real running kiosk (open, submit missing field, submit success, dialog closes) | — | — | **PENDING HUMAN VERIFICATION** — requires `kiosk/guestEnabled` set `true` in `QSettings` (`KioskViewModel.cpp:29`) for the Guest button to render (`BrandPanel.qml:119`); click Guest, submit with a required field blank → error toast; fill all required fields, submit → dialog closes (`GuestDialog.qml:68`, `onGuestSucceeded → dlg.hide()`) + success toast (`KioskScreen.qml`'s `Connections { target: guestVm }`) |
| 1.5 | Invalid/empty submit → error, auto-dismiss | `mainwindow.cpp:183-186` (`QMessageBox::warning`, modal, no auto-dismiss) → **behavior intentionally upgraded**: `mainwindow.cpp`'s RFID-path errors (`showKioskStatus`, line 278-297) already auto-dismiss after 2500 ms; Phase 2 unifies both paths onto that non-blocking toast | `KioskViewModel::submitLogin` empty-input branch calls `setStatus(...)` (`KioskViewModel.cpp:124-128`); `LToast.qml`'s `autoDismissMs: Theme.motion.toastHold` auto-dismiss timer (`LToast.qml:9,21-27`) | **PASS (automated)** for the status-setting logic (`tst_kioskviewmodel.cpp` covers `setStatus`/error paths indirectly via `invalidRfidCodeSetsErrorStatusNoCrash`); the toast's own auto-dismiss timing has no dedicated Quick test in this task — code-inspection PASS only. **PENDING HUMAN VERIFICATION** — submit an empty ID field, confirm an error toast appears and auto-dismisses after ~2.5 s |

## 2. Kiosk surface elements

| # | Behavior | Legacy reference | New implementation | Status |
|---|---|---|---|---|
| 2.1 | Live feed prepend + newest-row highlight | `mainwindow.cpp:272-274` (`recentLogins.prepend`, no highlight state) — **behavior added, not regressed**: legacy had no "fresh" highlight | `RecentLoginsModel::prepend` (`quick/models/RecentLoginsModel.cpp:61-83`) clears prior `fresh` flags then inserts the new row with `fresh: true`; `KioskFeedRow.qml` consumes `rowFresh` | **PASS (automated)** — `tst_kioskviewmodel.cpp::applyStudentPrependsRowFresh` |
| 2.2 | Feed cap | `mainwindow.cpp:273-274`: fixed **9-row** spotlight panel (`while (recentLogins.size() > 9) removeLast()`) | `RecentLoginsModel::kMaxRows = 40` (`quick/models/RecentLoginsModel.h:42`) | **INTENTIONAL DEVIATION — record, do not imply parity.** The design's kiosk feed is a scrollable `ListView` (not 9 fixed label rows), so the design spec caps it at 40 rows to give the live-feed panel meaningful scroll depth. This is a deliberate design-feed change, not a silent parity match with the legacy 9-row spotlight. **PASS (automated)** that the new cap is enforced as designed — `tst_kioskviewmodel.cpp::modelCapsAtForty` |
| 2.3 | Idle empty-state ("waiting for log in…") | `mainwindow.cpp:387-394` (`"Waiting for log in…"` placeholder row) | `KioskMain.qml`'s greeting text falls back to `"<greeting> — waiting for log in…"` when `!vm.hasStudent` (`KioskMain.qml:27-31`); hero card shows `"Scan your ID to begin"` (`KioskMain.qml:90-96`) | **PASS (automated)** — `quick/tests/tst_qml_kiosk.qml::KioskScreenSmoke::test_loadsIdleState` (loads with no student, i.e. the idle path) plus `KioskMain::test_heroShowsCurrentWhenHasStudent` (inverse case, stubbed true) proves the `hasStudent`-gated branching exists and is wired |
| 2.4 | "NOW SIGNED IN" hero on successful login | `mainwindow.cpp:237-276` (`displayStudent`, updates spotlight labels) | `KioskMain.qml:66-96` hero card, bound to `vm.hasStudent`/`currentFullName`/`currentCourse`/etc | **PASS (automated)** — `tst_qml_kiosk.qml::KioskMain::test_heroShowsCurrentWhenHasStudent`, `test_statTilesBound` |
| 2.5 | Session stat tiles ("visitors today" / "this hour") | Not present in legacy `mainwindow.cpp` (no stat-tile UI) — **new in Phase 2**, per design spec | `KioskViewModel::applyStudentLogin` bumps `m_visitorsToday`/`m_visitorsThisHour` (`KioskViewModel.cpp:79-80`); hour counter rolls over on hour change (`tickClock`, `KioskViewModel.cpp:49-55`) | **PASS (automated)** — `tst_kioskviewmodel.cpp::applyStudentSetsCurrentAndBumpsStats`; `tst_qml_kiosk.qml::KioskMain::test_statTilesBound` (stub). See DEFERRED 3.3 below for the real DB-backed aggregate |
| 2.6 | 1 Hz clock, date, greeting bands (morning/afternoon/evening) | `mainwindow.cpp:124-127, 353-357` (`updateTimeandDate`, 1000 ms `QTimer`, no greeting band) — **greeting band is new in Phase 2** | `KioskViewModel::tickClock` (`KioskViewModel.cpp:36-57`), 1000 ms `QTimer` (`KioskViewModel.cpp:31-33`), greeting thresholds `<12`/`<17`/else | **PASS (automated)** — code inspection of `tickClock`; no dedicated timer-firing Quick test exists in this task (would require a `QTest::qWait` in an offscreen Quick test, out of scope here). **PENDING HUMAN VERIFICATION** — watch the BrandPanel clock tick once per second and the greeting/date match the current time |
| 2.7 | School name / address / hours from settings | `mainwindow.cpp:99-121` (`QSettings` keys `school/name`, `school/address`; font family/size, no `libraryHours` key in legacy) | `KioskViewModel` constructor reads `school/name`, `school/address`, `school/libraryHours` (default `"6 AM – 5 PM"`), `kiosk/guestEnabled` (`KioskViewModel.cpp:24-29`) | **PASS (automated)** — `tst_qml_kiosk.qml::BrandPanel::test_bindsSchoolInfo`, `test_guestButtonVisibleWhenEnabled` (via stub VM, proves the binding path); the real `KioskViewModel` reading real `QSettings` at startup is code-inspection PASS (no unit test spins up a real `QSettings` INI + `KioskViewModel` together in this task) |
| 2.8 | Cache-first branding — no unbranded flash | `mainwindow.cpp:156-162` (`BrandTheme::setCurrent(BrandTheme::loadCachedConfig(...).palette)` before window construction proceeds) | `quick/main.cpp` — identical pattern, applied before `QQmlApplicationEngine` loads any QML (see code excerpt below) | **PASS (ordering guarantee + code inspection)** — the guarantee is structural, not visual: `BrandTheme::setCurrent(...)` runs before `engine.load()`/`loadFromModule(...)`, so no QML — and therefore no frame — can be constructed with an unbranded palette; there is no window in existence yet to render a flash. Mirrors the exact legacy call verbatim (same `BrandTheme::loadCachedConfig`/`setCurrent` API, same `QSettings("MyCompany","MyApp")` store), and `BrandTheme::loadCachedConfig`/`saveCachedConfig` are covered by `tests/tst_brandtheme.cpp`. (The two console-only `Start-Process` launches in §4 confirmed a clean, warning-free startup but cannot observe rendered pixels, so no "saw the branded frame" claim is made here.) |

```cpp
// qt-app/quick/main.cpp (added this task)
{
    QSettings brandingStore(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
    BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
}
```

## 3. Deferred to later phases (explicitly out of Phase 2 scope)

| # | Item | Reason / target phase |
|---|---|---|
| 3.1 | Student photo fetch + fade animation (`mainwindow.cpp:237-270`) | Phase 4 — admin logo import + asset handling. Phase 2 uses a logo-circle placeholder (`BrandPanel.qml:29-42`, `"LOGO"` text) and no photo fetch |
| 3.2 | Logo image + poster background rendering (`mainwindow.cpp:414-492`, `updateLogo`/`updatePoster`/`syncPosterBg`) | Phase 4 — admin logo import + asset handling. Phase 2 uses the flat brand gradient (`BrandPanel.qml:16-19`) |
| 3.3 | Live admin → kiosk updates (school info / logo / poster / clear-attendance / guest-toggle signals, `mainwindow.cpp:66-85`) | Phase 4 — admin settings surface. Phase 2 reads `QSettings` once at `KioskViewModel` construction (`KioskViewModel.cpp:24-29`), not live-updated |
| 3.4 | Real "visitors today / this hour" backed by a DB aggregate endpoint | Phase 3 — dashboard. Phase 2 derives them as in-session counters (`KioskViewModel.cpp:79-80`), reset to 0 on process restart — matches the design demo's behavior, not a persisted server-side count |
| 3.5 | Background branding re-sync + live logo re-theme | Phase 4. Phase 2 is cache-first-at-startup only (§2.8); the legacy `BrandingController::fetchRemoteConfig()` background-refresh path (`mainwindow.cpp:163-172`) has no Quick-side equivalent yet |
| 3.6 | Fullscreen-by-default for deployment | Set at deploy/launch time. Legacy `mainwindow.cpp:44` calls `showFullScreen()` unconditionally; `WITSQuick`/`AppShell.qml` currently launches windowed (`AppShell.qml:7-9`, `width/height` only, no `visibility: Window.FullScreen`) — intentional for dev; must be revisited before go-live per the Phase 1 render-check note |
| 3.7 | Dark mode + full a11y pass | Phase 5 |
| 3.8 | Backend hardening (parameterized queries in `student_login.php`/`rfid_login.php`/`guest_login.php`; remove `phpinfo.php`/`testupload.php`; `apiconfig.h` base URL) | Phase 6, contract-tests-first — server-side PHP, out of this client-only task's reach entirely |

## 4. Automated verification performed for this task

- **Full suite:** `ctest --test-dir C:\b\loams-p2 --output-on-failure` → **22/22 passed**, including the new `tst_qml_kiosk::KioskScreenSmoke::test_loadsIdleState`.
- **Zero-QML-warning gate:** `ctest -V -R "tst_qml_kiosk|tst_qml_components|tst_appshell"` — no `QWARN`/"does not support customization"/anchor-conflict lines in any of the three; `tst_appshell::loadsWithZeroWarnings` (which loads the *real* `AppShell → KioskScreen{appWindow} → GuestDialog`, i.e. the composition with the anchors setup used in the brief's own `KioskScreen.qml` code) asserts and confirms an empty QML-message list.
- **App launch (normal):** `WITSQuick.exe` launched and stayed running for several seconds with **zero console output** (stdout+stderr both empty under `QT_FORCE_STDERR_LOGGING=1`), then closed cleanly.
- **App launch (`--software`):** same result — zero console output, stays running, closes cleanly. Confirms the Phase 1 deployment-hardware fallback is intact.
- **Legacy `WITS.exe`:** `cmake --build C:\b\loams-p2 --target WITS` — builds clean (no new warnings from the Task 5 RFID-base refactor); launched briefly and stayed running with no crash.

## 5. Sign-off

**Manual smoke run by:** ____________________  **date:** ____________________

(Drive the running `WITSQuick.exe`: BrandPanel visuals, student-ID login →
feed/hero/stats, admin-key login → Admin placeholder, invalid submit → toast,
guest flow if enabled, console zero-warnings including the
`ApplicationWindow → QQuickWindow*` coercion check noted in §6 below.)

**RFID on-device verified by:** ____________________  **date:** ____________________

(Physical reader attached to the kiosk window: scan behaves exactly as the
legacy app — student logs in, terminating Enter consumed, 2.5 s same-card
debounce holds.)

## 6. `installRfid` window-coercion check (review Round 1, Low)

`KioskViewModel::installRfid` takes a `QQuickWindow*` directly (`KioskViewModel.h:79`),
not a `QObject*`/`QQuickItem*` requiring a manual cast. `AppShell.qml`'s
`ApplicationWindow { id: appShell }` is passed straight through:
`KioskScreen { appWindow: appShell }` → `Component.onCompleted: kioskVm.installRfid(screen.appWindow)`.
QML's invokable-argument marshalling resolves `ApplicationWindow` (backed by
`QQuickApplicationWindow`, a `QQuickWindow` subclass) to `QQuickWindow*` via
`qobject_cast` under the hood — during the two manual `WITSQuick.exe` launches
performed for this task (§4), **no `ApplicationWindow → QQuickWindow*`
conversion warning fired** (console output was empty both times). No code
change was needed for this item.
