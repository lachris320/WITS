# LOAMS 2.0 Phase 1 — Gate Proof

**Purpose.** This document assembles the Phase 1 exit-gate evidence for the LOAMS
2.0 Widgets-to-Qt-Quick modernization (proposal §20 Phase 1 deliverables a–d,
spec §8/§9). It is the deliverable of Task 7 of the Phase 1 implementation plan.

**Scope note.** This proof covers everything CI/build-machine-verifiable: build
matrix, full test suite, and the wiring-level evidence for the RFID and render
risk retirements. Three items remain **human gates on real hardware/eyes** and
are called out explicitly in the "Pending human gates" section below — they are
not satisfied by this document.

**Branch:** `loams2-phase1-core` (worktree). **Commits covered** (Tasks 1–6, in
order): extraction of the `witscore` static library, the minimal `WITSQuick`
executable with an empty `AppShell`, `RfidQuickFilter` (RFID spike), the
software-rendering fallback + render-check checklist, `ThemeViewModel` +
`Theme.qml`, and the ten `L*` design-system component skeletons.

---

## Build matrix

All three configurations were clean-configured and built from a git worktree
checkout of this branch, using Qt 6.11.1 (MinGW 13.1.0 kit) with Ninja.

| Configuration | Configure | Build | New warnings | ctest |
|---|---|---|---|---|
| Debug (`qt-app/build`) | clean, succeeded | 205/205 targets, succeeded | 0 | 17/17 passed |
| Release (separate build dir) | clean, succeeded | 205/205 targets, succeeded | 0 | 17/17 passed |
| Rollback (`-DBUILD_LEGACY_WIDGETS=OFF`, separate build dir) | clean, succeeded | 193/193 targets, succeeded (12 fewer than the default build — the omitted legacy `WITS` sources) | 0 | 17/17 passed |

**Commands run** (Debug shown; Release and rollback used separate `-B` build
directories with `CMAKE_BUILD_TYPE=Release` and `-DBUILD_LEGACY_WIDGETS=OFF`
respectively, per the same pattern):

```
cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Debug
cmake --build qt-app/build
ctest --test-dir qt-app/build --output-on-failure
```

**Warning classification.** Every configure step (Debug, Release, and the
rollback build) emitted the *same* deterministic set of CMake-configure-time
notices, none of which are compiler warnings and none of which are new
regressions introduced by this branch:

1. `Qt6GuiPrivate` "This project is using headers of the GuiPrivate module..."
   — pre-existing, from `libs/QXlsx` (vendored, not project code). Known-harmless
   per the task brief.
2. Two "`GuiPrivate` target is mentioned as a dependency for QXlsx, but not
   declared" notices — same root cause as (1); `QXlsx`'s own build config
   references `GuiPrivate` without declaring it, which CMake's Qt6 helper
   scripts flag as an FYI, not an error. Harmless — same class as the QXlsx/
   GuiPrivate warning called out in the brief.
3. **New, not previously classified, but confirmed benign:** a CMake note that
   `witsquickmodule`'s QML module target path is `LOAMS` while its build
   `OUTPUT_DIRECTORY` is `.../build/quick` (i.e. the directory name doesn't
   match the QML URI). The note is scoped explicitly to tooling: *"Tooling such
   as qmllint may not work correctly."* It reproduced identically and
   deterministically across all three configure runs (Debug, Release, rollback)
   — it is a structural artifact of `quick/CMakeLists.txt`'s
   `qt_add_qml_module(witsquickmodule URI LOAMS ...)` call living in a directory
   named `quick/` rather than `LOAMS/`, not a build-breaking or behavior-
   affecting issue. Compilation, linking, and all 17 tests are unaffected.
   Flagging here for visibility rather than silently folding it into the
   "known-harmless" set — a future task could rename the directory to silence
   it, but that is a cosmetic follow-up, not a Phase 1 blocker.

No compiler warnings (GCC/MinGW) were observed in any of the three builds —
every `Building CXX object ...` line completed with no `warning:` diagnostics.

---

## Test evidence

**Command:** `ctest --test-dir qt-app/build --output-on-failure`

**Result (Debug, representative — identical pass count on Release and rollback):**

```
      Start  1: tst_appshell
 1/17 Test  #1: tst_appshell .....................   Passed    2.53 sec
      Start  2: tst_rfidquickfilter
 2/17 Test  #2: tst_rfidquickfilter ..............   Passed    1.19 sec
      Start  3: tst_themeviewmodel
 3/17 Test  #3: tst_themeviewmodel ...............   Passed    1.25 sec
      Start  4: tst_qml_theme
 4/17 Test  #4: tst_qml_theme ....................   Passed    2.28 sec
      Start  5: tst_qml_components
 5/17 Test  #5: tst_qml_components ...............   Passed    2.32 sec
      Start  6: tst_rfidscandetector
 6/17 Test  #6: tst_rfidscandetector .............   Passed   11.28 sec
      Start  7: tst_rfidkeyboardfilter
 7/17 Test  #7: tst_rfidkeyboardfilter ...........   Passed    1.27 sec
      Start  8: tst_apiconfig
 8/17 Test  #8: tst_apiconfig ....................   Passed    1.19 sec
      Start  9: tst_theme
 9/17 Test  #9: tst_theme ........................   Passed    1.15 sec
      Start 10: tst_responsive_ui
10/17 Test #10: tst_responsive_ui ................   Passed    2.47 sec
      Start 11: tst_visitorcontroller
11/17 Test #11: tst_visitorcontroller ............   Passed    2.69 sec
      Start 12: tst_importcontroller
12/17 Test #12: tst_importcontroller .............   Passed    3.06 sec
      Start 13: tst_studentcontroller
13/17 Test #13: tst_studentcontroller ............   Passed    1.30 sec
      Start 14: tst_reportcontroller
14/17 Test #14: tst_reportcontroller .............   Passed    1.42 sec
      Start 15: tst_reportrenderer
15/17 Test #15: tst_reportrenderer ...............   Passed    3.32 sec
      Start 16: tst_brandtheme
16/17 Test #16: tst_brandtheme ...................   Passed    1.69 sec
      Start 17: tst_brandingcontroller
17/17 Test #17: tst_brandingcontroller ...........   Passed    1.31 sec

100% tests passed, 0 tests failed out of 17
```

Release and rollback (`-DBUILD_LEGACY_WIDGETS=OFF`) runs produced the identical
`100% tests passed, 0 tests failed out of 17` result (different per-test
timings only).

**Test inventory — 12 → 17 targets:**

The 12 baseline targets (pre-existing, unchanged by this branch except for the
`witscore` extraction move — see Check 1):

`tst_rfidscandetector`, `tst_rfidkeyboardfilter`, `tst_apiconfig`, `tst_theme`,
`tst_responsive_ui`, `tst_visitorcontroller`, `tst_importcontroller`,
`tst_studentcontroller`, `tst_reportcontroller`, `tst_reportrenderer`,
`tst_brandtheme`, `tst_brandingcontroller`.

The 5 additions (Tasks 2, 3, 5, 6 — all under `qt-app/quick/tests/`):

- `tst_appshell` — loads `AppShell` via `QQmlApplicationEngine::loadFromModule`,
  asserts one root object and zero captured QML warnings/errors.
- `tst_rfidquickfilter` — exercises `RfidQuickFilter` under a real
  `QQuickWindow` (offscreen QPA).
- `tst_themeviewmodel` — exercises the `ThemeViewModel` C++ class.
- `tst_qml_theme` — QML-side test of the `Theme.qml` singleton.
- `tst_qml_components` — QML-side test instantiating all ten `L*` components.

---

## Check 1 — `witscore` extraction (Task 1, deliverable a)

- 12/12 baseline tests were green both before and after the `witscore` move
  (per Task 1's own before/after verification; re-confirmed green again in this
  task's Debug/Release/rollback runs above).
- Each of the 12 baseline test targets still direct-compiles its
  class-under-test `.cpp` from `qt-app/tests/CMakeLists.txt` (source-of-truth
  read for this proof):
  - `tst_rfidscandetector` → `core/rfidscandetector.cpp`
  - `tst_rfidkeyboardfilter` → `rfidkeyboardfilter.cpp` (root, not moved —
    depends on `Widgets`) + `core/rfidscandetector.cpp`
  - `tst_apiconfig` → header-only (`core/apiconfig.h`), no `.cpp` under test
  - `tst_theme` → header-only (root `theme.h`, deliberately **not** moved into
    `core/` — it is UI-styling-adjacent and still included by
    `core/brandtheme.cpp`), no `.cpp` under test
  - `tst_responsive_ui` → tests `.ui` layout files directly via `UiTools`, no
    `.cpp` under test
  - `tst_visitorcontroller` → `core/visitorcontroller.cpp`
  - `tst_importcontroller` → `core/importcontroller.cpp`
  - `tst_studentcontroller` → `core/studentcontroller.cpp`
  - `tst_reportcontroller` → `core/reportcontroller.cpp` (still omits
    `reportrenderer.cpp`, as before the move)
  - `tst_reportrenderer` → `core/reportrenderer.cpp` (still omits
    `reportcontroller.cpp`, as before the move)
  - `tst_brandtheme` → `core/brandtheme.cpp` (keeps **both** `${CMAKE_SOURCE_DIR}`
    and `${CMAKE_SOURCE_DIR}/core` on its include path, because
    `brandtheme.cpp` includes the root-staying `theme.h`)
  - `tst_brandingcontroller` → `core/brandingcontroller.cpp` +
    `core/brandtheme.cpp` (same root-`theme.h` include-path subtlety)
- `WITS.exe` still builds and links against `witscore` (confirmed present at
  `qt-app/build/WITS.exe`, built from this worktree, not a stale binary from a
  different checkout).

Confirms proposal §20 Phase 1 deliverable (a).

---

## Check 2 — AppShell + Theme + components (Tasks 2, 5, 6; deliverable b)

- `WITSQuick` builds (confirmed present at `qt-app/build/quick/WITSQuick.exe`
  in all three build-matrix configurations).
- `tst_appshell` loads `AppShell` via `loadFromModule("LOAMS", "AppShell")` —
  the same production entry path `quick/main.cpp` uses — and asserts zero
  captured QML warnings. Passed in all three configurations.
- `ThemeViewModel` maps the brand-engine's palette roles and emits change
  notifications; covered by `tst_themeviewmodel`. Passed in all three
  configurations.
- `Theme.qml` (the `pragma Singleton` QML wrapper backed by `ThemeViewModel`)
  resolves design tokens; covered by `tst_qml_theme`. Passed in all three
  configurations.
- All ten `L*` skeletons (`LBarChart`, `LButton`, `LCard`, `LDialog`,
  `LPageHeader`, `LSegmented`, `LSideNav`, `LStatTile`, `LTable`, `LToast`)
  instantiate and bind to `Theme`; covered by `tst_qml_components`. Passed in
  all three configurations.
- **No-stray-hex grep:** `grep -rEn "#[0-9A-Fa-f]{3,8}" qt-app/quick/qml
  --include="*.qml"` excluding `theme/Theme.qml` returned **zero matches** —
  confirmed clean at proof-assembly time. All hex color literals live only in
  the `Theme.qml` singleton; the ten components reference tokens, not literals.

Confirms proposal §20 Phase 1 deliverable (b).

---

## Check 3 — RFID spike (Task 3, deliverable c)

`tst_rfidquickfilter` proves that `RfidQuickFilter` reuses `RfidScanDetector`
**verbatim** (moved, not rewritten — the class file is byte-for-byte the same
logic as the legacy `RfidKeyboardFilter` depends on) and correctly consumes the
scan terminator under a re-authored gate driven by `QQuickWindow` active-focus
state (rather than the legacy `QWidget` event filter). This test runs under
offscreen QPA and passed in all three build-matrix configurations.

**Deliberate non-fix, called out explicitly:** `RfidQuickFilter` does **not**
call `RfidScanDetector::reset()`. This matches legacy parity — the original
`RfidKeyboardFilter` doesn't call it either; `RfidScanDetector` self-recovers
via its own inter-key-gap timeout. Confirmed by direct inspection of
`qt-app/quick/RfidQuickFilter.cpp`/`.h`: no `reset()` call site exists. This is
intentional, not an oversight — do not "fix" this with a spurious `reset()`
call in Phase 2 or later without first re-deriving why the legacy code doesn't
need one.

**What this retires, and what it doesn't:** This retires Risk 1 (R1,
RFID-under-QML) **at the wiring level** — the detector logic and the
QML-window gate-open seam are proven under automated test with a real
(offscreen) `QQuickWindow`. It does **not** retire R1 at the
observable-behavior level — i.e., an actual RFID reader emitting real
keystrokes into a real, focused, on-screen `WITSQuick` window on real hardware.
That observable-behavior sign-off is **deferred to the Phase 2 on-device kiosk
parity checklist**, where the RFID reader is physically present and the full
kiosk window (not an empty `AppShell`) is running.

Confirms proposal §20 Phase 1 deliverable (c) at the level Phase 1 is scoped to
prove.

---

## Check 4 — Render check (Task 4, deliverable d)

`WITSQuick` supports a software-rendering fallback: `--software` on the
command line, or `QT_QUICK_BACKEND=software` in the environment, forces the Qt
Quick scene graph onto the software rasterizer instead of the default
hardware/RHI backend. This build machine's own launches of both the default
and `--software` paths (build/link success only — see "Pending human gates"
below for the actual launch observation, which is a human gate not performed by
this task) are covered by the build matrix above; both `WITSQuick.exe` and its
`--software` code path compile identically in all three configurations.

The full on-device procedure — including recording the GPU/driver used, the
`QSG_INFO=1` backend line, and pass/fail for both the default and `--software`
launches — lives in a dedicated checklist:

**`docs/superpowers/proofs/2026-07-09-loams2-phase1-render-check.md`**

That checklist is currently **unfilled** (a template) — it is explicitly a
human, on-device gate that cannot be satisfied by a CI/build-machine run, and
it has not yet been executed on the real library deployment PC. Risk 4 (R4)
remains **open** until that checklist's sign-off section is completed by
someone running it on the actual deployment hardware (or a faithful GPU/driver
stand-in).

Confirms proposal §20 Phase 1 deliverable (d) at the level Phase 1 is scoped to
prove; the human on-device portion is explicitly deferred, not silently
skipped.

---

## Pending HUMAN gates (must clear before Phase 1 sign-off)

These three items are outside the scope of automated build/test verification
and are **not** satisfied by this proof document:

1. **Legacy `WITS.exe` visual kiosk smoke** after the `witscore` relink (Task
   1) — verify the app still launches and behaves correctly as a kiosk app.
   **Must be run against the worktree build** (`qt-app/build/WITS.exe` as
   built in this task, timestamped from this checkout), not a Qt-Creator build
   from the main WITS-main checkout — the project has multiple stale WITS.exe
   builds floating around this machine, and testing the wrong one would give a
   false signal.
2. **`WITSQuick.exe` manual visual walkthrough** (Task 7 scope, human-performed):
   confirm the empty `AppShell` window opens, no console QML warnings appear,
   and the `--software` path also launches correctly. Not performed by this
   task by design — this task is the mechanical/automated portion only.
3. **On-device render check** executed on the real library deployment PC (Task
   4 checklist, referenced above) — GPU/driver capture, default-backend launch
   result, `--software` launch result, and the decision on whether the
   deployment launcher needs to force `--software` by default.

---

## `/claude-review` verdict

**APPROVE — round 1 of 3.** External Claude, PHASE mode. No Critical, no
Important findings.

- One Low: `witsquickmodule`'s QML URI `LOAMS` does not match its
  `OUTPUT_DIRECTORY quick/` — a qmllint-tooling-only configure warning, no
  runtime effect. Deferred to Phase 2, with a suggested one-line fix
  (`OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LOAMS`).

## Final whole-branch review verdict

**Ready to merge: YES.** Fable 5, SDD final gate. No Critical, no Important
findings.

- Verified cross-cutting invariants held across the branch: the `witscore`
  boundary is respected by all consumers; both CMake workarounds are applied
  to exactly the 4 runtime-`loadFromModule` consumers; the token system is
  coherent with zero hex outside `Theme.qml`; no lost sources or double
  registrations; the security baseline is clean.
- All deferred Minor findings (see below) were triaged safe-to-defer to
  Phase 2.
- Note: the `ThemeViewModel::refresh()` palette re-sync (re-syncing
  `m_config.palette` to `BrandTheme::current()`, closing a stale-cache trap
  ahead of Phase 2's `BrandingController`) was landed as a follow-up fix after
  this review.

---

## Deferred Minor findings carried from per-task reviews

These were raised (and accepted as deferred, not blocking) during the
individual Task 2–6 fresh-context reviews. Carrying them here so the final
`/claude-review` pass can triage them together rather than losing track of
them across six separate task reports:

- **T2:** `tst_appshell`'s message-handler capture window is
  load-synchronous-only — will need widening once the shell grows actual
  content beyond an empty window. Also: a comment typo at
  `qt-app/quick/CMakeLists.txt:30`.
- **T3:** The RFID burst test has a wall-clock timing dependency (this is
  plan-mandated, not an oversight). Also: `gateOpen` null-check ordering is a
  style nit, not a correctness issue.
- **T4:** The render-check doc's `QSG_INFO` example uses Command-Prompt `set`
  syntax but the doc says "Command Prompt or PowerShell" — should add the
  PowerShell form (`$env:QSG_INFO="1"`) alongside it. Also: the
  `QT_QUICK_BACKEND` environment-variable check is redundant with `--software`
  (plan-mandated duplication, not a bug).
- **T5:** `tst_qml_theme.qml`'s `!==` color-inequality check is a near-no-op in
  QML (colors should be compared with `Qt.colorEqual`, not `!==`/`===`). Also:
  a stale "witsquick" comment at `qt-app/quick/tests/tst_qml_theme.cpp:14`.
  **Flagged as more than cosmetic:** `ThemeViewModel::refresh()` does not
  re-sync `m_config.palette` — this is a latent trap that will bite before the
  Phase 2 `BrandingController` wiring lands (a caller that calls `refresh()`
  expecting the config's palette field to reflect the just-applied brand
  palette will silently get a stale value). Suggested one-line fix:
  `m_config.palette = BrandTheme::current();` inside `refresh()`. Not applied
  in this task (out of scope — mechanical verification only) but flagged
  clearly for the `/claude-review` pass or a fast-follow fix before Phase 2
  begins.
- **T6:** `LBarChart`'s bar-height binding is fragile inside a `RowLayout` —
  should use `Layout.preferredHeight` instead of a raw `height` binding.
  `LToast.autoDismissMs` is currently bound to `motion.toastIn` (200ms, an
  *entrance* animation duration) as a stand-in — it needs its own
  `motion.toastHold` token once real auto-dismiss logic lands; using the entry
  token is a scaffolding placeholder, not the final design. `LTable`'s header
  `radius` rounds all four corners where only the top two should round (a
  visual nit for a future pass).

None of the above are Critical or Important by the per-task reviews' own
classification; they are carried forward for the final `/claude-review` pass
to confirm or resolve, per this task's scope (which does not fix findings,
only assembles evidence).

---

## Summary

| Deliverable | Status |
|---|---|
| (a) `witscore` extraction, 12/12 green, both apps build | **DONE** — verified above |
| (b) AppShell + Theme + ThemeViewModel + 10 `L*` components | **DONE** — verified above |
| (c) RFID spike (wiring-level) | **DONE** — wiring retired; observable-behavior deferred to Phase 2 |
| (d) Render check (dev-machine build/launch) | **DONE** (build); on-device execution **PENDING** (human gate) |
| Debug build, 0 new warnings | **DONE** |
| Release build, 0 new warnings | **DONE** |
| Rollback build (`BUILD_LEGACY_WIDGETS=OFF`) | **DONE** |
| Full ctest, 17/17 | **DONE** (all three configurations) |
| `/claude-review` | **APPROVE** (round 1/3; 1 deferred Low) |
| Final whole-branch review | **Ready to merge: YES** (Fable 5, SDD final gate) |
| Legacy `WITS.exe` visual kiosk smoke (worktree build) | **PENDING** — human gate |
| `WITSQuick.exe` manual visual walkthrough | **PENDING** — human gate |
| On-device render check | **PENDING** — human gate |
