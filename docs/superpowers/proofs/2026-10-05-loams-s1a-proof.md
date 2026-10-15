# LOAMS S1a — client networking seam: Layer 9 regression proof

Plan: [`2026-10-05-loams-s1a-client-networking-seam.md`](../plans/2026-10-05-loams-s1a-client-networking-seam.md) (Task 14).
Branch `feat/s1a-client-networking-seam`, base `master` `b713dde`. Toolchain: Qt 6.11.1 MinGW 64-bit, CMake + Ninja,
default `LOAMS_BUILD_LEGACY_WIDGETS=OFF`. All data in this document is synthetic; no admin key (real or smoke), DB
credential, real student record or screenshot is included.

## 1. Commits tested

| Evidence | Commit | Notes |
|---|---|---|
| Task 14 Steps 1-4 (clean build, full suite, ON config, plumbing diff, static checks) | `6c1b8aa` | Fresh `rm -rf C:/b/s1a` build, `cmake --build -j 4` |
| Final-review fix wave re-check (full suite + access-log bracket, G1/G2, Step 3, Step 4) | `df4ea4f` (branch HEAD) | Incremental build of `C:/b/s1a` (`ninja: no work to do` before the run); fix wave `6c1b8aa..df4ea4f` = `24722f8`, `3e0b8dc`, `df4ea4f` |
| GATE G2 thread log line | `3e8e550` (Task 5b) | `C:/b/s1a-gate-g2.log`; `tst_qmlfactorygate` re-passed at `6c1b8aa` and `df4ea4f` |
| Step 6 smoke, Pass 1 | `b713dde` (`master`) | `WITSQuick.exe` sha256 `28c3e67c1b76900800c7d235cd59316f48b06e84ce22ead530d11d006368dd53` |
| Step 6 smoke, Pass 2 | `df4ea4f` (branch) | `WITSQuick.exe` sha256 `912582ab3b0e15359f708c5c6eaa48fdc8698b564e216df5b783d4396e4fc1f0` |

The Step 2 developer ON configuration was run at `6c1b8aa` only; the fix wave touched no legacy-Widgets source.

## 2. Automated evidence

### Step 1 — clean full build + full suite, proven offline (`6c1b8aa`)

```
configure rc=0
build rc=0
no new warnings in S1a files
total warnings: 1
before=8317
ctest rc=0
after=8317
access.log lines added during ctest: 0
no backend request from the test run
```

The single build warning is pre-existing (`git blame` `4489f75f`), not in an S1a file:
`qt-app/quick/tests/tst_importviewmodel.cpp:33:15: warning: ignoring return value of 'virtual bool QFile::open(...)' [-Wunused-result]`.

```
100% tests passed, 0 tests failed out of 68
Total Test time (real) =  27.17 sec
```

`Not Run` occurrences in the ctest log: **0**.

Re-check at `df4ea4f` (`C:/b/s1a-ctest-final.log`): `ctest -j 8 rc=0`, `100% tests passed, 0 tests failed out of 68`,
`Not Run` 0, dev Apache `access.log` `before=8317` / `after=8317` → **0 lines added**, no backend request.

### Mandatory gates

| Gate | Evidence | Result |
|---|---|---|
| G1 — `std::atomic_load`/`atomic_store` on `shared_ptr` warning-free | `qt-app/core/CMakeLists.txt:73-74` sets `COMPILE_OPTIONS "-Wall;-Wextra;-Werror"` on `transport/transportpolicy.cpp`; it compiled in the clean build; `tst_transportpolicy` Passed (`61/68 Test #66` at `6c1b8aa`; `34/68 Test #66` at `df4ea4f`). First run (Task 2): 0 warnings in `C:/b/s1a-gate-g1.log` | **PASS** |
| G2 — QML `Image` / `Canvas.loadImage` use the installed factory's manager | `tst_qmlfactorygate` Passed (`8/68 Test #6` at `6c1b8aa`; `18/68 Test #6` at `df4ea4f`). First run (Task 5b) log line below | **PASS** |

```
QINFO  : TestQmlFactoryGate::imageLoadUsesInstalledFactoryManager() GATE G2: image request ran on the non-GUI (pixmap reader) thread
```

Neither gate failed at any point; no workaround was applied.

### Step 2 — developer ON configuration (`6c1b8aa`)

```
cmake -S qt-app -B C:/b/s1a-legacy -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DLOAMS_BUILD_LEGACY_WIDGETS=ON   # rc=0
cmake --build C:/b/s1a-legacy -j 4 --target WITS tst_rfidkeyboardfilter tst_responsive_ui                                 # rc=0
1/3 Test  #1: tst_legacywidgetsgate ............   Passed    0.02 sec
2/3 Test #41: tst_rfidkeyboardfilter ...........   Passed    0.04 sec
3/3 Test #44: tst_responsive_ui ................   Passed    0.50 sec
100% tests passed, 0 tests failed out of 3
```

**PASS** — `WITS` builds; 3/3.

### Step 3 — Passthrough evidence: tests changed only by injection plumbing

```
git diff master...HEAD -U0 -- 'qt-app/tests/*' 'qt-app/quick/tests/*' | grep -E '^-[^-]' > C:/b/s1a-removed.txt
PLUMBING='AccessControlHub \w+(\(&\w+\))?;|ViewModel \w+\(nullptr, &\w+\);|Controller \w+\(nullptr\);|^-\s*\w+ViewModel \w+;'
grep -cE "$PLUMBING" C:/b/s1a-removed.txt      -> 249
grep -vE "$PLUMBING" C:/b/s1a-removed.txt      -> (no output)   # total removed lines: 249
```

**PASS** at `6c1b8aa` and again at `df4ea4f` (249 plumbing lines, 0 other removed/modified test lines).

### Step 4 — static checks (`6c1b8aa`; re-checked at `df4ea4f`)

| Check | Output | Result |
|---|---|---|
| 4.1 `new QNetworkAccessManager` / `make_*<QNetworkAccessManager>` in `qt-app/core` + `qt-app/quick` (non-test) | (no output) | PASS |
| 4.2 `ignoreSslErrors` in client source | (no output) | PASS |
| 4.3 default-constructed seam owners in `quick/tests` | 11 sites, all inside request-free ownership tests (list below) | PASS |
| 4.4 `cmake -LA -N C:/b/s1a \| grep LOAMS_BUILD_LEGACY_WIDGETS` | `LOAMS_BUILD_LEGACY_WIDGETS:BOOL=OFF` | PASS |
| 4.5 `git diff --stat master...HEAD -- qt-app/quick/qml` | (no output) | PASS |

4.3 matches and their enclosing functions (at `6c1b8aa`; at `df4ea4f` the hub line moved 326 → 339 because static_asserts were added above it, same function):

```
tst_accesscontrolhub.cpp:326       -> TestAccessControlHub::defaultHubManagerIsSeamManager()
tst_accesscontrolviewmodel.cpp:402 -> TestAccessControlViewModel::defaultConstructionUsesTransportSeam()
tst_dashboardviewmodel.cpp:128     -> TestDashboardViewModel::defaultConstructionUsesTransportSeam()
tst_databaseviewmodel.cpp:950      -> TestDatabaseViewModel::defaultConstructionUsesOneSeamManager()
tst_guestviewmodel.cpp:61          -> TestGuestViewModel::defaultConstructionUsesTransportSeam()
tst_importviewmodel.cpp:139        -> TestImportViewModel::defaultConstructionUsesTransportSeam()
tst_kioskviewmodel.cpp:427         -> TestKioskViewModel::defaultConstructionUsesTransportSeam()
tst_reportingviewmodel.cpp:1067    -> TestReportingViewModel::defaultConstructionUsesTransportSeam()
tst_searchviewmodel.cpp:194        -> TestSearchViewModel::defaultConstructionUsesTransportSeam()
tst_settingsviewmodel.cpp:731      -> TestSettingsViewModel::defaultConstructionUsesTransportSeam()
tst_visitlogsviewmodel.cpp:226     -> TestVisitLogsViewModel::defaultConstructionUsesTransportSeam()
```

## 3. Layer 9 manual smoke (Steps 5-7)

### Environment — Option A (disposable VM)

| Item | Value |
|---|---|
| Hypervisor / guest | VirtualBox 7.2.20 (Hyper-V/NEM backend on the host), Windows 11 Enterprise evaluation |
| Server stack | XAMPP 8.2.4 = Apache 2.4.56 / PHP 8.2.4 / MariaDB 10.4.28 (matches dev); PHP extension set matched to the dev `php.ini` |
| Server code | committed `deliverables/loams_api` (`git diff --stat b713dde HEAD -- deliverables/` is empty: S1a changes no server code) |
| Database | created from a structure-only dump (`INSERT INTO` count 0; no rows left the dev box); synthetic records only |
| Admin key | smoke-only key; only its hash was seeded, inside the VM; the key never appeared in the kit, chat or this document |
| Clients | `windeployqt`-deployed `master` and `branch` folders, no `config.ini` in either |
| Checkpoints | `S1A-GOLDEN` (golden v1), then `S1A-GOLDEN-2` (golden v2). **Both passes started from `S1A-GOLDEN-2`** |

Harness (kit at `C:/b/s1a-smoke/kit`, outside the repo, never committed): `smoke-state.sh` (the plan's helper plus the
CRLF fix in §5), `SMOKE-CHECKLIST.md` (owner steps), and small in-guest PowerShell helpers — `vm-provision` /
`vm-setup` / `vm-php-match` / `vm-db-init` (provision XAMPP, match PHP extensions, create the DB, seed the smoke admin
hash), `vm-golden` / `vm-golden2` + `golden2.sql` (record the golden fingerprint; apply golden v2), `vm-pass`
(per pass: fingerprint → `start-<pass>.fp`, backend check → `<pass>.backend`, start Apache, launch that pass's
`WITSQuick.exe` from the same process), `vm-start-servers` / `vm-shutdown` (clean start/stop; never savestate),
`vm-agent` (whitelisted actions while the kiosk keeps focus: Apache stop/start, turnstile bench swipe),
`vm-wedge` (HID-wedge burst simulator) and `vm-genlog` (MariaDB general query log for diagnostics).

Golden v2 synthetic roster (from `students-golden2.txt`): `Test Student A`-`H`; A school ID `2110000001`, B card
`TEST0002`, others `21-1-000N`; photos on A-D, none on E-H; H carries card `TEST-0001` and status `0` (created through
Register — see the pre-existing register bug in §6).

### Start state — fingerprints

`golden.fingerprint` (v2; sha256 `775210290b99f2850ecb1f1d6dd308dd1bfc063482cc1aaaa1668c30083655b0`). The file
begins with an empty line:

```

db 9aec69063ca31bbd791b4aa130d8a76a2b6160da9bb58576df306fef30888c7b
uploads d246c6bb573597baec2ab6eba4ae662d7106a9559322f989d18143f2263b8130 10
loams_api.uploads ef6c8238b303074a4b7f512d00fda92367afad7e0e656fe2a8c640919cd9c3b3 2
registry 3e215c0704209fe2a3c631583be72c241ee74b919229525e5d2f133e59c8e296
appdata absent
count admin 1
count library_visits 0
count students 8
count turnstile_events 0
count visitors 0
```

`start-master.fp` (Pass 1, 2026-10-13) and `start-branch.fp` (Pass 2, 2026-10-15) are byte-identical to it (same
sha256 `775210290b99…655b0`):

```
diff golden.fingerprint start-master.fp     -> (empty)
diff golden.fingerprint start-branch.fp     -> (empty)
diff start-master.fp start-branch.fp        -> (empty)
```

Superseded fingerprints (kept as evidence, not used as a start state):

| File | sha256 | Note |
|---|---|---|
| `golden-v1.fingerprint` | `7146eefbce1526ac554b728a6d7aa2212b3ab470d3a68b711be29340855aaced` | Identical to v2 except `db 433095142a95afb14379eadbe8a4c516bb91066bf08990b945f31cba4ddfec59` (before `golden2.sql`) |
| `golden.fingerprint.crlf-bug` | `a676803901ec328b19251c8474c64a7744767d256d53e69c753f0708e2cd4c75` | Discarded: table names carried `\r`, counts blank (`count admin^M ` …) — see §5 item 1 |

### Backend pinning

```
backend master effective=http://localhost/loams_api/ source=default env.shell=unset env.user=unset env.system=unset config.ini=absent config.BaseURL=absent
backend branch effective=http://localhost/loams_api/ source=default env.shell=unset env.user=unset env.system=unset config.ini=absent config.BaseURL=absent
```

Both `backend` checks exited 0; the lines are identical apart from the pass label: same `effective=` URL, source
`default`, `WITS_API_BASE_URL` unset in shell/user/system environments, no `config.ini`.

### Step 6 — smoke table

Pass 1 `master` ran 2026-10-13, Pass 2 branch ran 2026-10-15. **All 17 rows PASS on both passes; zero behavioural
differences between master and branch.**

| # | Area | master | branch | Notes (both passes unless stated) |
|---|---|---|---|---|
| K1 | Kiosk student | PASS | PASS | `2110000001` typed → Test Student A welcome card, remote synthetic photo, feed row, counter 1 |
| K2 | Kiosk RFID | PASS (scan) | PASS (scan) | Wedge burst `TEST0002` → Test Student B, photo, feed row, counter 2; unregistered code → "Card not registered". Debounce sub-check not exercisable in the VM (§5 item 3) |
| K3 | Kiosk errors | PASS | PASS | `9999999999` → "Invalid School ID"; Apache stopped → "Network error. Please try again." (~2-4 s) |
| K4 | Guest | PASS | PASS | Guest dialog → "Guest logged successfully." |
| K5 | Admin entry | PASS | PASS | Smoke-only key at the kiosk → admin shell (Dashboard) |
| A1 | Dashboard | PASS | PASS | Stats + hourly and by-department bars load |
| A2 | Search | PASS | PASS | Department/course chips; "Test" → 8 results; photos A-D, initials E-H |
| A3 | Visit logs | PASS | PASS | Students Today / Guests This Week load |
| A4 | Database | PASS | PASS | Filter (Dept Two → 3 rows); **M5 shared-manager check**: All reload + immediate row double-click → edit-dialog course list loaded (Computing, Science); single edit saved; bulk edit; register Test Student I with photo → 9 rows; delete I; deactivate + delete Test Department Three; CSV export (648 bytes both) |
| A5 | Import | PASS | PASS | Template saved; synthetic CSV + ZIP → "5 of 7 rows already exist" → Continue → "2 imported · 5 skipped · 0 failed" |
| A6 | Reporting | PASS | PASS | Departments load; Day report (`api.php/reports/data`); time analytics; PDF + Excel export saved; print dialog opens (cancelled) |
| A7 | Settings | PASS | PASS | Logo import → preview + "Logo imported."; Save → "Settings saved." + sidebar logo; admin info saved; admin key → second synthetic key → back to the smoke key ("Admin key changed." ×2); reset visits on Test Department Two with manifest, re-entering the smoke key → success (proves the key was restored) |
| A8 | Guarded ops | PASS | PASS | Reset Visits with a wrong key → "Admin key rejected. Please sign in again." |
| A9 | Access Control | PASS | PASS | Feed loads; monitoring on → Connected / last contact 0 s; off → Disconnected / Monitoring off |
| T1 | Turnstile polling | PASS | PASS | Bench `SearchCardAcs` `Card=TEST0002` → `AcsRes 1` → kiosk welcome card Test Student B with photo + feed row |
| T2 | Reconnect + cursor | PASS | PASS | Apache stopped → "Degraded"; restarted → "Connected"; second swipe `Card=2110000001` → after Refresh exactly 2 entries, no duplicate or skipped entry |
| R | Assets | PASS | PASS | Remote photos (kiosk + search); missing photo → initials (E-H, imported G); `default.jpg` sentinel (photo-less Test Student J, card `TEST0010`, turnstile entry) → "TS" initials; imported logo on kiosk BrandPanel, sidebar and settings preview; bundled QML renders; `WITSQuick.exe --software` renders the kiosk |

Run-specific values that differ only because of input or date, not behaviour: Pass 2's K2 recorded one extra valid
`TEST0002` visit (a queued burst arriving > 2.5 s after the previous scan), so A1/A3/A6 visit counts are one higher
on the branch (A1 "Visitors Today" 2 vs 3; A6 Day report 2 vs 3 visits, 2 unique on both); report dates, timestamps
and export sizes (PDF 527686 vs 527447 bytes, XLSX 7041 vs 7047 bytes) differ with the run date and data.

**Turnstile setup for T1/T2:** in-VM bench simulation only. Controller-style JSON `SearchCardAcs` POSTs (Reader 0,
Serial 9001) from `127.0.0.1` to the VM's own `turnstile.php`, whose `ALLOWED_CONTROLLER_IP` was pinned to
`127.0.0.1` in the VM's copy only. The production gate PC was never involved.

### Step 7 — environment reset

- VM and all its snapshots deleted (`VBoxManage unregistervm --delete-all`).
- `structure-only.sql` deleted.
- Both smoke keys (the smoke-only admin key and the A7 second synthetic key) deleted.
- Nothing on the dev box's database, registry or app data was mutated by the smoke (the dev box only supplied the structure-only dump).

## 4. Passthrough proves no behaviour change

| # | Evidence | Result |
|---|---|---|
| 1 | `tst_transportpolicy::prepareIsIdentityInPassthrough` | Passed (in 68/68) |
| 2 | `tst_policyenforcingnam` wire-parity suite (GET, GET+query, form POST, JSON POST to `api.php/reports/data`, multipart; status/body/error; redirects; `ConnectionRefused`; `reply->request()`) | Passed (in 68/68) |
| 3 | `tst_qmlresourceinventory` — every inventoried resource loads (or falls back) as before on factory-made managers | Passed (in 68/68) |
| 4 | Step 3 diff — only injection plumbing changed in tests | 249 / 0 (§2) |
| 5 | Layer 9 manual smoke — zero differences from `master` | 17/17 PASS both passes (§3) |

## 5. Deviations and harness notes

1. **`smoke-state.sh fingerprint` CRLF bug (plan script).** Native `mysql.exe` emits CRLF, so table names carried
   `\r` (ERROR 1103, blank counts — `set -e` does not catch failures inside `$(...)`). Fixed in the kit with
   `| tr -d '\r'` on the table-list and COUNT command substitutions; the first golden
   (`golden.fingerprint.crlf-bug`) was discarded. The same fix is folded into the plan's embedded script.
2. **Golden v2 synthetic IDs.** The plan's IDs cannot drive the kiosk on either build: a typed login is classified
   as a student only if it is all digits (`LoginParser::classify`), and an RFID code must match `[0-9A-Za-z]{3,64}`
   (`LoginParser::isValidRfidCode`). `golden2.sql` set Test Student A's school ID to `2110000001` and Test Student
   B's card to `TEST0002` (synthetic) and was applied on a fresh `S1A-GOLDEN` revert before taking `S1A-GOLDEN-2`.
   A first master attempt on golden v1 was discarded before any row was recorded (files kept as `*-v1`).
3. **K2 debounce sub-check not exercisable in the VM.** The scan detector timestamps keys when the GUI thread
   processes them (60 ms inter-key window) and the VM's GUI thread fragments injected bursts nondeterministically.
   The card scan itself PASSES on both passes; debounce stays covered by unit tests (`shouldDebounceRfid`). Same on
   both builds.
4. **Pass 2 incident.** The host C: drive filled to 0 bytes mid-A6; VirtualBox auto-paused the VM
   (`DrvVD_DISKFULL`). About 5 GB was freed (owner-approved cleanup of Visual Studio installer temp leftovers) and the
   VM resumed; some queued keystrokes were lost and retyped. No application behaviour was affected.
5. **NEM backend quirks (environment only).** Guest reboot hangs were cleared with a VM reset; restoring a saved
   state blue-screened the guest, so savestate was never used; MariaDB 10.4 occasionally hung after a clean InnoDB
   shutdown (killed after "Shutdown completed").

## 6. Pre-existing observations (identical on master and branch — not S1a regressions)

| Observation | Note |
|---|---|
| Register stores status `0` instead of `Active` | `deliverables/loams_api/register_student.php:93`: `bind_param("sssssssiss", …)` binds `$status` as an integer; should be `"ssssssssis"` (status `s`, visits `i`) |
| Deleted department name persists | Database department filter, Reporting and Settings department lists still show it |
| Opening-hours mismatch | Kiosk shows "Open 6 AM – 5 PM" while Settings shows 8–17 |
| Bulk-edit toast counts affected rows | "Updated 1 student" when one of two rows already had the value |
| Access Control counters | Entries Today / Last Entry / table update only on Refresh |
| Department combo | Typing into the empty Department combo shows "undefined" |
| Clock format | Clock once showed "17:25:29 PM" during setup |
| Register dialog | Department list stale until page reload |

## 7. Rollback

See the plan's [Rollback Considerations](../plans/2026-10-05-loams-s1a-client-networking-seam.md#rollback-considerations):
S1a is client-only and behaviour-neutral (Passthrough); revert the S1a merge commit on `master` via a normal PR.
The Option A smoke left nothing to restore (VM deleted, §3 Step 7).
