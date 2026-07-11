# Phase 1 Verification & Proof Assembly — Branding Engine

Branch: `feat/brand-theme-engine`, HEAD `7a42111`. Baseline: `master` @ `341fac5`.

All commands below were run from the worktree root
`.worktrees/brand-theme-engine` with the toolchain prepended to `PATH`:

```powershell
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
```

Full command outputs are saved under `.superpowers/sdd/proofs/` (gitignored,
not committed — referenced here by path):

- `build-debug.log` — Debug configure + build
- `build-release.log` — Release configure + build
- `baseline-build.log` — master (341fac5) Debug configure + build, in a
  disposable detached worktree, used only for the warning-delta comparison
- `ctest-debug.log` — full `ctest` run
- `step5-tests-diffstat.log`, `step6-loams-api-diff.log`, `step7-sql-diff.log`
  — diff outputs for Steps 5–7

---

## Condition 1 — Debug+Release builds succeed, no new warnings

**Step 1 — Debug build**

```
cmake -S qt-app -B qt-app/build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build-debug
```

Configure exit code: `0`. Build exit code: `0`. Full output:
`.superpowers/sdd/proofs/build-debug.log`.

**Step 2 — Release build**

```
cmake -S qt-app -B qt-app/build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
cmake --build qt-app/build-release
```

Configure exit code: `0`. Build exit code: `0`. Full output:
`.superpowers/sdd/proofs/build-release.log`.

Both builds link `WITS.exe` and all 12 test executables (`[116/116]` /
equivalent, no `FAILED:` lines).

**Step 3 — Warning delta vs. master baseline**

A disposable, detached worktree was created for the master baseline:

```
git worktree add ../baseline-master --detach 341fac5
```

It was configured and built the same way (Debug) into
`../baseline-master/qt-app/build-debug`, logged to
`.superpowers/sdd/proofs/baseline-build.log`. Configure+build exit code: `0`.

Warning extraction (`Select-String -Pattern "warning" -CaseSensitive:$false`)
over each log, then deduplicated to unique line signatures:

| Log | Raw "warning" hits | Unique warning lines |
|---|---|---|
| `build-debug.log` (branch) | 6 | 5 |
| `build-release.log` (branch) | 6 | 5 |
| `baseline-build.log` (master 341fac5) | 6 | 5 |

`Compare-Object` between the baseline's unique warning-line set and each
branch log's set returned **empty diffs both directions** — i.e. the branch
introduces **zero new warnings** relative to master.

All 5 unique warning lines are the same pre-existing CMake dev warning,
triggered by `libs/QXlsx/CMakeLists.txt:22 (find_package)` pulling in
`Qt6GuiPrivate` (vendored QXlsx, out of scope per project instructions):

```
CMake Warning at .../QtPublicDependencyHelpers.cmake:339 (message):
  This project is using headers of the GuiPrivate module ...
  You can disable this warning by setting QT_NO_PRIVATE_MODULE_WARNING to ON.
CMake Warning at .../QtPublicWalkLibsHelpers.cmake:285 (message):
  The GuiPrivate target is mentioned as a dependency for QXlsx, but not declared. ...
(× 2, once per finalize-target pass)
```

No compiler warnings (GCC/MinGW `-W...`) appear in any of the three logs —
only these CMake configure-time dev warnings, identical across baseline and
branch, Debug and Release.

Baseline worktree removed after comparison: `git worktree remove
../baseline-master --force` (confirmed via `git worktree list`).

**Result: PASS.** Debug and Release both build to completion (exit 0), and
the new-vs-baseline warning delta is zero.

---

## Condition 2 — Existing tests pass unmodified except tst_theme.cpp

**Step 4 — Full test run**

```
ctest --test-dir qt-app/build-debug --output-on-failure
```

Full output: `.superpowers/sdd/proofs/ctest-debug.log`. Result:

```
      Start  1: tst_rfidscandetector
 1/12 Test  #1: tst_rfidscandetector .............   Passed    9.47 sec
      Start  2: tst_rfidkeyboardfilter
 2/12 Test  #2: tst_rfidkeyboardfilter ...........   Passed    1.00 sec
      Start  3: tst_apiconfig
 3/12 Test  #3: tst_apiconfig ....................   Passed    0.95 sec
      Start  4: tst_theme
 4/12 Test  #4: tst_theme ........................   Passed    0.98 sec
      Start  5: tst_responsive_ui
 5/12 Test  #5: tst_responsive_ui ................   Passed    2.36 sec
      Start  6: tst_visitorcontroller
 6/12 Test  #6: tst_visitorcontroller ............   Passed    2.88 sec
      Start  7: tst_importcontroller
 7/12 Test  #7: tst_importcontroller .............   Passed    2.90 sec
      Start  8: tst_studentcontroller
 8/12 Test  #8: tst_studentcontroller ............   Passed    1.08 sec
      Start  9: tst_reportcontroller
 9/12 Test  #9: tst_reportcontroller .............   Passed    1.14 sec
      Start 10: tst_reportrenderer
10/12 Test #10: tst_reportrenderer ...............   Passed    3.37 sec
      Start 11: tst_brandtheme
11/12 Test #11: tst_brandtheme ...................   Passed    1.37 sec
      Start 12: tst_brandingcontroller
12/12 Test #12: tst_brandingcontroller ...........   Passed    1.14 sec

100% tests passed, 0 tests failed out of 12

Total Test time (real) =  28.69 sec
```

**Step 5 — Unmodified-tests proof**

```
git diff 341fac5 --stat -- qt-app/tests/
```

```
 qt-app/tests/CMakeLists.txt             |  46 ++++
 qt-app/tests/tst_brandingcontroller.cpp | 136 +++++++++
 qt-app/tests/tst_brandtheme.cpp         | 473 ++++++++++++++++++++++++++++++++
 qt-app/tests/tst_theme.cpp              |  86 ++++--
 4 files changed, 724 insertions(+), 17 deletions(-)
```

Exactly four paths touched under `qt-app/tests/`: `tst_theme.cpp` modified
(the only allowed pre-existing-test change), two new files added
(`tst_brandtheme.cpp`, `tst_brandingcontroller.cpp`), and `CMakeLists.txt`
extended (additions register the two new test targets). No other `tst_*.cpp`
appears in the diff — the 8 pre-existing suites
(`tst_rfidscandetector`, `tst_rfidkeyboardfilter`, `tst_apiconfig`,
`tst_responsive_ui`, `tst_visitorcontroller`, `tst_importcontroller`,
`tst_studentcontroller`, `tst_reportcontroller`, `tst_reportrenderer` — note
that's 9, all unmodified) all pass unchanged per the ctest output above.

**Result: PASS.** 12/12 green; only `tst_theme.cpp` was modified among
pre-existing test files.

---

## Condition 3 — tst_theme.cpp rewritten behavioral, passes

Commit `7a42111` — `test(theme): replace hardcoded-hex assertions with
behavioral legibility checks` — rewrote `qt-app/tests/tst_theme.cpp` (86
lines changed per Step 5's diffstat) to assert on palette *properties*
(contrast ratio, relative lightness, role distinctness) instead of pinned hex
values, so a rebrand cannot break the suite unless it actually breaks
legibility.

Zero-hex-literal grep over the rewritten file:

```
Select-String -Path "qt-app\tests\tst_theme.cpp" -Pattern '#[0-9A-Fa-f]{6}'
```

Hit count: **0**. No `#RRGGBB` literal appears anywhere in the file — every
assertion goes through `BrandColorMath::contrastRatio`,
`BrandColorMath::relativeLuminance`, `.lightness()` comparisons, or
`QColor::isValid()` / inequality checks against named `WitsTheme::Color::*`
constants (see e.g. `textIsLegibleOnItsSurfaces` asserting `contrastRatio(...)
>= 4.5`, `adminAndKioskRolesStayDistinct` asserting `AdminPrimary !=
KioskPrimary`).

ctest section for this suite (from `ctest-debug.log`):

```
      Start  4: tst_theme
 4/12 Test  #4: tst_theme ........................   Passed    0.98 sec
```

**Result: PASS.**

---

## Condition 4 — tst_brandtheme passes: palette-from-logo, determinism, different-logo, min-contrast

ctest line (from `ctest-debug.log`):

```
      Start 11: tst_brandtheme
11/12 Test #11: tst_brandtheme ...................   Passed    1.37 sec
```

`qt-app/tests/tst_brandtheme.cpp` (473 lines added per Step 5) declares 23
slots. The ones covering this condition:

- **Palette-from-logo**: `validPngProducesBrandedPalette`,
  `validSvgProducesBrandedPalette`, `twoToneLogoMapsPrimaryAndSecondary`
- **Determinism**: `sameLogoTwiceIsDeterministic`
- **Different-logo → different palette**: `differentLogosDifferentPalettes`
- **Minimum contrast**: `generatedPalettesMeetMinContrast`

All 23 slots in the suite pass as part of the single `tst_brandtheme` ctest
entry (Qt Test reports a suite as failed if any slot fails; the suite's
"Passed" status covers every slot).

**Result: PASS.**

---

## Condition 5 — branding_config CREATE TABLE + endpoints exist

**Step 7 — schema.** `deliverables/sql/branding_config.sql` (new file, see
Condition 10 below for the diff proof):

```sql
-- Branding engine persistence (Phase 1, PRD condition 5). One singleton row
-- (id = 1) holding the current branding state. New table only — no existing
-- table is altered.
CREATE TABLE IF NOT EXISTS branding_config (
  id           TINYINT UNSIGNED NOT NULL,
  theme_mode   VARCHAR(10)      NOT NULL DEFAULT 'auto',
  palette_json TEXT             NOT NULL,
  logo_hash    VARCHAR(64)      NOT NULL DEFAULT '',
  updated_at   TIMESTAMP        NOT NULL DEFAULT CURRENT_TIMESTAMP
                                ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

**Step 6 — endpoints.** Two new PHP files under `deliverables/loams_api/`:

- `get_branding.php` — **no auth required** (read-only; returns the current
  singleton row as JSON, or `"branding": null` if no row exists yet). Uses
  `$conn->query(...)` (no user input in the query).
- `save_branding.php` — **`requireAdminAuth($conn)`** (from
  `auth_helper.php`) gates the write path before any input is processed.
  Validates `theme_mode` against an allow-list (`auto`/`manual`), validates
  `palette` decodes to a JSON object, validates `logo_hash` matches
  `^[0-9a-f]{64}$` (SHA-256 hex) when non-empty, then does a parameterized
  `INSERT ... ON DUPLICATE KEY UPDATE` via `$stmt->bind_param("sss", ...)` —
  no string-concatenated SQL.

**Result: PASS.**

---

## Condition 6 — Logo validation: valid-SVG / valid-PNG / corrupt outcomes

From `qt-app/tests/tst_brandtheme.cpp`, the "Logo validation + extraction"
slot group:

- **Valid PNG** → `validPngProducesBrandedPalette`
- **Valid SVG** → `validSvgProducesBrandedPalette`
- **Corrupt file** → `corruptFileRejectedWithSpecificError` (asserts a
  specific, non-generic error message is populated and the palette is
  rejected)
- Related edge cases also covered in the same group:
  `unsupportedExtensionRejected`, `missingFileRejected`, `webpNeverCrashes`,
  `greyscaleLogoFallsBack`.

ctest line: `11/12 Test #11: tst_brandtheme ... Passed 1.37 sec` (from
`ctest-debug.log`, same run as Condition 4).

**Result: PASS.**

---

## Condition 7 — Startup cache-first walkthrough

**Step 8.** `qt-app/mainwindow.cpp`, lines 156–172 (`MainWindow`
constructor):

```cpp
156	    // Branding: cache-first, then background sync. The cached palette is
157	    // applied immediately (no network wait); the backend is fetched in the
158	    // background and re-applied + re-cached only if strictly newer.
159	    {
160	        QSettings brandingStore(QLatin1String("MyCompany"), QLatin1String("MyApp"));
161	        BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
162	    }
163	    m_brandingController = new BrandingController(networkManager, this);
164	    connect(m_brandingController, &BrandingController::remoteConfigLoaded, this,
165	            [](const BrandingConfig &remote) {
166	        QSettings store(QLatin1String("MyCompany"), QLatin1String("MyApp"));
167	        if (BrandTheme::isNewer(remote, BrandTheme::loadCachedConfig(store))) {
168	            BrandTheme::setCurrent(remote.palette);
169	            BrandTheme::saveCachedConfig(store, remote);
170	        }
171	    });
172	    m_brandingController->fetchRemoteConfig();
```

Walkthrough:

1. **Lines 159–162** — synchronous cache apply: `BrandTheme::loadCachedConfig`
   reads the locally persisted `BrandingConfig` from `QSettings` and
   `BrandTheme::setCurrent` applies its palette immediately, before any
   network I/O — the UI never blocks on the backend.
2. **Line 163** — `BrandingController` is constructed (owns the
   `QNetworkAccessManager`-backed fetch).
3. **Lines 164–171** — the `remoteConfigLoaded` signal handler is connected
   *before* the fetch starts. When it fires (async, after the ctor returns
   and control reaches the event loop), it re-reads the cache, and only if
   `BrandTheme::isNewer(remote, cached)` is true does it re-apply
   (`setCurrent`) and re-persist (`saveCachedConfig`) — a stale or
   not-newer remote response is a no-op.
4. **Line 172** — `fetchRemoteConfig()` kicks off the async HTTP request
   after the handler is wired up.

`isNewer` (`qt-app/brandtheme.cpp:406-410`) is a simple timestamp
comparison: `remote.updatedAt.isValid() && (!local.updatedAt.isValid() ||
remote.updatedAt > local.updatedAt)`.

**Result: PASS.**

---

## Condition 8 — Manual-mode hook, no UI

**Step 9.** `qt-app/brandtheme.cpp`, lines 412–419
(`regenerateFromLogo`):

```cpp
412	bool regenerateFromLogo(BrandingConfig &config, const QString &logoPath, QString *errorMsg)
413	{
414	    // Manual-mode hook (PRD condition 8): auto-regeneration is skipped
415	    // entirely when the config is in Manual mode — config is left untouched.
416	    if (config.mode == ThemeMode::Manual) {
417	        if (errorMsg) errorMsg->clear();
418	        return false;
419	    }
```

When `config.mode == ThemeMode::Manual`, the function returns `false`
immediately — `config` is left completely untouched (no palette/hash/
timestamp mutation), and `errorMsg` is cleared rather than set (this is a
deliberate no-op, not an error condition). Behavior verified by
`tst_brandtheme.cpp::manualModeSkipsRegeneration` (part of the passing
`tst_brandtheme` suite; contrasted with `autoModeRegenerates` for the
non-Manual path).

UI-exposure check:

```
Select-String -Path "qt-app\*.ui" -Pattern "Manual" -CaseSensitive:$false
```

over all four tracked `.ui` files (`adminwindow.ui`, `attachFilesDialog.ui`,
`guestwindow.ui`, `mainwindow.ui`) — hit count: **0**. There is no UI control
in Phase 1 that lets a user select or toggle Manual mode; the hook exists in
the backend/model layer only, ready for a future UI to set `config.mode`.

**Result: PASS.**

---

## Condition 9 — No existing PHP endpoint changed

**Step 6.**

```
git diff 341fac5 --name-status -- deliverables/loams_api/
```

```
A	deliverables/loams_api/get_branding.php
A	deliverables/loams_api/save_branding.php
```

Only `A` (added) lines — **zero `M`** lines. No pre-existing endpoint under
`deliverables/loams_api/` was touched by this branch.

**Result: PASS.**

---

## Condition 10 — No existing MySQL schema altered

**Step 7.**

```
git diff 341fac5 --name-status -- deliverables/sql/
```

```
A	deliverables/sql/branding_config.sql
```

Only one `A` line, the new table's definition file — zero `M` lines, so no
existing `.sql` deliverable was modified.

`ALTER TABLE` search across the actual changed code/deliverables (scoped
to `deliverables/` and `qt-app/`, which is where a schema change would have
to land):

```
git diff 341fac5 -- deliverables/ | Select-String -Pattern "ALTER TABLE" -CaseSensitive:$false   → 0 hits
git diff 341fac5 -- qt-app/       | Select-String -Pattern "ALTER TABLE" -CaseSensitive:$false   → 0 hits
```

(An unscoped search over the *entire* branch diff, including
`.superpowers/sdd/task-8-brief.md`, does surface 1 hit — but it is the
literal string "ALTER TABLE" quoted inside this task's own planning-doc
prose, describing the check to run, not a SQL statement. It was inspected
directly and confirmed to not be executable SQL.)

`branding_config.sql` uses `CREATE TABLE IF NOT EXISTS` for a **new**
singleton table — no `ALTER TABLE`, no modification to any pre-existing
table.

**Deploy-time verification for the DBA** (live check against the production
database, to be run immediately before and after applying
`deliverables/sql/branding_config.sql`, for every pre-existing table the app
depends on — e.g. `students`, `visitors`, `admins`, or whatever the current
schema names them):

```sql
SHOW CREATE TABLE <existing_table_name>;
-- apply deliverables/sql/branding_config.sql
SHOW CREATE TABLE <existing_table_name>;
-- diff the two outputs — expect byte-for-byte identical CREATE TABLE text
```

If any pre-existing table's `SHOW CREATE TABLE` output differs before vs.
after, that is a regression outside this migration's scope and must block
deployment.

**Result: PASS** (repo-side proof complete; live `SHOW CREATE TABLE`
before/after is a deploy-time action for the DBA, recorded above as the
exact command to run).

---

## Summary

| Condition | Area | Result |
|---|---|---|
| 1 | Debug+Release build, zero new warnings | PASS |
| 2 | Existing tests pass unmodified (except tst_theme.cpp) | PASS |
| 3 | tst_theme.cpp rewritten behavioral | PASS |
| 4 | tst_brandtheme: palette-from-logo/determinism/different-logo/min-contrast | PASS |
| 5 | branding_config table + 2 PHP endpoints exist | PASS |
| 6 | Logo validation valid-SVG/valid-PNG/corrupt | PASS |
| 7 | Startup cache-first walkthrough | PASS |
| 8 | Manual-mode hook, no UI | PASS |
| 9 | No existing PHP endpoint changed | PASS |
| 10 | No existing MySQL schema altered | PASS |

All ten conditions verified. 12/12 tests pass. Debug and Release builds
succeed with zero new warnings relative to the `master` (341fac5) baseline.
