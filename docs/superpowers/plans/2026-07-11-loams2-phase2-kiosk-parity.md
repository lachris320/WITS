# LOAMS 2.0 Phase 2 — Kiosk Surface Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Each `- [ ]` is ONE action (write a failing test / run it & watch it fail / minimal implementation / run it & watch it pass / commit). Show your work; do not batch unrelated edits into one commit. The executing engineer writes the application code — this markdown is the plan.

**Goal:** Land Phase 2 of the LOAMS 2.0 Qt Quick modernization (spec §7 Kiosk, §9 roadmap Phase 2): rebuild the kiosk surface as a Qt Quick screen at parity with the legacy Widgets kiosk (`mainwindow.*`) — student-ID / admin-key / RFID / guest login, live attendance feed with row-entry motion, the "now signed in" hero card, session stat tiles, and the idle empty-state — driven by a `KioskViewModel` over `witscore`, with the deferred Phase-1 review items folded in where the code is already being touched.

**Architecture:** Phase 2 is organized as **friction removal first, then the Kiosk subsystem** (owner decision 2026-07-11). Tasks 1–2 remove friction that would otherwise contaminate the Kiosk build (a `wits_add_qttest()` CMake helper, a clean `qmllint` baseline, written `quick/` conventions, the `ThemeViewModel` single-source-of-truth fix, and the theme tokens the kiosk screen needs but does not yet have). Tasks 3–10 build the kiosk: a C++ `Navigator` singleton + `AppShell` surface host, pure login/RFID logic in `witscore` (`LoginParser`), a shared `RfidEventFilterBase` (deduping the Quick + legacy filters), a `KioskViewModel` + `RecentLoginsModel`, the brand-panel and main-area QML, the guest flow, and final assembly + parity gate. **MVVM holds throughout: ViewModels are the only QML-facing C++; QML never calls a controller directly.**

**Tech Stack:** Qt 6.11.1 MinGW (`C:\Qt\6.11.1\mingw_64`) — components already found by the top-level `find_package`: `Widgets Network Charts Test UiTools PrintSupport Svg Qml Quick QuickControls2 QuickTest`; vendored QXlsx; C++17; CMake + Ninja + MinGW; Qt Test + Qt Quick Test via ctest.

## Global Constraints

- **Design is LOCKED** (spec §2's 8 decisions). Elaborate within; do not relitigate. The visual source of truth for this phase is `../Library System UI Modernization/Library Kiosk v2.dc.html` (normative tokens/layout/motion; light-theme only — dark is a Phase-5 item).
- **Parity, not pixel-cloning:** every legacy kiosk *behavior* (`qt-app/mainwindow.cpp`, `qt-app/guestwindow.cpp`) must be reproduced. Behaviors are enumerated in the Task 10 parity checklist. "RFID scan behaves exactly as today" (spec §7) is a hard requirement.
- **`-DBUILD_LEGACY_WIDGETS=ON` (default ON)** stays: the legacy `WITS` Widgets kiosk remains the rollback until Phase 6. Nothing in Phase 2 may break the `WITS` build or its 12 legacy ctest targets.
- **The QML module CMake target is `witsquickmodule`** (NOT `witsquick` — NTFS case-insensitive collision with the `WITSQuick` executable's autogen dir). Its externally-visible identity is `URI LOAMS`.
- **Runtime `loadFromModule` consumers must explicitly link `witsquickmoduleplugin` + `witsquickmoduleplugin_init`** (qmlimportscanner only fires on a literal `import LOAMS` in a scanned C++/QML source; `main.cpp` and `.qml` QuickTest data files are not scanned). Every executable/test that resolves the module at runtime links both.
- **`Theme.qml` is the single source of every visual token; ZERO raw hex outside `Theme.qml`.** New colors the kiosk needs are added to `Theme.qml` (Task 2), never inlined into a screen. Opacity variants use `Qt.alpha(Theme.<token>, a)` / `Qt.rgba(...)` over a token, never a literal `#RRGGBB`.
- **ViewModels are the only QML-facing C++.** Screens receive their ViewModel as a `property var vm` (injected by the parent), so QuickTests can substitute a plain-QML stub with the same property surface.
- **QtTest/Quick-test mechanics (this MinGW kit), now centralized in `wits_add_qttest()` (Task 1):** every test target is a console app (`WIN32_EXECUTABLE FALSE`, or ctest captures zero output) with `QT_FORCE_STDERR_LOGGING=1` in its env; GUI/Quick/offscreen-painting tests add `QT_QPA_PLATFORM=offscreen`.
- **No live network in unit tests** (house rule). Logic is isolated from `QNetworkAccessManager`: pure parsing/decision code lives in `LoginParser` (Task 4) and is fed synthetic `QByteArray` payloads; ViewModels expose a non-network "apply a parsed record" seam that tests drive directly.
- **No secrets / no PII / no personal paths** in source, tests, CMake, QML, or commit messages (security-hygiene rule). Synthetic data only (`"Maria Santos"`, `"BSCE"`, `"2023-00123"`, `"ABC1"`). Endpoints go through `ApiConfig::endpoint(...)`; the shipped `http://localhost/loams_api/` base is pre-existing and unchanged.
- **Stage files by name** — never `git add -A`/`.`/`-u`. Commit via the `commit` skill. Never commit `qt-app/build/`, `*.user`, or `moc_*`/`ui_*`. **NEVER stage `qt-app/adminwindow.ui`** — it carries the user's own uncommitted edit.
- Harmless warnings to ignore: `LF will be replaced by CRLF`; the pre-existing `QXlsx ... GuiPrivate target` warning.
- **Build/test commands (PowerShell — tools are NOT on PATH):**
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Windows path-length note: if the default build tree overflows `MAX_PATH` on the deep QML-module autogen paths, configure into a short build dir (e.g. `-B C:/b/loams-dbg`) and adjust the `--test-dir` accordingly.
- **Baseline suite = 17 ctest targets** (`100% tests passed, 0 tests failed out of 17`). Record it green before Task 1 and keep it green after every task; each new test target raises the count.

---

### Task 1: Remove build & convention friction (`wits_add_qttest`, qmllint, CLAUDE.md)

Three small, screen-agnostic changes that reduce friction for the whole Kiosk build before it starts: a CMake test helper (Phase 2 adds ~6 test targets — the helper deletes ~8 lines of boilerplate each and keeps the env flags uniform), a clean `qmllint` baseline (so the many new QML files' warnings stay visible instead of drowning in a pre-existing one), and a written `quick/` convention section in CLAUDE.md (Phase 2 dispatches many QML/VM-writing subagents; a codified convention prevents drift and re-review churn). No behavior change — the regression gate is the suite staying **17/17** and `qmllint` exiting clean.

**Files:**
- Create: `qt-app/cmake/WitsTest.cmake`
- Modify: `qt-app/CMakeLists.txt` (include the helper before test dirs), `qt-app/quick/CMakeLists.txt` (retrofit the 5 quick tests to the helper; add the qmllint `OUTPUT_DIRECTORY`), `CLAUDE.md`
- Leave as-is (explicitly deferred): the 12 legacy `qt-app/tests/CMakeLists.txt` targets — retrofitting stable Widgets-era tests is churn with no Kiosk-friction payoff and real regression risk; new tests use the helper, legacy ones keep their hand-rolled form.

**Interfaces:**
- Produces: CMake function `wits_add_qttest(<name> SOURCES <...> [LIBS <...>] [DEFINES <...>] [INCLUDES <...>] [OFFSCREEN])` — registers a console QtTest executable + `add_test` with the house env; `OFFSCREEN` adds `QT_QPA_PLATFORM=offscreen`.

Steps:

- [ ] **Record the baseline.** Configure + build + run and confirm the gate is real BEFORE touching anything:
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Expected: `100% tests passed, 0 tests failed out of 17`.

- [ ] **Write `qt-app/cmake/WitsTest.cmake`** — the helper:
  ```cmake
  # wits_add_qttest(<name>
  #     SOURCES  <src...>     # test .cpp plus any class-under-test .cpp/.h it compiles directly
  #     [LIBS    <lib...>]    # extra link libs beyond Qt::Test
  #     [DEFINES <def...>]    # target_compile_definitions PRIVATE
  #     [INCLUDES <dir...>]   # extra PRIVATE include dirs
  #     [OFFSCREEN])          # add QT_QPA_PLATFORM=offscreen to the ctest env
  #
  # Registers a console (WIN32_EXECUTABLE FALSE — required on this MinGW kit or
  # ctest captures zero output) QtTest executable and its ctest test with the
  # house-standard environment (QT_FORCE_STDERR_LOGGING=1, plus offscreen when
  # OFFSCREEN is given). Centralizes the boilerplate every Phase-1 test repeated.
  function(wits_add_qttest name)
      cmake_parse_arguments(T "OFFSCREEN" "" "SOURCES;LIBS;DEFINES;INCLUDES" ${ARGN})
      qt_add_executable(${name} ${T_SOURCES})
      set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
      target_link_libraries(${name} PRIVATE Qt${QT_VERSION_MAJOR}::Test ${T_LIBS})
      if(T_INCLUDES)
          target_include_directories(${name} PRIVATE ${T_INCLUDES})
      endif()
      if(T_DEFINES)
          target_compile_definitions(${name} PRIVATE ${T_DEFINES})
      endif()
      add_test(NAME ${name} COMMAND ${name})
      if(T_OFFSCREEN)
          set(_env "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")
      else()
          set(_env "QT_FORCE_STDERR_LOGGING=1")
      endif()
      set_tests_properties(${name} PROPERTIES ENVIRONMENT "${_env}")
  endfunction()
  ```

- [ ] **Include the helper from `qt-app/CMakeLists.txt`** — add one line immediately after the existing `enable_testing()` call (so both `quick/` and `tests/` can use it), leaving everything else untouched:
  ```cmake
  enable_testing()

  # Shared test-target helper (Phase 2): console QtTest exe + ctest reg + house env.
  include(${CMAKE_SOURCE_DIR}/cmake/WitsTest.cmake)
  ```

- [ ] **Add the qmllint `OUTPUT_DIRECTORY` fix** in `qt-app/quick/CMakeLists.txt`. Add the `OUTPUT_DIRECTORY` line to the existing `qt_add_qml_module(witsquickmodule ...)` call so the generated module lands under a `LOAMS/` subdir and `qmllint` resolves the `import LOAMS` cleanly (L1 deferred follow-up). Insert it right after `VERSION 1.0`:
  ```cmake
  qt_add_qml_module(witsquickmodule
      URI LOAMS
      VERSION 1.0
      OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LOAMS
      QML_FILES
          # ... (unchanged existing list) ...
      SOURCES
          # ... (unchanged existing list) ...
  )
  ```

- [ ] **Retrofit the 5 quick-side test targets in `qt-app/quick/CMakeLists.txt` to `wits_add_qttest`.** Replace the five hand-rolled blocks (`tst_appshell`, `tst_rfidquickfilter`, `tst_themeviewmodel`, `tst_qml_theme`, `tst_qml_components`) with:
  ```cmake
  # --- AppShell load test (C++ QtTest, offscreen) ---
  wits_add_qttest(tst_appshell
      SOURCES tests/tst_appshell.cpp
      LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
           Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
      OFFSCREEN)

  # --- RFID-in-QML spike test (C++ QtTest, offscreen) ---
  wits_add_qttest(tst_rfidquickfilter
      SOURCES tests/tst_rfidquickfilter.cpp
      LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Quick
      OFFSCREEN)

  # --- ThemeViewModel unit test (C++ QtTest, offscreen: paints QImages) ---
  wits_add_qttest(tst_themeviewmodel
      SOURCES tests/tst_themeviewmodel.cpp
      LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::Svg
      OFFSCREEN)

  # --- Theme singleton Quick test (offscreen) ---
  wits_add_qttest(tst_qml_theme
      SOURCES tests/tst_qml_theme.cpp
      LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
           Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
      DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
      OFFSCREEN)

  # --- L* component skeletons Quick test (offscreen) ---
  wits_add_qttest(tst_qml_components
      SOURCES tests/tst_qml_components.cpp
      LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
           Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
      DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
      OFFSCREEN)
  ```
  (The `Qt::Test` link the old blocks named is now supplied by the helper; QuickTest targets keep `Qt::QuickTest`.)

- [ ] **Add the `quick/` conventions section to `CLAUDE.md`.** Under "## Project layout", append a subsection codifying the Phase-1-established rules so Phase-2 subagents follow them without re-deriving:
  ```markdown
  ## qt-app/quick/ conventions (LOAMS 2.0)

  - **QML module target is `witsquickmodule`** (URI `LOAMS`), NOT `witsquick` — the
    latter collides on case-insensitive NTFS with the `WITSQuick` executable's
    autogen dir. Runtime `loadFromModule` consumers link `witsquickmoduleplugin`
    + `witsquickmoduleplugin_init`.
  - **File naming:** QML types and C++ ViewModel/model classes are `PascalCase`
    (`KioskViewModel.h`, `BrandPanel.qml`); C++ member variables are `m_camelCase`.
  - **MVVM:** ViewModels (`quick/viewmodels/`) are the ONLY QML-facing C++; QML
    never calls a `witscore` controller directly. Screens receive their VM as a
    `property var vm` so QuickTests can inject a plain-QML stub.
  - **Theming:** `Theme.qml` (pragma Singleton) is the single source of every
    visual token. ZERO raw hex outside `Theme.qml`; opacity variants use
    `Qt.alpha(Theme.<token>, a)`, never a literal color.
  - **Tests:** register via `wits_add_qttest()` (`qt-app/cmake/WitsTest.cmake`);
    add `OFFSCREEN` for any GUI/Quick/painting test.
  ```

- [ ] **Re-configure, build, and verify the suite + qmllint.**
  ```
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Expected: still `17 tests passed` (helper produces identical targets). Then run qmllint on the module and confirm it is clean:
  ```
  cmake --build qt-app/build --target all_qmllint
  ```
  Expected: no warnings referencing an unresolved `LOAMS` import. (If the `all_qmllint` target name differs in this Qt version, list targets with `cmake --build qt-app/build --target help | Select-String qmllint`.)

- [ ] **Commit** via the `commit` skill. Stage `qt-app/cmake/WitsTest.cmake`, `qt-app/CMakeLists.txt`, `qt-app/quick/CMakeLists.txt`, `CLAUDE.md`. Suggested message: `build: add wits_add_qttest helper, fix qmllint output dir, codify quick/ conventions`.

---

### Task 2: Remove theme-foundation friction (`ThemeViewModel` single source; kiosk tokens; `LToast` auto-dismiss)

The kiosk is the first heavy consumer of `Theme`, so it must not build on a shaky foundation. Two friction items: (1) `ThemeViewModel` currently caches `m_config.palette` in its ctor/`refresh` but the getters read `BrandTheme::current()` live — a dead dual source that can silently drift; collapse it to one source. (2) The brand panel needs tokens that don't exist yet (`motion.toastHold`, `scrim`, the warm on-brand neutrals) and `LToast.autoDismissMs` is wired to the wrong token (`toastIn`, an entrance duration) and never actually dismisses — fix both so the kiosk's status toasts and panel colors come from `Theme`, not screen-local literals.

**Files:**
- Modify: `qt-app/quick/viewmodels/ThemeViewModel.cpp` (drop the dead palette cache), `qt-app/quick/viewmodels/ThemeViewModel.h` (comment only — the getters are already live)
- Modify: `qt-app/quick/qml/theme/Theme.qml` (add tokens), `qt-app/quick/qml/components/LToast.qml` (real auto-dismiss)
- Modify: `qt-app/quick/tests/tst_themeviewmodel.cpp` (assert single-source: `setCurrent` is visible through the getters without `refresh`), `qt-app/quick/tests/tst_qml_theme.qml` (assert new tokens)

**Interfaces:**
- Produces (Theme.qml new tokens, consumed by Tasks 7–9): `Theme.motion.toastHold` (int ms), `Theme.scrim` (color), `Theme.brand.onKiosk` (color, from `_vm.kioskOnPrimary`), `Theme.onBrandMuted` (color).
- Produces (ThemeViewModel behavior): getters remain the single live source (`BrandTheme::current()`); `regenerateFromImportedLogo` seeds `m_config.palette` from the live palette immediately before calling the engine, so there is no stale cache to drift.

Steps:

- [ ] **Write the failing single-source assertion** in `qt-app/quick/tests/tst_themeviewmodel.cpp` — a new slot proving a `BrandTheme::setCurrent` is visible through the getter WITHOUT calling `refresh()` (i.e. the getter is live, not a cache). Add the declaration to the `private slots:` block and the definition:
  ```cpp
  // (add to private slots:)
  void getterIsLiveNotCached();
  ```
  ```cpp
  void TestThemeViewModel::getterIsLiveNotCached()
  {
      BrandTheme::setCurrent(BrandTheme::fallbackPalette());
      ThemeViewModel vm;

      BrandPalette custom = BrandTheme::fallbackPalette();
      custom.adminPrimary = QColor(0x0A, 0x0B, 0x0C);
      BrandTheme::setCurrent(custom);   // change the engine, do NOT call vm.refresh()

      // Single source of truth: the getter reflects the engine immediately.
      QCOMPARE(vm.adminPrimary(), QColor(0x0A, 0x0B, 0x0C));
  }
  ```

- [ ] **Run it — expect PASS already** (the getters are already live: `adminPrimary() { return BrandTheme::current().adminPrimary; }`). This test is a *regression lock*, not a red step — it must keep passing after the cache removal:
  ```
  cmake --build qt-app/build --target tst_themeviewmodel
  ctest --test-dir qt-app/build -R tst_themeviewmodel --output-on-failure
  ```
  Expected: PASS. (If it somehow fails, the getters were changed to read the cache — do not do that.)

- [ ] **Remove the dead palette cache in `qt-app/quick/viewmodels/ThemeViewModel.cpp`.** The ctor and `refresh()` assign `m_config.palette = BrandTheme::current()`, but no getter reads `m_config.palette` — it is write-only state that can drift from the engine. Drop those assignments; keep `m_config` solely as the scratch record `regenerateFromImportedLogo` hands to the engine, seeding it from the live palette right before use:
  ```cpp
  #include "ThemeViewModel.h"
  #include "brandtheme.h"

  ThemeViewModel::ThemeViewModel(QObject *parent)
      : QObject(parent)
  {
      // Single source of truth for colors is BrandTheme::current(); every getter
      // reads it live. No palette cache here — a cached copy would only drift.
      // m_config is scratch for regenerateFromImportedLogo (below).
  }

  void ThemeViewModel::refresh()
  {
      // Re-notify QML after an external BrandTheme::setCurrent. Nothing to cache —
      // the getters already read the engine live; this just fires the binding.
      emit changed();
  }

  bool ThemeViewModel::regenerateFromImportedLogo(const QString &path)
  {
      // Seed the scratch config from the live palette so regeneration starts from
      // whatever is current, then let the engine overwrite it.
      m_config.palette = BrandTheme::current();
      QString err;
      const bool ok = BrandTheme::regenerateFromLogo(m_config, path, &err);
      if (ok) {
          BrandTheme::setCurrent(m_config.palette);
          emit changed();
      }
      return ok;
  }
  ```
  Update the `m_config` comment in `ThemeViewModel.h` from "// scratch" intent — change the trailing comment on `BrandingConfig m_config;` to: `// scratch for regenerateFromImportedLogo only; NOT a palette cache`.

- [ ] **Run the full ThemeViewModel test** — all four slots must pass (the three originals + the new live-source lock):
  ```
  cmake --build qt-app/build --target tst_themeviewmodel
  ctest --test-dir qt-app/build -R tst_themeviewmodel --output-on-failure
  ```
  Expected: `Totals: 4 passed`.

- [ ] **Add the kiosk tokens to `qt-app/quick/qml/theme/Theme.qml`.** Three edits:
  1. In the `brand` `QtObject`, add the kiosk on-color (the engine already exposes it via `_vm.kioskOnPrimary`) after `onPrimary`:
     ```qml
             readonly property color onKiosk:      root._vm.kioskOnPrimary
     ```
  2. After the existing extra-token literals (`errorBorder`), add the scrim and warm muted-on-brand neutral. `scrim` matches the legacy poster overlay `QColor(15,23,42,115)` (≈0.45 alpha); `onBrandMuted` is the design's warm secondary panel text `#EFC9A8` (a design-fixed warm neutral, §12.1 — no BrandPalette field):
     ```qml
     readonly property color scrim:            Qt.rgba(15/255, 23/255, 42/255, 0.45)
     readonly property color onBrandMuted:     "#EFC9A8"
     ```
  3. In the `motion` `QtObject`, add the hold duration (how long a toast stays before auto-dismissing) after `toastOut`. `2500` matches the legacy `showKioskStatus` `QTimer::singleShot(2500, ...)`:
     ```qml
             readonly property int toastHold: 2500
     ```

- [ ] **Wire real auto-dismiss into `qt-app/quick/qml/components/LToast.qml`.** Point `autoDismissMs` at the new hold token and add the `Timer` that actually clears the message (currently the property is declared but nothing consumes it). Replace the property line and add the timer + a signal:
  ```qml
  import QtQuick
  import LOAMS

  // Inline, non-blocking status surface (§11). Assertive live-region semantics.
  Rectangle {
      id: toast
      property string message: ""
      property string severity: "Info"   // Info | Success | Warning | Error
      property int autoDismissMs: Theme.motion.toastHold
      signal dismissed()

      visible: message.length > 0
      color: severity === "Error" ? Theme.errorSoft : Theme.card
      radius: Theme.radius.md
      border.width: 2
      border.color: severity === "Error" ? Theme.errorBorder : Theme.border
      implicitHeight: 40
      implicitWidth: contentText.implicitWidth + Theme.spacing.xxl

      // Auto-dismiss: (re)start whenever a non-empty message is shown.
      onMessageChanged: if (message.length > 0 && autoDismissMs > 0) dismissTimer.restart()
      Timer {
          id: dismissTimer
          interval: toast.autoDismissMs
          repeat: false
          onTriggered: { toast.message = ""; toast.dismissed() }
      }

      Text {
          id: contentText
          anchors.centerIn: parent
          text: toast.message
          color: toast.severity === "Success" ? Theme.success
               : toast.severity === "Error" ? Theme.error : Theme.text
          font.family: Theme.typography.sans
          font.pixelSize: Theme.typography.body
      }

      Accessible.role: Accessible.AlertMessage
      Accessible.name: toast.message
  }
  ```

- [ ] **Assert the new tokens in `qt-app/quick/tests/tst_qml_theme.qml`.** Add a test function proving they resolve (guards against a typo or missing property):
  ```qml
      function test_kioskTokensExposed() {
          verify(Theme.motion.toastHold >= 1000);
          verify(Theme.brand.onKiosk.a === 1.0);
          compare(Theme.onBrandMuted.toString().length, 7);   // "#RRGGBB"
          verify(Theme.scrim.a > 0 && Theme.scrim.a < 1);
      }
  ```

- [ ] **Build and run the theme + component tests green.**
  ```
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure -R "tst_themeviewmodel|tst_qml_theme|tst_qml_components"
  ```
  Expected: all pass. Then the full suite — still `17 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/viewmodels/ThemeViewModel.cpp`, `qt-app/quick/viewmodels/ThemeViewModel.h`, `qt-app/quick/qml/theme/Theme.qml`, `qt-app/quick/qml/components/LToast.qml`, `qt-app/quick/tests/tst_themeviewmodel.cpp`, `qt-app/quick/tests/tst_qml_theme.qml`. Suggested message: `refactor(theme): single-source ThemeViewModel, add kiosk tokens, wire LToast auto-dismiss`.

---

### Task 3: `Navigator` singleton + `AppShell` surface host

Turn `AppShell.qml` from an empty window into a surface host driven by a C++ `Navigator` singleton (spec §4: "A C++ Navigator singleton owns surface/page/modal state so flow is testable and keyboard shortcuts are centralized"). Phase 2 needs two surfaces: `Kiosk` (default) and `Admin` (a placeholder this phase; built out in Phases 3–4). The kiosk's admin-key login switches to the Admin surface via `Navigator.showAdmin()`.

**Files:**
- Create: `qt-app/quick/viewmodels/Navigator.h`, `qt-app/quick/viewmodels/Navigator.cpp`
- Create: `qt-app/quick/qml/kiosk/KioskScreen.qml` (placeholder this task; fleshed out Task 10), `qt-app/quick/qml/admin/AdminScreen.qml` (placeholder for the whole phase)
- Create: `qt-app/quick/tests/tst_navigator.cpp`
- Modify: `qt-app/quick/qml/AppShell.qml` (host the surfaces), `qt-app/quick/CMakeLists.txt` (add Navigator to module SOURCES, the two new QML files, and register `tst_navigator`)

**Interfaces:**
- Produces: `class Navigator : public QObject` — singleton (`QML_ELEMENT` + `QML_SINGLETON`); `enum Surface { Kiosk, Admin }` (`Q_ENUM`); `Q_PROPERTY(Surface currentSurface READ currentSurface NOTIFY currentSurfaceChanged)`; `Q_INVOKABLE void showKiosk()`, `Q_INVOKABLE void showAdmin()`. Default surface is `Kiosk`.
- Consumes: nothing new.

Steps:

- [ ] **Write the failing test `qt-app/quick/tests/tst_navigator.cpp`** (red — `Navigator` does not exist):
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include "Navigator.h"

  class TestNavigator : public QObject
  {
      Q_OBJECT
  private slots:
      void defaultsToKiosk();
      void showAdminSwitchesAndSignals();
      void showKioskSwitchesBack();
  };

  void TestNavigator::defaultsToKiosk()
  {
      Navigator nav;
      QCOMPARE(nav.currentSurface(), Navigator::Kiosk);
  }

  void TestNavigator::showAdminSwitchesAndSignals()
  {
      Navigator nav;
      QSignalSpy spy(&nav, &Navigator::currentSurfaceChanged);
      nav.showAdmin();
      QCOMPARE(nav.currentSurface(), Navigator::Admin);
      QCOMPARE(spy.count(), 1);
      nav.showAdmin();                 // idempotent — no redundant signal
      QCOMPARE(spy.count(), 1);
  }

  void TestNavigator::showKioskSwitchesBack()
  {
      Navigator nav;
      nav.showAdmin();
      nav.showKiosk();
      QCOMPARE(nav.currentSurface(), Navigator::Kiosk);
  }

  QTEST_MAIN(TestNavigator)
  #include "tst_navigator.moc"
  ```

- [ ] **Write `qt-app/quick/viewmodels/Navigator.h`:**
  ```cpp
  #ifndef NAVIGATOR_H
  #define NAVIGATOR_H

  #include <QObject>
  #include <qqml.h>

  // Owns which top-level surface is showing. A C++ singleton so surface flow is
  // unit-testable and (later) keyboard shortcuts route through one place.
  class Navigator : public QObject
  {
      Q_OBJECT
      QML_ELEMENT
      QML_SINGLETON
      Q_PROPERTY(Surface currentSurface READ currentSurface NOTIFY currentSurfaceChanged)
  public:
      enum Surface { Kiosk, Admin };
      Q_ENUM(Surface)

      explicit Navigator(QObject *parent = nullptr);

      Surface currentSurface() const { return m_surface; }

  public slots:
      void showKiosk();
      void showAdmin();

  signals:
      void currentSurfaceChanged();

  private:
      void setSurface(Surface s);
      Surface m_surface = Kiosk;
  };

  #endif // NAVIGATOR_H
  ```

- [ ] **Write `qt-app/quick/viewmodels/Navigator.cpp`:**
  ```cpp
  #include "Navigator.h"

  Navigator::Navigator(QObject *parent)
      : QObject(parent)
  {
  }

  void Navigator::setSurface(Surface s)
  {
      if (m_surface == s)
          return;                      // idempotent: no redundant re-theme/re-layout
      m_surface = s;
      emit currentSurfaceChanged();
  }

  void Navigator::showKiosk() { setSurface(Kiosk); }
  void Navigator::showAdmin() { setSurface(Admin); }
  ```

- [ ] **Write the placeholder `qt-app/quick/qml/kiosk/KioskScreen.qml`** (real layout lands in Task 10; for now a themed stub so `AppShell` has something to host):
  ```qml
  import QtQuick
  import LOAMS

  Rectangle {
      id: kiosk
      color: Theme.appBackground
      Text {
          anchors.centerIn: parent
          text: qsTr("Kiosk")
          color: Theme.text
          font.family: Theme.typography.serif
          font.pixelSize: Theme.typography.pageTitle
      }
  }
  ```

- [ ] **Write the placeholder `qt-app/quick/qml/admin/AdminScreen.qml`** (Admin is Phases 3–4; this is the surface stub `Navigator.showAdmin()` reveals):
  ```qml
  import QtQuick
  import LOAMS

  Rectangle {
      id: admin
      color: Theme.appBackground
      Text {
          anchors.centerIn: parent
          text: qsTr("Admin (coming in Phase 3)")
          color: Theme.mutedText
          font.family: Theme.typography.sans
          font.pixelSize: Theme.typography.cardTitle
      }
  }
  ```

- [ ] **Rewrite `qt-app/quick/qml/AppShell.qml`** to host the surfaces via the `Navigator` singleton. A `Loader` swaps the active surface; the window is full-screen for deployment (a dev override note is in Task 10):
  ```qml
  import QtQuick
  import QtQuick.Controls
  import LOAMS

  ApplicationWindow {
      id: appShell
      width: 1280
      height: 800
      visible: true
      title: qsTr("LOAMS 2.0")
      color: Theme.appBackground

      Loader {
          id: surface
          anchors.fill: parent
          sourceComponent: Navigator.currentSurface === Navigator.Admin
                           ? adminSurface : kioskSurface
      }

      Component { id: kioskSurface; KioskScreen {} }
      Component { id: adminSurface; AdminScreen {} }
  }
  ```

- [ ] **Wire the new sources into `qt-app/quick/CMakeLists.txt`.** Add the two QML files to the `QML_FILES` list and `Navigator.h`/`.cpp` to the module `SOURCES`:
  ```cmake
      QML_FILES
          qml/AppShell.qml
          qml/theme/Theme.qml
          qml/kiosk/KioskScreen.qml
          qml/admin/AdminScreen.qml
          qml/components/LButton.qml
          # ... (rest of existing components unchanged) ...
      SOURCES
          RfidQuickFilter.h RfidQuickFilter.cpp
          viewmodels/ThemeViewModel.h viewmodels/ThemeViewModel.cpp
          viewmodels/Navigator.h viewmodels/Navigator.cpp
  ```
  Then register the test with the helper (append near the other quick tests):
  ```cmake
  # --- Navigator unit test (C++ QtTest) ---
  wits_add_qttest(tst_navigator
      SOURCES tests/tst_navigator.cpp
      LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Qml)
  ```
  Note: `tst_navigator` quote-includes `"Navigator.h"` from `viewmodels/`; that dir is already on `witsquickmodule`'s PUBLIC include path (existing `target_include_directories(... ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels)`), so linking `witsquickmodule` resolves it.

- [ ] **Build and run red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R tst_navigator --output-on-failure
  ```
  Expected: PASS (3 slots). Then the full suite — `18 tests passed`. Then confirm `tst_appshell` still passes (AppShell now loads `KioskScreen`, which imports Theme — the load test still asserts one root object, zero QML warnings).

- [ ] **Manual smoke test:** run `qt-app/build/quick/WITSQuick.exe` — the window now shows the themed "Kiosk" placeholder on `Theme.appBackground`. Close it.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/viewmodels/Navigator.h`, `qt-app/quick/viewmodels/Navigator.cpp`, `qt-app/quick/qml/AppShell.qml`, `qt-app/quick/qml/kiosk/KioskScreen.qml`, `qt-app/quick/qml/admin/AdminScreen.qml`, `qt-app/quick/tests/tst_navigator.cpp`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat(quick): add Navigator singleton and AppShell surface host`.

---

### Task 4: Kiosk login logic — pure `LoginParser` in `witscore`

Extract the kiosk's decision + response-decode logic (currently inline in `mainwindow.cpp`'s `handleLogin`/`handleRfidLogin`) into a pure, widget-free, network-free `LoginParser` namespace in `witscore`, unit-tested against synthetic payloads. This is the house rule in action (§8: "isolate logic from `QNetworkAccessManager`") and the home for the deferred RFID **charset validation** follow-up.

**Files:**
- Create: `qt-app/core/loginparser.h`, `qt-app/core/loginparser.cpp`
- Create: `qt-app/tests/tst_loginparser.cpp`
- Modify: `qt-app/core/CMakeLists.txt` (add the two files to `witscore`), `qt-app/tests/CMakeLists.txt` (register `tst_loginparser`)

**Interfaces:**
- Produces (consumed by `KioskViewModel`, Task 6):
  - `enum class LoginParser::LoginKind { StudentId, AdminKey }`
  - `LoginKind LoginParser::classify(const QString &input)` — mirrors `mainwindow.cpp:193` (`input.toLongLong(&ok)`; pure digits → `StudentId`, else → `AdminKey`).
  - `struct LoginParser::LoginResult { bool ok; bool isStudent; bool isAdmin; QJsonObject student; QString message; }`
  - `LoginResult LoginParser::parseLoginResponse(const QByteArray &json)` — mirrors `mainwindow.cpp:215-233`.
  - `struct LoginParser::RfidResult { bool ok; QJsonObject student; QString message; }`
  - `RfidResult LoginParser::parseRfidResponse(const QByteArray &json)` — mirrors `mainwindow.cpp:329-340`.
  - `bool LoginParser::isValidRfidCode(const QString &code)` — charset/length gate before a backend POST (deferred follow-up): non-empty, length in `[3, 64]` (matching `RfidScanDetector`'s bounds), ASCII alphanumeric only.
  - `bool LoginParser::shouldDebounceRfid(const QString &lastCode, qint64 lastMs, const QString &code, qint64 nowMs, qint64 windowMs = 2500)` — mirrors `mainwindow.cpp:301-303` (same code re-seen inside the window → true = ignore).
- Consumes: `QJsonDocument`/`QJsonObject` (Qt::Core).

Steps:

- [ ] **Write the failing test `qt-app/tests/tst_loginparser.cpp`** (red — `LoginParser` does not exist). Covers each function including the legacy `toLongLong` classification quirk (a dashed student number is non-numeric → treated as an admin key, exactly as the legacy kiosk does — parity preserves the quirk):
  ```cpp
  #include <QtTest>
  #include <QJsonObject>
  #include "loginparser.h"

  class TestLoginParser : public QObject
  {
      Q_OBJECT
  private slots:
      void classifyPureDigitsIsStudent();
      void classifyNonNumericIsAdmin();
      void classifyDashedIdIsAdmin_legacyQuirk();
      void parseStudentSuccess();
      void parseAdminSuccess();
      void parseFailureCarriesMessage();
      void parseInvalidJsonIsNotOk();
      void parseRfidStudentSuccess();
      void parseRfidUnregistered();
      void validRfidCodeAcceptsAlnum();
      void invalidRfidCodeRejectsBounds();
      void debounceIgnoresSameCodeInWindow();
      void debounceAllowsDifferentCode();
      void debounceAllowsSameCodeAfterWindow();
  };

  void TestLoginParser::classifyPureDigitsIsStudent()
  {
      QCOMPARE(LoginParser::classify("202300123"), LoginParser::LoginKind::StudentId);
  }
  void TestLoginParser::classifyNonNumericIsAdmin()
  {
      QCOMPARE(LoginParser::classify("letmein"), LoginParser::LoginKind::AdminKey);
  }
  void TestLoginParser::classifyDashedIdIsAdmin_legacyQuirk()
  {
      // "2023-00123".toLongLong() fails (the dash) -> legacy treats it as an
      // admin key. Parity: reproduce exactly, do not "fix" it here.
      QCOMPARE(LoginParser::classify("2023-00123"), LoginParser::LoginKind::AdminKey);
  }

  void TestLoginParser::parseStudentSuccess()
  {
      const QByteArray json =
          R"({"status":"success","student":{"name":"Maria Santos","course":"BSCE"}})";
      LoginParser::LoginResult r = LoginParser::parseLoginResponse(json);
      QVERIFY(r.ok);
      QVERIFY(r.isStudent);
      QVERIFY(!r.isAdmin);
      QCOMPARE(r.student.value("name").toString(), QStringLiteral("Maria Santos"));
  }
  void TestLoginParser::parseAdminSuccess()
  {
      // success with no "student" object -> admin (legacy: else adminWin->show()).
      LoginParser::LoginResult r = LoginParser::parseLoginResponse(R"({"status":"success"})");
      QVERIFY(r.ok);
      QVERIFY(!r.isStudent);
      QVERIFY(r.isAdmin);
  }
  void TestLoginParser::parseFailureCarriesMessage()
  {
      LoginParser::LoginResult r =
          LoginParser::parseLoginResponse(R"({"status":"error","message":"Bad key"})");
      QVERIFY(!r.ok);
      QCOMPARE(r.message, QStringLiteral("Bad key"));
  }
  void TestLoginParser::parseInvalidJsonIsNotOk()
  {
      LoginParser::LoginResult r = LoginParser::parseLoginResponse("<html>nope");
      QVERIFY(!r.ok);
      QVERIFY(!r.message.isEmpty());   // a user-facing "invalid response" string
  }

  void TestLoginParser::parseRfidStudentSuccess()
  {
      const QByteArray json =
          R"({"status":"success","student":{"name":"Jose Ramirez"}})";
      LoginParser::RfidResult r = LoginParser::parseRfidResponse(json);
      QVERIFY(r.ok);
      QCOMPARE(r.student.value("name").toString(), QStringLiteral("Jose Ramirez"));
  }
  void TestLoginParser::parseRfidUnregistered()
  {
      LoginParser::RfidResult r = LoginParser::parseRfidResponse(R"({"status":"error"})");
      QVERIFY(!r.ok);
  }

  void TestLoginParser::validRfidCodeAcceptsAlnum()
  {
      QVERIFY(LoginParser::isValidRfidCode("ABC1"));
      QVERIFY(LoginParser::isValidRfidCode("0004829173"));
  }
  void TestLoginParser::invalidRfidCodeRejectsBounds()
  {
      QVERIFY(!LoginParser::isValidRfidCode(""));          // empty
      QVERIFY(!LoginParser::isValidRfidCode("AB"));        // < 3
      QVERIFY(!LoginParser::isValidRfidCode(QString(65, 'A')));  // > 64
      QVERIFY(!LoginParser::isValidRfidCode("AB C1"));     // space
      QVERIFY(!LoginParser::isValidRfidCode("AB;C1"));     // punctuation
  }

  void TestLoginParser::debounceIgnoresSameCodeInWindow()
  {
      QVERIFY(LoginParser::shouldDebounceRfid("ABC1", 1000, "ABC1", 2000, 2500));
  }
  void TestLoginParser::debounceAllowsDifferentCode()
  {
      QVERIFY(!LoginParser::shouldDebounceRfid("ABC1", 1000, "XYZ9", 2000, 2500));
  }
  void TestLoginParser::debounceAllowsSameCodeAfterWindow()
  {
      QVERIFY(!LoginParser::shouldDebounceRfid("ABC1", 1000, "ABC1", 4000, 2500));
  }

  QTEST_MAIN(TestLoginParser)
  #include "tst_loginparser.moc"
  ```

- [ ] **Write `qt-app/core/loginparser.h`:**
  ```cpp
  #ifndef LOGINPARSER_H
  #define LOGINPARSER_H

  #include <QByteArray>
  #include <QJsonObject>
  #include <QString>

  // Pure, widget-free, network-free decode + decision logic for the kiosk login
  // flows (student / admin / RFID / debounce). Extracted from mainwindow.cpp so
  // it is unit-testable against synthetic payloads with no QNetworkAccessManager.
  namespace LoginParser {

  enum class LoginKind { StudentId, AdminKey };

  struct LoginResult {
      bool ok = false;          // status == "success"
      bool isStudent = false;   // success AND a "student" object is present
      bool isAdmin = false;     // success AND no "student" object
      QJsonObject student;
      QString message;          // user-facing failure/invalid message when !ok
  };

  struct RfidResult {
      bool ok = false;          // success AND a "student" object is present
      QJsonObject student;
      QString message;
  };

  // Numeric (QString::toLongLong succeeds) -> StudentId, else AdminKey.
  // Mirrors mainwindow.cpp:193 exactly, including the dashed-ID quirk.
  LoginKind classify(const QString &input);

  LoginResult parseLoginResponse(const QByteArray &json);
  RfidResult  parseRfidResponse(const QByteArray &json);

  // Charset/length gate before POSTing a scanned code to rfid_login.php:
  // non-empty, 3..64 chars, ASCII alphanumeric only.
  bool isValidRfidCode(const QString &code);

  // Same code re-seen within windowMs -> true (ignore this scan). One tap = one
  // visit. Mirrors mainwindow.cpp:301-303.
  bool shouldDebounceRfid(const QString &lastCode, qint64 lastMs,
                          const QString &code, qint64 nowMs,
                          qint64 windowMs = 2500);

  } // namespace LoginParser

  #endif // LOGINPARSER_H
  ```

- [ ] **Write `qt-app/core/loginparser.cpp`:**
  ```cpp
  #include "loginparser.h"

  #include <QJsonDocument>

  namespace LoginParser {

  LoginKind classify(const QString &input)
  {
      bool ok = false;
      input.toLongLong(&ok);            // same numeric test the legacy kiosk uses
      return ok ? LoginKind::StudentId : LoginKind::AdminKey;
  }

  LoginResult parseLoginResponse(const QByteArray &json)
  {
      LoginResult r;
      const QJsonDocument doc = QJsonDocument::fromJson(json);
      if (!doc.isObject()) {
          r.message = QStringLiteral("Invalid server response.");
          return r;
      }
      const QJsonObject obj = doc.object();
      if (obj.value("status").toString() == QLatin1String("success")) {
          r.ok = true;
          if (obj.contains("student")) {
              r.isStudent = true;
              r.student = obj.value("student").toObject();
          } else {
              r.isAdmin = true;
          }
          return r;
      }
      r.message = obj.value("message").toString();
      if (r.message.isEmpty())
          r.message = QStringLiteral("Login failed. Please check your ID or Admin Key.");
      return r;
  }

  RfidResult parseRfidResponse(const QByteArray &json)
  {
      RfidResult r;
      const QJsonDocument doc = QJsonDocument::fromJson(json);
      if (!doc.isObject()) {
          r.message = QStringLiteral("Invalid server response.");
          return r;
      }
      const QJsonObject obj = doc.object();
      if (obj.value("status").toString() == QLatin1String("success")
          && obj.contains("student")) {
          r.ok = true;
          r.student = obj.value("student").toObject();
          return r;
      }
      r.message = QStringLiteral("Card not registered. Please see the librarian.");
      return r;
  }

  bool isValidRfidCode(const QString &code)
  {
      if (code.size() < 3 || code.size() > 64)
          return false;
      for (const QChar c : code) {
          if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9'))
             || (c >= QLatin1Char('A') && c <= QLatin1Char('Z'))
             || (c >= QLatin1Char('a') && c <= QLatin1Char('z'))))
              return false;
      }
      return true;
  }

  bool shouldDebounceRfid(const QString &lastCode, qint64 lastMs,
                          const QString &code, qint64 nowMs, qint64 windowMs)
  {
      return code == lastCode && (nowMs - lastMs) < windowMs;
  }

  } // namespace LoginParser
  ```

- [ ] **Add `loginparser` to `witscore` in `qt-app/core/CMakeLists.txt`.** Add the two files to the `add_library(witscore STATIC ...)` source list (alongside the other pure logic, e.g. after `rfidscandetector.h rfidscandetector.cpp`):
  ```cmake
      rfidscandetector.h rfidscandetector.cpp
      loginparser.h loginparser.cpp
  ```

- [ ] **Register `tst_loginparser` in `qt-app/tests/CMakeLists.txt`** using the helper. It compiles `loginparser.cpp` directly (same lean pattern as the other pure-logic tests — no `witscore` link) and needs only `Qt::Test` (`QJson*` is in Qt::Core, pulled transitively):
  ```cmake
  wits_add_qttest(tst_loginparser
      SOURCES
          tst_loginparser.cpp
          ${CMAKE_SOURCE_DIR}/core/loginparser.cpp
          ${CMAKE_SOURCE_DIR}/core/loginparser.h
      INCLUDES ${CMAKE_SOURCE_DIR}/core)
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R tst_loginparser --output-on-failure
  ```
  Expected: all 14 slots pass. Full suite — `19 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/core/loginparser.h`, `qt-app/core/loginparser.cpp`, `qt-app/tests/tst_loginparser.cpp`, `qt-app/core/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`. Suggested message: `feat(core): add pure LoginParser (classify, response decode, RFID validation + debounce)`.

---

### Task 5: `RfidEventFilterBase` — dedupe the Quick + legacy RFID filters

Extract the shared key-burst filter body (currently duplicated between `RfidQuickFilter` and legacy `RfidKeyboardFilter`) into a widget-free `RfidEventFilterBase` in `witscore`, with the window-active gate as a pure-virtual seam each subclass overrides (DRY Important, deferred from Phase 1). Fold in the two RFID plumbing follow-ups while the code is open: `QPointer<QQuickWindow>` for dangling safety in `RfidQuickFilter`. The legacy filter's ancestor-dedup (its `focusWidget()`/`activeWindow()` logic) moves *into* its `gateOpen()` override, so the shared body is identical for both and **both existing RFID tests keep passing unchanged**.

**Files:**
- Create: `qt-app/core/rfideventfilterbase.h`, `qt-app/core/rfideventfilterbase.cpp`
- Modify: `qt-app/quick/RfidQuickFilter.h`, `qt-app/quick/RfidQuickFilter.cpp` (derive from base; `QPointer`), `qt-app/rfidkeyboardfilter.h`, `qt-app/rfidkeyboardfilter.cpp` (derive from base; fold dedup into `gateOpen`)
- Modify: `qt-app/core/CMakeLists.txt` (add base to `witscore`), `qt-app/tests/CMakeLists.txt` (`tst_rfidkeyboardfilter` compiles the base .cpp directly)
- Tests reused as the regression gate (unchanged assertions): `qt-app/quick/tests/tst_rfidquickfilter.cpp`, `qt-app/tests/tst_rfidkeyboardfilter.cpp`

**Interfaces:**
- Produces: `class RfidEventFilterBase : public QObject` — `static bool isTerminator(int key)`; `signals: void rfidScanned(const QString &code)`; `protected: bool eventFilter(QObject*, QEvent*) override` (the shared body); `protected: virtual bool gateOpen(QObject *watched) const = 0`.
- Consumes: `RfidScanDetector` (core), `QKeyEvent` (Qt::Gui — already a `witscore` PUBLIC link).

Steps:

- [ ] **Write `qt-app/core/rfideventfilterbase.h`:**
  ```cpp
  #ifndef RFIDEVENTFILTERBASE_H
  #define RFIDEVENTFILTERBASE_H

  #include <QObject>
  #include <QElapsedTimer>
  #include "rfidscandetector.h"

  // Shared, widget-free key-burst filter. Feeds printable keys (and a normalized
  // terminator) to an RfidScanDetector while gateOpen(watched) is true; on a
  // recognized scan it consumes the terminating Enter and emits the code. The
  // window-type-specific "is my login window active?" test is the only thing that
  // differs between the Widgets and Quick installs, so it is the pure-virtual seam.
  class RfidEventFilterBase : public QObject
  {
      Q_OBJECT
  public:
      explicit RfidEventFilterBase(QObject *parent = nullptr);

      static bool isTerminator(int key);

  signals:
      void rfidScanned(const QString &code);

  protected:
      bool eventFilter(QObject *watched, QEvent *event) override;

      // True when a key delivered to `watched` should be fed to the detector.
      // Subclasses encode their window-active (and, for Widgets, ancestor-dedup)
      // rules here.
      virtual bool gateOpen(QObject *watched) const = 0;

  private:
      RfidScanDetector m_detector;
      QElapsedTimer m_clock;
  };

  #endif // RFIDEVENTFILTERBASE_H
  ```

- [ ] **Write `qt-app/core/rfideventfilterbase.cpp`** — the single shared body (byte-for-byte the current `RfidQuickFilter::eventFilter` logic, now gated through the virtual):
  ```cpp
  #include "rfideventfilterbase.h"

  #include <QKeyEvent>

  RfidEventFilterBase::RfidEventFilterBase(QObject *parent)
      : QObject(parent)
  {
      m_clock.start();
  }

  bool RfidEventFilterBase::isTerminator(int key)
  {
      return key == Qt::Key_Return || key == Qt::Key_Enter;
  }

  bool RfidEventFilterBase::eventFilter(QObject *watched, QEvent *event)
  {
      if (event->type() != QEvent::KeyPress)
          return QObject::eventFilter(watched, event);

      if (!gateOpen(watched))
          return QObject::eventFilter(watched, event);

      auto *ke = static_cast<QKeyEvent *>(event);
      const qint64 ts = m_clock.elapsed();

      if (isTerminator(ke->key())) {
          std::optional<QString> code = m_detector.feedKey(QChar('\n'), ts);
          if (code) {
              emit rfidScanned(*code);
              return true;   // consume Enter so no default activation fires
          }
          return QObject::eventFilter(watched, event);
      }

      const QString text = ke->text();
      if (text.isEmpty())   // modifiers / function keys
          return QObject::eventFilter(watched, event);

      m_detector.feedKey(text.at(0), ts);
      return QObject::eventFilter(watched, event);   // never consume printable keys
  }
  ```

- [ ] **Rewrite `qt-app/quick/RfidQuickFilter.h`** to derive from the base (only the QQuickWindow gate + `QPointer` remain here):
  ```cpp
  #ifndef RFIDQUICKFILTER_H
  #define RFIDQUICKFILTER_H

  #include <QPointer>
  #include "rfideventfilterbase.h"

  class QQuickWindow;

  // Quick-side install of RfidEventFilterBase. Gates on the specific login
  // QQuickWindow being active. QPointer so a destroyed window closes the gate
  // instead of dangling.
  class RfidQuickFilter : public RfidEventFilterBase
  {
      Q_OBJECT
  public:
      explicit RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent = nullptr);

  protected:
      bool gateOpen(QObject *watched) const override;

  private:
      QPointer<QQuickWindow> m_loginWindow;
  };

  #endif // RFIDQUICKFILTER_H
  ```

- [ ] **Rewrite `qt-app/quick/RfidQuickFilter.cpp`:**
  ```cpp
  #include "RfidQuickFilter.h"

  #include <QQuickWindow>

  RfidQuickFilter::RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent)
      : RfidEventFilterBase(parent)
      , m_loginWindow(loginWindow)
  {
  }

  bool RfidQuickFilter::gateOpen(QObject *watched) const
  {
      return !m_loginWindow.isNull()
          && watched == m_loginWindow
          && m_loginWindow->isActive();
  }
  ```
  Note: the Phase-1 `tst_rfidquickfilter.cpp` subclass `PublicQuickFilter` overrides `gateOpen` and calls `eventFilter` via `callEventFilter` — both are still `protected` on the base, so that test compiles and passes unchanged.

- [ ] **Rewrite `qt-app/rfidkeyboardfilter.h`** (legacy Widgets) to derive from the base:
  ```cpp
  #ifndef RFIDKEYBOARDFILTER_H
  #define RFIDKEYBOARDFILTER_H

  #include "rfideventfilterbase.h"

  class QWidget;

  // Widgets-side install of RfidEventFilterBase. Gates on the login window being
  // QApplication::activeWindow() AND `watched` being the key's primary target
  // (focusWidget else activeWindow) — the ancestor-dedup that stops a qApp-global
  // filter from double-counting a propagating key.
  class RfidKeyboardFilter : public RfidEventFilterBase
  {
      Q_OBJECT
  public:
      explicit RfidKeyboardFilter(QWidget *loginWindow, QObject *parent = nullptr);

  protected:
      bool gateOpen(QObject *watched) const override;

  private:
      QWidget *m_loginWindow;
  };

  #endif // RFIDKEYBOARDFILTER_H
  ```

- [ ] **Rewrite `qt-app/rfidkeyboardfilter.cpp`** — the dedup logic that was inline in the old `eventFilter` (lines 24–37) now lives entirely in `gateOpen`, so `tst_rfidkeyboardfilter`'s five behavioral slots (including `propagatedKeyCountedOnce` and `inactiveLoginWindowIgnoresScan`) still hold:
  ```cpp
  #include "rfidkeyboardfilter.h"

  #include <QApplication>
  #include <QWidget>

  RfidKeyboardFilter::RfidKeyboardFilter(QWidget *loginWindow, QObject *parent)
      : RfidEventFilterBase(parent)
      , m_loginWindow(loginWindow)
  {
  }

  bool RfidKeyboardFilter::gateOpen(QObject *watched) const
  {
      // Only while the login window is the active window.
      if (QApplication::activeWindow() != m_loginWindow)
          return false;
      // Act only on the key's primary target (focus widget, or the active window
      // when nothing is focused); skip propagated ancestor deliveries so a
      // qApp-global filter never double-counts a character.
      QWidget *target = QApplication::focusWidget();
      if (!target)
          target = QApplication::activeWindow();
      return watched == target;
  }
  ```

- [ ] **Add the base to `witscore` in `qt-app/core/CMakeLists.txt`** (after `loginparser.h loginparser.cpp`):
  ```cmake
      loginparser.h loginparser.cpp
      rfideventfilterbase.h rfideventfilterbase.cpp
  ```

- [ ] **Update `tst_rfidkeyboardfilter` in `qt-app/tests/CMakeLists.txt`** so it compiles the base directly (it already direct-compiles `rfidkeyboardfilter.cpp` + `rfidscandetector.cpp`; add the base). Only the `SOURCES` of that block change — add `${CMAKE_SOURCE_DIR}/core/rfideventfilterbase.cpp` and its header:
  ```cmake
  # tst_rfidkeyboardfilter sources — now include the shared base it derives from:
  #   tst_rfidkeyboardfilter.cpp
  #   ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.cpp
  #   ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.h
  #   ${CMAKE_SOURCE_DIR}/core/rfideventfilterbase.cpp
  #   ${CMAKE_SOURCE_DIR}/core/rfideventfilterbase.h
  #   ${CMAKE_SOURCE_DIR}/core/rfidscandetector.cpp
  #   ${CMAKE_SOURCE_DIR}/core/rfidscandetector.h
  ```
  Apply that source addition to the existing `tst_rfidkeyboardfilter` target definition (keep its current `Qt::Test Qt::Widgets` link and offscreen env). `tst_rfidquickfilter` (quick side) links `witsquickmodule` → `witscore`, so it already gets the base with no change.

- [ ] **Build and run both RFID tests green** (the regression gate for the dedup — identical behavior, deduplicated code):
  ```
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure -R "tst_rfidquickfilter|tst_rfidkeyboardfilter"
  ```
  Expected: both pass (all Phase-1 slots unchanged). Full suite — `19 tests passed` (no new target; count unchanged from Task 4). Confirm the legacy `WITS.exe` still builds and its kiosk RFID path is intact by smoke-running it later in Task 10's parity check.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/core/rfideventfilterbase.h`, `qt-app/core/rfideventfilterbase.cpp`, `qt-app/quick/RfidQuickFilter.h`, `qt-app/quick/RfidQuickFilter.cpp`, `qt-app/rfidkeyboardfilter.h`, `qt-app/rfidkeyboardfilter.cpp`, `qt-app/core/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`. Suggested message: `refactor(rfid): extract RfidEventFilterBase, share across Quick + legacy filters, QPointer window`.

---

### Task 6: `KioskViewModel` + `RecentLoginsModel`

The kiosk's C++ ViewModel: it owns the login orchestration (via `LoginParser` + a `QNetworkAccessManager`), the recent-logins list model, the live clock, the session stat counters, school info, and the guest-enabled flag — and installs `RfidQuickFilter` on the login window. It is the single QML-facing object for the kiosk screen. All network-free logic is exercised through the `applyStudentLogin` seam (no live network in tests, §8).

**Files:**
- Create: `qt-app/quick/models/RecentLoginsModel.h`, `qt-app/quick/models/RecentLoginsModel.cpp`
- Create: `qt-app/quick/viewmodels/KioskViewModel.h`, `qt-app/quick/viewmodels/KioskViewModel.cpp`
- Create: `qt-app/quick/tests/tst_kioskviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add both to module SOURCES; `models/` on the include path; register `tst_kioskviewmodel`)

**Interfaces:**
- Produces `class RecentLoginsModel : public QAbstractListModel` — roles `NameRole, CourseRole, YearShortRole, DeptRole, TimeRole, InitialsRole, FreshRole`; `void prepend(const QString &name, const QString &course, const QString &year, const QString &dept, const QString &time)` (marks the new row fresh, clears fresh on all others, caps at 40).
- Produces `class KioskViewModel : public QObject` (`QML_ELEMENT`) — properties `recentLogins` (the model, CONSTANT), `hasStudent`, `currentName/currentFullName/currentCourse/currentYear/currentDept/currentTime`, `visitorsToday`, `visitorsThisHour`, `clockTime/clockMeridiem/clockDate/greeting`, `schoolName/schoolAddress/libraryHours`, `guestEnabled`, `statusMessage/statusSeverity`; invokables `submitLogin(input)`, `handleRfidScan(code)`, `installRfid(QQuickWindow*)`, `requestGuest()`; signals `adminRequested()`, `guestRequested()` (+ the property NOTIFYs); public seam `void applyStudentLogin(const QJsonObject &student)`.
- Consumes: `LoginParser` (Task 4), `RfidQuickFilter` (Task 5), `ApiConfig` (core), `VisitorController` not used here (guest flow is Task 9).

Steps:

- [ ] **Write the failing test `qt-app/quick/tests/tst_kioskviewmodel.cpp`** (red — neither class exists). Drives the network-free seams: `applyStudentLogin` feeds the model + current + stats; an invalid RFID code sets an error status without a network call; the model caps and marks freshness:
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include <QJsonObject>
  #include "KioskViewModel.h"
  #include "RecentLoginsModel.h"

  static QJsonObject student(const QString &name, const QString &course,
                             const QString &year, const QString &dept,
                             const QString &time)
  {
      QJsonObject o;
      o.insert("name", name);
      o.insert("course", course);
      o.insert("year_level", year);
      o.insert("department", dept);
      o.insert("time_date", time);
      return o;
  }

  class TestKioskViewModel : public QObject
  {
      Q_OBJECT
  private slots:
      void applyStudentSetsCurrentAndBumpsStats();
      void applyStudentPrependsRowFresh();
      void modelCapsAtForty();
      void invalidRfidCodeSetsErrorStatusNoCrash();
      void requestGuestEmitsSignal();
  };

  void TestKioskViewModel::applyStudentSetsCurrentAndBumpsStats()
  {
      KioskViewModel vm;
      QSignalSpy cur(&vm, &KioskViewModel::currentChanged);
      QSignalSpy stat(&vm, &KioskViewModel::statsChanged);

      vm.applyStudentLogin(student("Maria Santos", "BSCE", "3rd Year",
                                   "Civil Engineering", "8:04 AM"));

      QVERIFY(vm.hasStudent());
      QCOMPARE(vm.currentFullName(), QStringLiteral("Maria Santos"));
      QCOMPARE(vm.currentName(), QStringLiteral("Maria"));       // first name only
      QCOMPARE(vm.visitorsToday(), 1);
      QCOMPARE(vm.visitorsThisHour(), 1);
      QVERIFY(cur.count() >= 1);
      QVERIFY(stat.count() >= 1);
  }

  void TestKioskViewModel::applyStudentPrependsRowFresh()
  {
      KioskViewModel vm;
      vm.applyStudentLogin(student("Maria Santos", "BSCE", "3rd Year", "CE", "8:04 AM"));
      vm.applyStudentLogin(student("Jose Ramirez", "BSEE", "2nd Year", "EE", "8:06 AM"));

      RecentLoginsModel *m = vm.recentLogins();
      QCOMPARE(m->rowCount(), 2);
      // Newest is row 0 and is the only "fresh" row.
      QCOMPARE(m->data(m->index(0), RecentLoginsModel::NameRole).toString(),
               QStringLiteral("Jose Ramirez"));
      QCOMPARE(m->data(m->index(0), RecentLoginsModel::FreshRole).toBool(), true);
      QCOMPARE(m->data(m->index(1), RecentLoginsModel::FreshRole).toBool(), false);
      // Initials derived.
      QCOMPARE(m->data(m->index(0), RecentLoginsModel::InitialsRole).toString(),
               QStringLiteral("JR"));
  }

  void TestKioskViewModel::modelCapsAtForty()
  {
      KioskViewModel vm;
      for (int i = 0; i < 45; ++i)
          vm.applyStudentLogin(student(QStringLiteral("S%1 Name").arg(i),
                                       "BSIT", "1st Year", "IT", "9:00 AM"));
      QCOMPARE(vm.recentLogins()->rowCount(), 40);
  }

  void TestKioskViewModel::invalidRfidCodeSetsErrorStatusNoCrash()
  {
      KioskViewModel vm;
      QSignalSpy st(&vm, &KioskViewModel::statusChanged);
      vm.handleRfidScan(QStringLiteral("A;"));   // fails isValidRfidCode -> no POST
      QVERIFY(!vm.statusMessage().isEmpty());
      QCOMPARE(vm.statusSeverity(), QStringLiteral("Error"));
      QVERIFY(st.count() >= 1);
      QCOMPARE(vm.recentLogins()->rowCount(), 0);  // nothing logged
  }

  void TestKioskViewModel::requestGuestEmitsSignal()
  {
      KioskViewModel vm;
      QSignalSpy g(&vm, &KioskViewModel::guestRequested);
      vm.requestGuest();
      QCOMPARE(g.count(), 1);
  }

  QTEST_MAIN(TestKioskViewModel)
  #include "tst_kioskviewmodel.moc"
  ```

- [ ] **Write `qt-app/quick/models/RecentLoginsModel.h`:**
  ```cpp
  #ifndef RECENTLOGINSMODEL_H
  #define RECENTLOGINSMODEL_H

  #include <QAbstractListModel>
  #include <QList>
  #include <QString>

  // The kiosk attendance feed. Newest first; the newest row is the only "fresh"
  // one (drives the gold highlight). Capped at 40 rows to match the design feed.
  class RecentLoginsModel : public QAbstractListModel
  {
      Q_OBJECT
  public:
      enum Roles {
          NameRole = Qt::UserRole + 1,
          CourseRole,
          YearShortRole,
          DeptRole,
          TimeRole,
          InitialsRole,
          FreshRole,
      };

      explicit RecentLoginsModel(QObject *parent = nullptr);

      int rowCount(const QModelIndex &parent = QModelIndex()) const override;
      QVariant data(const QModelIndex &index, int role) const override;
      QHash<int, QByteArray> roleNames() const override;

      void prepend(const QString &name, const QString &course,
                   const QString &year, const QString &dept, const QString &time);

  private:
      struct Row {
          QString name, course, yearShort, dept, time, initials;
          bool fresh;
      };
      static QString shortenYear(const QString &year);   // "3rd Year" -> "3rd Yr"
      static QString initialsOf(const QString &name);    // "Maria Santos" -> "MS"

      QList<Row> m_rows;
      static constexpr int kMaxRows = 40;
  };

  #endif // RECENTLOGINSMODEL_H
  ```

- [ ] **Write `qt-app/quick/models/RecentLoginsModel.cpp`:**
  ```cpp
  #include "RecentLoginsModel.h"

  RecentLoginsModel::RecentLoginsModel(QObject *parent)
      : QAbstractListModel(parent)
  {
  }

  int RecentLoginsModel::rowCount(const QModelIndex &parent) const
  {
      return parent.isValid() ? 0 : m_rows.size();
  }

  QVariant RecentLoginsModel::data(const QModelIndex &index, int role) const
  {
      if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
          return {};
      const Row &r = m_rows.at(index.row());
      switch (role) {
      case NameRole:      return r.name;
      case CourseRole:    return r.course;
      case YearShortRole: return r.yearShort;
      case DeptRole:      return r.dept;
      case TimeRole:      return r.time;
      case InitialsRole:  return r.initials;
      case FreshRole:     return r.fresh;
      default:            return {};
      }
  }

  QHash<int, QByteArray> RecentLoginsModel::roleNames() const
  {
      return {
          { NameRole,      "name" },
          { CourseRole,    "course" },
          { YearShortRole, "yearShort" },
          { DeptRole,      "dept" },
          { TimeRole,      "time" },
          { InitialsRole,  "initials" },
          { FreshRole,     "fresh" },
      };
  }

  QString RecentLoginsModel::shortenYear(const QString &year)
  {
      QString y = year;
      return y.replace(QLatin1String(" Year"), QLatin1String(" Yr"));
  }

  QString RecentLoginsModel::initialsOf(const QString &name)
  {
      QString out;
      const QStringList parts = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
      for (const QString &p : parts) {
          out.append(p.at(0).toUpper());
          if (out.size() == 2)
              break;
      }
      return out;
  }

  void RecentLoginsModel::prepend(const QString &name, const QString &course,
                                  const QString &year, const QString &dept,
                                  const QString &time)
  {
      // Clear the previous fresh flag(s) so only the incoming row highlights.
      for (int i = 0; i < m_rows.size(); ++i) {
          if (m_rows[i].fresh) {
              m_rows[i].fresh = false;
              emit dataChanged(index(i), index(i), { FreshRole });
          }
      }

      beginInsertRows(QModelIndex(), 0, 0);
      m_rows.prepend(Row{ name, course, shortenYear(year), dept, time,
                          initialsOf(name), true });
      endInsertRows();

      if (m_rows.size() > kMaxRows) {
          beginRemoveRows(QModelIndex(), kMaxRows, m_rows.size() - 1);
          m_rows.erase(m_rows.begin() + kMaxRows, m_rows.end());
          endRemoveRows();
      }
  }
  ```

- [ ] **Write `qt-app/quick/viewmodels/KioskViewModel.h`:**
  ```cpp
  #ifndef KIOSKVIEWMODEL_H
  #define KIOSKVIEWMODEL_H

  #include <QObject>
  #include <QString>
  #include <QJsonObject>
  #include <QElapsedTimer>
  #include <QUrl>
  #include <qqml.h>
  #include "RecentLoginsModel.h"

  class QNetworkAccessManager;
  class QQuickWindow;
  class QTimer;
  class RfidQuickFilter;

  // The kiosk surface's only QML-facing object. Owns login orchestration (via
  // LoginParser + a QNetworkAccessManager), the attendance model, a 1 Hz clock,
  // session stat counters, school info, and the guest-enabled flag.
  class KioskViewModel : public QObject
  {
      Q_OBJECT
      QML_ELEMENT
      Q_PROPERTY(RecentLoginsModel *recentLogins READ recentLogins CONSTANT)
      Q_PROPERTY(bool hasStudent READ hasStudent NOTIFY currentChanged)
      Q_PROPERTY(QString currentName READ currentName NOTIFY currentChanged)
      Q_PROPERTY(QString currentFullName READ currentFullName NOTIFY currentChanged)
      Q_PROPERTY(QString currentCourse READ currentCourse NOTIFY currentChanged)
      Q_PROPERTY(QString currentYear READ currentYear NOTIFY currentChanged)
      Q_PROPERTY(QString currentDept READ currentDept NOTIFY currentChanged)
      Q_PROPERTY(QString currentTime READ currentTime NOTIFY currentChanged)
      Q_PROPERTY(int visitorsToday READ visitorsToday NOTIFY statsChanged)
      Q_PROPERTY(int visitorsThisHour READ visitorsThisHour NOTIFY statsChanged)
      Q_PROPERTY(QString clockTime READ clockTime NOTIFY clockChanged)
      Q_PROPERTY(QString clockMeridiem READ clockMeridiem NOTIFY clockChanged)
      Q_PROPERTY(QString clockDate READ clockDate NOTIFY clockChanged)
      Q_PROPERTY(QString greeting READ greeting NOTIFY clockChanged)
      Q_PROPERTY(QString schoolName READ schoolName NOTIFY schoolInfoChanged)
      Q_PROPERTY(QString schoolAddress READ schoolAddress NOTIFY schoolInfoChanged)
      Q_PROPERTY(QString libraryHours READ libraryHours NOTIFY schoolInfoChanged)
      Q_PROPERTY(bool guestEnabled READ guestEnabled NOTIFY guestEnabledChanged)
      Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
      Q_PROPERTY(QString statusSeverity READ statusSeverity NOTIFY statusChanged)

  public:
      explicit KioskViewModel(QObject *parent = nullptr);

      RecentLoginsModel *recentLogins() { return &m_recent; }
      bool hasStudent() const { return m_hasStudent; }
      QString currentName() const { return m_currentName; }
      QString currentFullName() const { return m_currentFullName; }
      QString currentCourse() const { return m_currentCourse; }
      QString currentYear() const { return m_currentYear; }
      QString currentDept() const { return m_currentDept; }
      QString currentTime() const { return m_currentTime; }
      int visitorsToday() const { return m_visitorsToday; }
      int visitorsThisHour() const { return m_visitorsThisHour; }
      QString clockTime() const { return m_clockTime; }
      QString clockMeridiem() const { return m_clockMeridiem; }
      QString clockDate() const { return m_clockDate; }
      QString greeting() const { return m_greeting; }
      QString schoolName() const { return m_schoolName; }
      QString schoolAddress() const { return m_schoolAddress; }
      QString libraryHours() const { return m_libraryHours; }
      bool guestEnabled() const { return m_guestEnabled; }
      QString statusMessage() const { return m_statusMessage; }
      QString statusSeverity() const { return m_statusSeverity; }

      // QML entry points.
      Q_INVOKABLE void submitLogin(const QString &input);
      Q_INVOKABLE void handleRfidScan(const QString &code);
      Q_INVOKABLE void installRfid(QQuickWindow *window);
      Q_INVOKABLE void requestGuest();

      // Network-free seam: apply an already-parsed student record (used by the
      // network reply handlers AND directly by unit tests).
      void applyStudentLogin(const QJsonObject &student);

  signals:
      void currentChanged();
      void statsChanged();
      void clockChanged();
      void schoolInfoChanged();
      void guestEnabledChanged();
      void statusChanged();
      void adminRequested();
      void guestRequested();

  private:
      void tickClock();
      void setStatus(const QString &message, const QString &severity);
      void postForm(const QUrl &url, const QString &key, const QString &value,
                    bool rfid);

      RecentLoginsModel m_recent;
      QNetworkAccessManager *m_nam = nullptr;
      QTimer *m_clockTimer = nullptr;
      RfidQuickFilter *m_rfid = nullptr;

      bool m_hasStudent = false;
      QString m_currentName, m_currentFullName, m_currentCourse,
              m_currentYear, m_currentDept, m_currentTime;
      int m_visitorsToday = 0;
      int m_visitorsThisHour = 0;
      int m_currentHour = -1;
      QString m_clockTime, m_clockMeridiem, m_clockDate, m_greeting;
      QString m_schoolName, m_schoolAddress, m_libraryHours;
      bool m_guestEnabled = false;
      QString m_statusMessage, m_statusSeverity;

      // RFID debounce state (LoginParser::shouldDebounceRfid decides).
      QElapsedTimer m_rfidClock;
      QString m_lastRfidCode;
      qint64 m_lastRfidMs = -100000;
  };

  #endif // KIOSKVIEWMODEL_H
  ```

- [ ] **Write `qt-app/quick/viewmodels/KioskViewModel.cpp`.** School info + guest flag are read from the same `QSettings` the legacy admin writes (`"MyCompany"/"MyApp"`); live admin updates are Phase 4. The clock mirrors `mainwindow.cpp:353-357` formatting and the design's greeting bands. The network POSTs mirror `handleLogin`/`handleRfidLogin` and route their parsed results to `applyStudentLogin`/`setStatus`/`adminRequested`:
  ```cpp
  #include "KioskViewModel.h"

  #include <QNetworkAccessManager>
  #include <QNetworkRequest>
  #include <QNetworkReply>
  #include <QUrlQuery>
  #include <QTimer>
  #include <QSettings>
  #include <QDateTime>
  #include <QQuickWindow>   // complete type: installRfid() calls window->installEventFilter()
  #include "apiconfig.h"
  #include "loginparser.h"
  #include "RfidQuickFilter.h"

  KioskViewModel::KioskViewModel(QObject *parent)
      : QObject(parent)
      , m_nam(new QNetworkAccessManager(this))
      , m_clockTimer(new QTimer(this))
  {
      m_rfidClock.start();

      // School info + guest toggle: cache the legacy QSettings keys. The admin
      // surface (Phase 4) will write these live; here we read them at startup.
      QSettings s(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
      m_schoolName    = s.value(QStringLiteral("school/name")).toString();
      m_schoolAddress = s.value(QStringLiteral("school/address")).toString();
      m_libraryHours  = s.value(QStringLiteral("school/libraryHours"),
                                QStringLiteral("6 AM – 5 PM")).toString();
      m_guestEnabled  = s.value(QStringLiteral("kiosk/guestEnabled"), false).toBool();

      connect(m_clockTimer, &QTimer::timeout, this, &KioskViewModel::tickClock);
      m_clockTimer->start(1000);
      tickClock();   // populate immediately, no 1s blank
  }

  void KioskViewModel::tickClock()
  {
      const QDateTime now = QDateTime::currentDateTime();
      const int h24 = now.time().hour();

      m_clockMeridiem = h24 >= 12 ? QStringLiteral("PM") : QStringLiteral("AM");
      m_clockTime = now.toString(QStringLiteral("h:mm:ss"));
      m_clockDate = now.toString(QStringLiteral("dddd, MMMM d, yyyy"));
      m_greeting  = h24 < 12 ? QStringLiteral("Good morning")
                  : h24 < 17 ? QStringLiteral("Good afternoon")
                             : QStringLiteral("Good evening");

      // This-hour stat rolls over when the clock hour changes.
      if (m_currentHour != h24) {
          m_currentHour = h24;
          if (m_visitorsThisHour != 0) {
              m_visitorsThisHour = 0;
              emit statsChanged();
          }
      }
      emit clockChanged();
  }

  void KioskViewModel::setStatus(const QString &message, const QString &severity)
  {
      m_statusMessage = message;
      m_statusSeverity = severity;
      emit statusChanged();
  }

  void KioskViewModel::applyStudentLogin(const QJsonObject &student)
  {
      m_currentFullName = student.value(QStringLiteral("name")).toString();
      m_currentCourse   = student.value(QStringLiteral("course")).toString();
      m_currentYear     = student.value(QStringLiteral("year_level")).toString();
      m_currentDept     = student.value(QStringLiteral("department")).toString();
      m_currentTime     = student.value(QStringLiteral("time_date")).toString();
      m_currentName     = m_currentFullName.section(QLatin1Char(' '), 0, 0);  // first name
      m_hasStudent      = true;

      m_recent.prepend(m_currentFullName, m_currentCourse, m_currentYear,
                       m_currentDept, m_currentTime);

      ++m_visitorsToday;
      ++m_visitorsThisHour;

      emit currentChanged();
      emit statsChanged();
      setStatus(QString(), QString());   // clear any prior error toast
  }

  void KioskViewModel::postForm(const QUrl &url, const QString &key,
                                const QString &value, bool rfid)
  {
      QNetworkRequest request(url);
      request.setHeader(QNetworkRequest::ContentTypeHeader,
                        QStringLiteral("application/x-www-form-urlencoded"));
      QUrlQuery form;
      form.addQueryItem(key, value);
      QNetworkReply *reply =
          m_nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());

      connect(reply, &QNetworkReply::finished, this, [this, reply, rfid]() {
          const QByteArray body = reply->readAll();
          const bool netErr = reply->error() != QNetworkReply::NoError;
          reply->deleteLater();

          if (netErr) {
              setStatus(QStringLiteral("Network error. Please try again."),
                        QStringLiteral("Error"));
              return;
          }
          if (rfid) {
              const LoginParser::RfidResult r = LoginParser::parseRfidResponse(body);
              if (r.ok) applyStudentLogin(r.student);
              else      setStatus(r.message, QStringLiteral("Error"));
              return;
          }
          const LoginParser::LoginResult r = LoginParser::parseLoginResponse(body);
          if (r.ok && r.isStudent)      applyStudentLogin(r.student);
          else if (r.ok && r.isAdmin)   emit adminRequested();
          else                          setStatus(r.message, QStringLiteral("Error"));
      });
  }

  void KioskViewModel::submitLogin(const QString &input)
  {
      const QString trimmed = input.trimmed();
      if (trimmed.isEmpty()) {
          setStatus(QStringLiteral("Please enter your School ID or Admin Key."),
                    QStringLiteral("Error"));
          return;
      }
      if (LoginParser::classify(trimmed) == LoginParser::LoginKind::StudentId)
          postForm(ApiConfig::endpoint(QStringLiteral("student_login.php")),
                   QStringLiteral("school_id"), trimmed, false);
      else
          postForm(ApiConfig::endpoint(QStringLiteral("admin_login.php")),
                   QStringLiteral("admin_key"), trimmed, false);
  }

  void KioskViewModel::handleRfidScan(const QString &code)
  {
      const qint64 now = m_rfidClock.elapsed();
      if (LoginParser::shouldDebounceRfid(m_lastRfidCode, m_lastRfidMs, code, now))
          return;                               // one tap = one visit
      m_lastRfidCode = code;
      m_lastRfidMs = now;

      if (!LoginParser::isValidRfidCode(code)) {
          setStatus(QStringLiteral("Card not registered. Please see the librarian."),
                    QStringLiteral("Error"));
          return;                               // reject before any backend POST
      }
      postForm(ApiConfig::endpoint(QStringLiteral("rfid_login.php")),
               QStringLiteral("rfid_id"), code, true);
  }

  void KioskViewModel::installRfid(QQuickWindow *window)
  {
      if (m_rfid || !window)
          return;
      m_rfid = new RfidQuickFilter(window, this);
      window->installEventFilter(m_rfid);
      connect(m_rfid, &RfidQuickFilter::rfidScanned,
              this, &KioskViewModel::handleRfidScan);
  }

  void KioskViewModel::requestGuest()
  {
      emit guestRequested();
  }
  ```

- [ ] **Wire the two classes into `qt-app/quick/CMakeLists.txt`.** Add to the module `SOURCES` and put `models/` on the PUBLIC include path (so `KioskViewModel.h`'s `#include "RecentLoginsModel.h"` and the test's quote-includes resolve):
  ```cmake
      SOURCES
          RfidQuickFilter.h RfidQuickFilter.cpp
          viewmodels/ThemeViewModel.h viewmodels/ThemeViewModel.cpp
          viewmodels/Navigator.h viewmodels/Navigator.cpp
          viewmodels/KioskViewModel.h viewmodels/KioskViewModel.cpp
          models/RecentLoginsModel.h models/RecentLoginsModel.cpp
  ```
  ```cmake
  # models/ on the include path (analogous to the viewmodels/ line).
  target_include_directories(witsquickmodule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models)
  ```
  Then register the test (needs `Qt::Quick` for `QQuickWindow` in the VM header chain, though the test itself does not construct one):
  ```cmake
  # --- KioskViewModel + RecentLoginsModel unit test (C++ QtTest, offscreen) ---
  wits_add_qttest(tst_kioskviewmodel
      SOURCES tests/tst_kioskviewmodel.cpp
      LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network
      OFFSCREEN)
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R tst_kioskviewmodel --output-on-failure
  ```
  Expected: all 5 slots pass. Full suite — `20 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/models/RecentLoginsModel.h`, `qt-app/quick/models/RecentLoginsModel.cpp`, `qt-app/quick/viewmodels/KioskViewModel.h`, `qt-app/quick/viewmodels/KioskViewModel.cpp`, `qt-app/quick/tests/tst_kioskviewmodel.cpp`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat(quick): add KioskViewModel and RecentLoginsModel`.

---

### Task 7: Kiosk brand panel QML (`BrandPanel.qml`)

The maroon gradient sidebar (design left `<aside>`): circular logo, "Library Attendance" serif title + school name·address, the pulsing "SCAN OR TYPE YOUR ID" eyebrow, the ID `TextField`, the gold "LOG IN →" CTA, and the clock/date/hours block. It binds a `KioskViewModel` (injected as `property var vm`) and calls `vm.submitLogin(...)`. All colors come from `Theme`; the gradient is `Theme.brand.kiosk → Theme.brand.kioskHover`.

**Files:**
- Create: `qt-app/quick/qml/kiosk/BrandPanel.qml`
- Create: `qt-app/quick/tests/tst_qml_kiosk.qml`, `qt-app/quick/tests/tst_qml_kiosk.cpp` (a QuickTest that injects a stub VM)
- Modify: `qt-app/quick/CMakeLists.txt` (add `BrandPanel.qml` to `QML_FILES`; register `tst_qml_kiosk`)

**Interfaces:**
- Produces: `BrandPanel` — `property var vm` (a `KioskViewModel` or a stub exposing `schoolName`, `schoolAddress`, `libraryHours`, `clockTime`, `clockMeridiem`, `clockDate`, `guestEnabled`, `submitLogin(string)`, `requestGuest()`). Emits nothing; drives the VM directly.
- Consumes: `Theme`, `LButton`.

Steps:

- [ ] **Write the failing QuickTest data file `qt-app/quick/tests/tst_qml_kiosk.qml`** (red — `BrandPanel` does not exist). It injects a minimal stub VM (a `QtObject` with the property surface `BrandPanel` reads/calls) and asserts the panel instantiates, binds school info, and routes the CTA to `vm.submitLogin`:
  ```qml
  import QtQuick
  import QtTest
  import LOAMS

  Item {
      id: host
      width: 420; height: 800

      // Stub VM: same property/method surface BrandPanel consumes.
      QtObject {
          id: stubVm
          property string schoolName: "Maryknoll School of Lupon, Inc."
          property string schoolAddress: "Lupon, Davao Oriental"
          property string libraryHours: "6 AM – 5 PM"
          property string clockTime: "8:04:11"
          property string clockMeridiem: "AM"
          property string clockDate: "Monday, July 6, 2026"
          property bool guestEnabled: true
          property string lastSubmitted: ""
          property int guestRequests: 0
          function submitLogin(v) { lastSubmitted = v }
          function requestGuest() { guestRequests++ }
      }

      BrandPanel { id: panel; anchors.fill: parent; vm: stubVm }

      TestCase {
          name: "BrandPanel"
          when: windowShown

          function test_bindsSchoolInfo() {
              verify(findText(panel, "Maryknoll School of Lupon, Inc.") !== null);
          }
          function test_ctaSubmitsIdField() {
              panel.idText = "202300123";
              panel.submit();                       // the panel's submit hook
              compare(stubVm.lastSubmitted, "202300123");
          }
          function test_guestButtonVisibleWhenEnabled() {
              verify(panel.guestButtonVisible === true);
          }

          // Small helper: depth-first search for a Text with the given content.
          function findText(root, s) {
              if (root.text !== undefined && root.text === s) return root;
              for (var i = 0; i < root.children.length; i++) {
                  var f = findText(root.children[i], s);
                  if (f !== null) return f;
              }
              return null;
          }
      }
  }
  ```

- [ ] **Write `qt-app/quick/qml/kiosk/BrandPanel.qml`.** Exposes `idText`, `submit()`, and `guestButtonVisible` as the test's seams; the gold sweep/scan-pulse animations use `Theme.motion` and gate on `Theme.motion.enabled`:
  ```qml
  import QtQuick
  import QtQuick.Controls
  import QtQuick.Layouts
  import LOAMS

  // Maroon brand sidebar (design left aside). Binds a KioskViewModel via `vm`.
  Rectangle {
      id: panel
      property var vm
      // Test/consumer seams:
      property alias idText: idField.text
      readonly property bool guestButtonVisible: guestBtn.visible
      function submit() { if (vm) vm.submitLogin(idField.text); idField.clear() }

      implicitWidth: 390
      gradient: Gradient {
          GradientStop { position: 0.0; color: Theme.brand.kiosk }
          GradientStop { position: 1.0; color: Theme.brand.kioskHover }
      }

      ColumnLayout {
          anchors.fill: parent
          anchors.margins: Theme.spacing.xxxl
          spacing: Theme.spacing.xxl

          // Brand block: logo + titles.
          RowLayout {
              spacing: Theme.spacing.xl
              Rectangle {
                  Layout.preferredWidth: 96; Layout.preferredHeight: 96
                  radius: width / 2
                  color: Theme.card
                  border.width: 3
                  border.color: Theme.secondary
                  // Logo image lands in Phase 4 (admin logo import); circle placeholder now.
                  Text {
                      anchors.centerIn: parent
                      text: qsTr("LOGO"); color: Theme.mutedText
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.eyebrow
                  }
              }
              ColumnLayout {
                  spacing: Theme.spacing.xs
                  Text {
                      text: qsTr("Library Attendance")
                      color: Theme.brand.onKiosk
                      font.family: Theme.typography.serif
                      font.pixelSize: Theme.typography.pageTitle
                      font.weight: Font.Bold
                  }
                  Text {
                      text: (panel.vm ? panel.vm.schoolName : "") + " · "
                            + (panel.vm ? panel.vm.schoolAddress : "")
                      color: Theme.onBrandMuted
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.control
                      wrapMode: Text.WordWrap
                      Layout.maximumWidth: 240
                  }
              }
          }

          // Scan/type block.
          ColumnLayout {
              Layout.fillWidth: true
              spacing: Theme.spacing.sm
              RowLayout {
                  spacing: Theme.spacing.sm
                  Rectangle {
                      width: 7; height: 7; radius: 3.5; color: Theme.secondary
                      SequentialAnimation on opacity {
                          running: Theme.motion.enabled; loops: Animation.Infinite
                          NumberAnimation { to: 1.0; duration: 900 }
                          NumberAnimation { to: 0.45; duration: 900 }
                      }
                  }
                  Text {
                      text: qsTr("SCAN OR TYPE YOUR ID")
                      color: Theme.secondary
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.eyebrow
                      font.weight: Font.ExtraBold
                      font.letterSpacing: 1.6
                  }
              }
              TextField {
                  id: idField
                  Layout.fillWidth: true
                  placeholderText: qsTr("ID number…")
                  color: Theme.brand.onKiosk
                  font.family: Theme.typography.sans
                  font.pixelSize: Theme.typography.cardTitle
                  // Admin keys are alphabetic -> mask; student IDs are digits -> show.
                  echoMode: (text.length > 0 && !/^[0-9]/.test(text))
                            ? TextInput.Password : TextInput.Normal
                  background: Rectangle {
                      radius: Theme.radius.md
                      color: Qt.alpha(Theme.card, 0.10)
                      border.width: 1
                      border.color: idField.activeFocus
                                    ? Theme.secondary : Qt.alpha(Theme.card, 0.28)
                  }
                  onAccepted: panel.submit()
                  Accessible.role: Accessible.EditableText
                  Accessible.name: qsTr("ID number")
              }
              LButton {
                  Layout.fillWidth: true
                  variant: "Accent"
                  text: qsTr("LOG IN →")
                  onClicked: panel.submit()
              }
              LButton {
                  id: guestBtn
                  Layout.fillWidth: true
                  variant: "Outline"
                  text: qsTr("Guest log in")
                  visible: panel.vm ? panel.vm.guestEnabled : false
                  onClicked: if (panel.vm) panel.vm.requestGuest()
              }
          }

          Item { Layout.fillHeight: true }   // push the clock to the bottom

          // Clock block.
          ColumnLayout {
              spacing: Theme.spacing.xs
              RowLayout {
                  spacing: Theme.spacing.sm
                  Text {
                      text: panel.vm ? panel.vm.clockTime : ""
                      color: Theme.brand.onKiosk
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.statValue
                      font.weight: Font.ExtraBold
                  }
                  Text {
                      text: panel.vm ? panel.vm.clockMeridiem : ""
                      color: Theme.secondary
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.cardTitle
                      Layout.alignment: Qt.AlignBottom
                  }
              }
              Text {
                  text: (panel.vm ? panel.vm.clockDate : "") + qsTr("  ·  Open ")
                        + (panel.vm ? panel.vm.libraryHours : "")
                  color: Theme.onBrandMuted
                  font.family: Theme.typography.sans
                  font.pixelSize: Theme.typography.control
              }
          }
      }
  }
  ```

- [ ] **Write the QuickTest C++ runner `qt-app/quick/tests/tst_qml_kiosk.cpp`** (mirrors `tst_qml_components.cpp` — adds the import path for the statically-embedded module):
  ```cpp
  #include <QtQuickTest/quicktest.h>
  #include <QQmlEngine>

  class Setup : public QObject
  {
      Q_OBJECT
  public slots:
      void qmlEngineAvailable(QQmlEngine *engine)
      {
          engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
      }
  };

  QUICK_TEST_MAIN_WITH_SETUP(tst_qml_kiosk, Setup)
  #include "tst_qml_kiosk.moc"
  ```

- [ ] **Wire the QML + test into `qt-app/quick/CMakeLists.txt`.** Add `qml/kiosk/BrandPanel.qml` to `QML_FILES`, then register the QuickTest (same static-plugin escape hatch as the other Quick tests):
  ```cmake
  # --- Kiosk screens Quick test (offscreen) ---
  wits_add_qttest(tst_qml_kiosk
      SOURCES tests/tst_qml_kiosk.cpp
      LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
           Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
      DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
      OFFSCREEN)
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R tst_qml_kiosk --output-on-failure
  ```
  Expected: `BrandPanel` cases pass. Full suite — `21 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/qml/kiosk/BrandPanel.qml`, `qt-app/quick/tests/tst_qml_kiosk.qml`, `qt-app/quick/tests/tst_qml_kiosk.cpp`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat(kiosk): add BrandPanel QML (logo, ID input, gold CTA, clock)`.

---

### Task 8: Kiosk main area QML (`KioskMain.qml` + `KioskFeedRow.qml`)

The right-hand main area (design `<main>`): the greeting headline that flips between "…just logged in" and the idle "waiting for log in…" empty-state, the LIVE FEED badge, the "NOW SIGNED IN" hero card, two `LStatTile`s (visitors today / this hour), and the attendance feed — a `ListView` over `vm.recentLogins` with per-row entry motion (`rowIn`), rendered by `KioskFeedRow`.

**Files:**
- Create: `qt-app/quick/qml/kiosk/KioskMain.qml`, `qt-app/quick/qml/kiosk/KioskFeedRow.qml`
- Modify: `qt-app/quick/tests/tst_qml_kiosk.qml` (add `KioskMain` cases), `qt-app/quick/CMakeLists.txt` (add the two QML files)

**Interfaces:**
- Produces: `KioskMain` — `property var vm` (reads `hasStudent`, `currentName/FullName/Course/Year/Dept/Time`, `greeting`, `visitorsToday`, `visitorsThisHour`, `recentLogins`). `KioskFeedRow` — a delegate reading the model roles `name, course, yearShort, dept, time, initials, fresh`.
- Consumes: `Theme`, `LStatTile`, `LCard`.

Steps:

- [ ] **Add the failing `KioskMain` cases to `qt-app/quick/tests/tst_qml_kiosk.qml`** (red — `KioskMain` does not exist). Extend the stub VM with the main-area surface + a real `ListModel` masquerading as `recentLogins`, and add a second host + TestCase. Append inside the top-level `Item`:
  ```qml
      // --- KioskMain fixture ---
      ListModel {
          id: feedModel
          ListElement { name: "Maria Santos"; course: "BSCE"; yearShort: "3rd Yr"; dept: "Civil Engineering"; time: "8:04 AM"; initials: "MS"; fresh: true }
          ListElement { name: "Jose Ramirez"; course: "BSEE"; yearShort: "2nd Yr"; dept: "Electrical Engineering"; time: "7:58 AM"; initials: "JR"; fresh: false }
      }
      QtObject {
          id: stubMainVm
          property bool hasStudent: true
          property string greeting: "Good morning"
          property string currentName: "Maria"
          property string currentFullName: "Maria Santos"
          property string currentCourse: "BSCE"
          property string currentYear: "3rd Year"
          property string currentDept: "Civil Engineering"
          property string currentTime: "8:04 AM"
          property int visitorsToday: 27
          property int visitorsThisHour: 4
          property var recentLogins: feedModel
      }
      KioskMain { id: main; width: 800; height: 800; vm: stubMainVm }

      TestCase {
          name: "KioskMain"
          when: windowShown
          function test_feedRowCountMatchesModel() {
              compare(main.feedCount, 2);
          }
          function test_heroShowsCurrentWhenHasStudent() {
              verify(main.heroShowsStudent === true);
          }
          function test_statTilesBound() {
              compare(main.visitorsTodayShown, "27");
          }
      }
  ```

- [ ] **Write `qt-app/quick/qml/kiosk/KioskFeedRow.qml`** — one feed row (avatar chip + name / course·year / dept / time), fresh-row gold styling driven by the `fresh` role, with the `rowIn` entry animation:
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // One attendance-feed row. `model*` come from RecentLoginsModel roles.
  Rectangle {
      id: row
      property string rowName
      property string rowCourse
      property string rowYearShort
      property string rowDept
      property string rowTime
      property string rowInitials
      property bool rowFresh: false

      implicitHeight: 60
      color: rowFresh ? Theme.secondarySoft : Theme.card
      // Left edge highlight for the freshest row.
      Rectangle {
          anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
          width: 4
          color: row.rowFresh ? Theme.secondary : "transparent"
      }
      Rectangle {   // bottom hairline
          anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
          height: 1; color: Theme.rowHairline
      }

      RowLayout {
          anchors.fill: parent
          anchors.leftMargin: Theme.spacing.xxl
          anchors.rightMargin: Theme.spacing.xxl
          spacing: Theme.spacing.md

          Rectangle {   // avatar chip
              Layout.preferredWidth: 34; Layout.preferredHeight: 34
              radius: width / 2
              color: row.rowFresh ? Theme.brand.kiosk : Theme.brand.kioskSoft
              Text {
                  anchors.centerIn: parent
                  text: row.rowInitials
                  color: row.rowFresh ? Theme.brand.onKiosk : Theme.brand.kiosk
                  font.family: Theme.typography.sans
                  font.pixelSize: Theme.typography.control
                  font.weight: Font.ExtraBold
              }
          }
          Text {
              Layout.preferredWidth: 200
              text: row.rowName
              color: Theme.text
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.body
              font.weight: Font.ExtraBold
              elide: Text.ElideRight
          }
          Text {
              Layout.fillWidth: true
              text: row.rowCourse + " · " + row.rowYearShort
              color: Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.control
          }
          Text {
              Layout.preferredWidth: 160
              text: row.rowDept
              color: Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.control
              elide: Text.ElideRight
          }
          Text {
              text: row.rowTime
              color: row.rowFresh ? Theme.brand.kiosk : Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.control
              font.weight: Font.ExtraBold
          }
      }

      // rowIn: slide-up + fade on insertion.
      opacity: 0
      Component.onCompleted: rowIn.start()
      ParallelAnimation {
          id: rowIn
          running: false
          NumberAnimation { target: row; property: "opacity"; from: 0; to: 1; duration: Theme.motion.rowIn }
          NumberAnimation { target: row; property: "y"; from: row.y + 14; to: row.y; duration: Theme.motion.rowIn; easing.type: Easing.OutCubic }
      }
  }
  ```

- [ ] **Write `qt-app/quick/qml/kiosk/KioskMain.qml`.** Exposes `feedCount`, `heroShowsStudent`, `visitorsTodayShown` as test seams:
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // Right-hand main area: greeting, hero card + stat tiles, live feed.
  Item {
      id: mainArea
      property var vm
      readonly property int feedCount: feed.count
      readonly property bool heroShowsStudent: vm ? vm.hasStudent : false
      readonly property string visitorsTodayShown: todayTile.value

      ColumnLayout {
          anchors.fill: parent
          anchors.margins: Theme.spacing.xxl
          spacing: Theme.spacing.lg

          // Greeting headline + LIVE FEED badge.
          RowLayout {
              Layout.fillWidth: true
              Text {
                  Layout.fillWidth: true
                  font.family: Theme.typography.serif
                  font.pixelSize: Theme.typography.heroName
                  font.weight: Font.Bold
                  wrapMode: Text.WordWrap
                  text: (mainArea.vm && mainArea.vm.hasStudent)
                        ? (mainArea.vm.greeting + " 👋  " + mainArea.vm.currentName + qsTr(" just logged in"))
                        : ((mainArea.vm ? mainArea.vm.greeting : "") + qsTr(" — waiting for log in…"))
                  color: (mainArea.vm && mainArea.vm.hasStudent)
                         ? Theme.text : Theme.mutedTextCaption
              }
              RowLayout {
                  spacing: Theme.spacing.sm
                  Rectangle {
                      width: 7; height: 7; radius: 3.5; color: Theme.brand.kiosk
                      SequentialAnimation on opacity {
                          running: Theme.motion.enabled; loops: Animation.Infinite
                          NumberAnimation { to: 1.0; duration: 800 }
                          NumberAnimation { to: 0.45; duration: 800 }
                      }
                  }
                  Text {
                      text: qsTr("LIVE FEED"); color: Theme.brand.kiosk
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.eyebrow
                      font.weight: Font.ExtraBold; font.letterSpacing: 1.4
                  }
              }
          }

          // Hero + stat tiles.
          RowLayout {
              Layout.fillWidth: true
              spacing: Theme.spacing.md
              LCard {
                  id: hero
                  Layout.fillWidth: true
                  Layout.preferredWidth: 2
                  Layout.preferredHeight: 120
                  filled: true
                  ColumnLayout {
                      anchors.fill: parent
                      spacing: Theme.spacing.xs
                      Text {
                          text: qsTr("NOW SIGNED IN")
                          color: Theme.secondary
                          font.family: Theme.typography.sans
                          font.pixelSize: Theme.typography.eyebrow
                          font.weight: Font.ExtraBold; font.letterSpacing: 1.4
                      }
                      Text {
                          visible: mainArea.vm ? mainArea.vm.hasStudent : false
                          text: mainArea.vm ? mainArea.vm.currentFullName : ""
                          color: Theme.brand.onKiosk
                          font.family: Theme.typography.serif
                          font.pixelSize: Theme.typography.heroName
                          font.weight: Font.Bold
                      }
                      Text {
                          visible: mainArea.vm ? mainArea.vm.hasStudent : false
                          text: mainArea.vm
                                ? (mainArea.vm.currentCourse + " · " + mainArea.vm.currentYear
                                   + " · " + mainArea.vm.currentDept + " · " + mainArea.vm.currentTime)
                                : ""
                          color: Theme.onBrandMuted
                          font.family: Theme.typography.sans
                          font.pixelSize: Theme.typography.body
                      }
                      Text {
                          visible: mainArea.vm ? !mainArea.vm.hasStudent : true
                          text: qsTr("Scan your ID to begin")
                          color: Theme.onBrandMuted
                          font.family: Theme.typography.serif
                          font.pixelSize: Theme.typography.cardTitle
                      }
                  }
              }
              LStatTile {
                  id: todayTile
                  Layout.fillWidth: true
                  Layout.preferredHeight: 120
                  label: qsTr("VISITORS TODAY")
                  value: mainArea.vm ? String(mainArea.vm.visitorsToday) : "0"
              }
              LStatTile {
                  Layout.fillWidth: true
                  Layout.preferredHeight: 120
                  label: qsTr("THIS HOUR")
                  value: mainArea.vm ? String(mainArea.vm.visitorsThisHour) : "0"
              }
          }

          // Attendance feed.
          LCard {
              Layout.fillWidth: true
              Layout.fillHeight: true
              padding: 0
              ListView {
                  id: feed
                  anchors.fill: parent
                  clip: true
                  model: mainArea.vm ? mainArea.vm.recentLogins : null
                  delegate: KioskFeedRow {
                      width: ListView.view ? ListView.view.width : 0
                      rowName: model.name
                      rowCourse: model.course
                      rowYearShort: model.yearShort
                      rowDept: model.dept
                      rowTime: model.time
                      rowInitials: model.initials
                      rowFresh: model.fresh
                  }
              }
          }
      }
  }
  ```

- [ ] **Add the two QML files to `qt-app/quick/CMakeLists.txt`** `QML_FILES` (after `KioskScreen.qml`):
  ```cmake
          qml/kiosk/KioskScreen.qml
          qml/kiosk/BrandPanel.qml
          qml/kiosk/KioskMain.qml
          qml/kiosk/KioskFeedRow.qml
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R tst_qml_kiosk --output-on-failure
  ```
  Expected: `BrandPanel` + `KioskMain` cases pass. Full suite — still `21 tests passed` (same target, more cases).

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/qml/kiosk/KioskMain.qml`, `qt-app/quick/qml/kiosk/KioskFeedRow.qml`, `qt-app/quick/tests/tst_qml_kiosk.qml`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat(kiosk): add main area (greeting, hero, stat tiles, live feed)`.

---

### Task 9: Guest flow (`GuestViewModel` + `GuestDialog.qml`)

Reproduce the legacy guest sign-in (`guestwindow.cpp`): a modal with Name / Contact / Company / Purpose (Name, Company, Purpose required), POSTing `guest_login.php`. A `GuestViewModel` owns the network + validation; `GuestDialog.qml` (built on `LDialog`) is shown when the brand-panel guest button fires `vm.requestGuest()` (gated on `guestEnabled`).

**Files:**
- Create: `qt-app/quick/viewmodels/GuestViewModel.h`, `qt-app/quick/viewmodels/GuestViewModel.cpp`
- Create: `qt-app/quick/qml/kiosk/GuestDialog.qml`
- Create: `qt-app/quick/tests/tst_guestviewmodel.cpp`
- Modify: `qt-app/quick/qml/components/LDialog.qml` (redefine as a self-contained modal with a content slot — review Round 1), `qt-app/quick/CMakeLists.txt` (module SOURCES + `GuestDialog.qml`; register `tst_guestviewmodel`)

**Interfaces:**
- Produces: `class GuestViewModel : public QObject` (`QML_ELEMENT`) — `Q_INVOKABLE void submitGuest(name, contact, company, purpose)`; `signals: void guestSucceeded(QString message), guestFailed(QString message)`; `static bool validate(name, company, purpose)` (all three non-empty); seam `void applyGuestResponse(const QByteArray &json)`.
- Consumes: `ApiConfig`, `QNetworkAccessManager`; `LDialog`, `LButton`, `Theme`.

Steps:

- [ ] **Write the failing test `qt-app/quick/tests/tst_guestviewmodel.cpp`** (red — class does not exist). Tests validation + the network-free response seam:
  ```cpp
  #include <QtTest>
  #include <QSignalSpy>
  #include "GuestViewModel.h"

  class TestGuestViewModel : public QObject
  {
      Q_OBJECT
  private slots:
      void validateRequiresNameCompanyPurpose();
      void applyResponseSuccessEmitsSucceeded();
      void applyResponseFailureEmitsFailed();
      void submitWithMissingFieldsEmitsFailedNoNetwork();
  };

  void TestGuestViewModel::validateRequiresNameCompanyPurpose()
  {
      QVERIFY(GuestViewModel::validate("Ana", "Acme", "Meeting"));
      QVERIFY(!GuestViewModel::validate("", "Acme", "Meeting"));
      QVERIFY(!GuestViewModel::validate("Ana", "", "Meeting"));
      QVERIFY(!GuestViewModel::validate("Ana", "Acme", ""));
  }

  void TestGuestViewModel::applyResponseSuccessEmitsSucceeded()
  {
      GuestViewModel vm;
      QSignalSpy ok(&vm, &GuestViewModel::guestSucceeded);
      vm.applyGuestResponse(R"({"status":"success","message":"Welcome"})");
      QCOMPARE(ok.count(), 1);
      QCOMPARE(ok.at(0).at(0).toString(), QStringLiteral("Welcome"));
  }

  void TestGuestViewModel::applyResponseFailureEmitsFailed()
  {
      GuestViewModel vm;
      QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
      vm.applyGuestResponse(R"({"status":"error","message":"Nope"})");
      QCOMPARE(bad.count(), 1);
  }

  void TestGuestViewModel::submitWithMissingFieldsEmitsFailedNoNetwork()
  {
      GuestViewModel vm;
      QSignalSpy bad(&vm, &GuestViewModel::guestFailed);
      vm.submitGuest("Ana", "0917", "", "Meeting");   // company missing
      QCOMPARE(bad.count(), 1);                         // rejected before any POST
  }

  QTEST_MAIN(TestGuestViewModel)
  #include "tst_guestviewmodel.moc"
  ```

- [ ] **Write `qt-app/quick/viewmodels/GuestViewModel.h`:**
  ```cpp
  #ifndef GUESTVIEWMODEL_H
  #define GUESTVIEWMODEL_H

  #include <QObject>
  #include <QString>
  #include <QByteArray>
  #include <qqml.h>

  class QNetworkAccessManager;

  // Guest sign-in: validates + POSTs guest_login.php. Mirrors guestwindow.cpp.
  class GuestViewModel : public QObject
  {
      Q_OBJECT
      QML_ELEMENT
  public:
      explicit GuestViewModel(QObject *parent = nullptr);

      Q_INVOKABLE void submitGuest(const QString &name, const QString &contact,
                                   const QString &company, const QString &purpose);

      static bool validate(const QString &name, const QString &company,
                           const QString &purpose);

      // Network-free seam for tests + the reply handler.
      void applyGuestResponse(const QByteArray &json);

  signals:
      void guestSucceeded(const QString &message);
      void guestFailed(const QString &message);

  private:
      QNetworkAccessManager *m_nam = nullptr;
  };

  #endif // GUESTVIEWMODEL_H
  ```

- [ ] **Write `qt-app/quick/viewmodels/GuestViewModel.cpp`:**
  ```cpp
  #include "GuestViewModel.h"

  #include <QNetworkAccessManager>
  #include <QNetworkRequest>
  #include <QNetworkReply>
  #include <QUrlQuery>
  #include <QJsonDocument>
  #include <QJsonObject>
  #include "apiconfig.h"

  GuestViewModel::GuestViewModel(QObject *parent)
      : QObject(parent)
      , m_nam(new QNetworkAccessManager(this))
  {
  }

  bool GuestViewModel::validate(const QString &name, const QString &company,
                                const QString &purpose)
  {
      return !name.trimmed().isEmpty()
          && !company.trimmed().isEmpty()
          && !purpose.trimmed().isEmpty();
  }

  void GuestViewModel::submitGuest(const QString &name, const QString &contact,
                                   const QString &company, const QString &purpose)
  {
      if (!validate(name, company, purpose)) {
          emit guestFailed(QStringLiteral("Name, Company, and Purpose are required."));
          return;
      }

      QNetworkRequest request(ApiConfig::endpoint(QStringLiteral("guest_login.php")));
      request.setHeader(QNetworkRequest::ContentTypeHeader,
                        QStringLiteral("application/x-www-form-urlencoded"));
      QUrlQuery form;
      form.addQueryItem(QStringLiteral("name"), name.trimmed());
      form.addQueryItem(QStringLiteral("company"), company.trimmed());
      form.addQueryItem(QStringLiteral("contact"), contact.trimmed());
      form.addQueryItem(QStringLiteral("purpose"), purpose.trimmed());

      QNetworkReply *reply =
          m_nam->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
      connect(reply, &QNetworkReply::finished, this, [this, reply]() {
          const QByteArray body = reply->readAll();
          const bool netErr = reply->error() != QNetworkReply::NoError;
          reply->deleteLater();
          if (netErr) {
              emit guestFailed(QStringLiteral("Network error. Please try again."));
              return;
          }
          applyGuestResponse(body);
      });
  }

  void GuestViewModel::applyGuestResponse(const QByteArray &json)
  {
      const QJsonDocument doc = QJsonDocument::fromJson(json);
      if (!doc.isObject()) {
          emit guestFailed(QStringLiteral("Invalid server response."));
          return;
      }
      const QJsonObject obj = doc.object();
      if (obj.value("status").toString() == QLatin1String("success"))
          emit guestSucceeded(obj.value("message").toString());
      else
          emit guestFailed(obj.value("message").toString());
  }
  ```

- [ ] **Redefine `qt-app/quick/qml/components/LDialog.qml` as a self-contained modal** (review Round 1, Important). The current skeleton is a `QtQuick.Controls.Dialog` with a hardcoded `contentItem: Text` — a child layout has nowhere to land, and overriding `open()`/`close()`/`visible` fights the popup's own semantics (and any resulting warning would trip `tst_appshell`'s zero-QML-warning gate once `KioskScreen` instantiates the guest dialog). Replace it with a plain overlay (scrim + centered card) exposing a `default property alias content`, a `title`, and `message` (kept for back-compat). `LDialog { title: "T" }` still instantiates, so `tst_qml_components` stays green:
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // The only modal-interruption primitive 2.0 keeps (§11). A self-contained
  // overlay (scrim + centered card) — NOT a Controls.Dialog — so screens drop
  // arbitrary content into it and drive it with plain `visible`.
  Item {
      id: dlg
      property string title: ""
      property string message: ""
      default property alias content: body.data

      anchors.fill: parent
      visible: false

      Rectangle {                                   // scrim: dim + swallow clicks
          anchors.fill: parent
          color: Theme.scrim
          MouseArea { anchors.fill: parent }
      }

      Rectangle {
          anchors.centerIn: parent
          width: Math.min(parent.width - Theme.spacing.xxxl * 2, 460)
          implicitHeight: card.implicitHeight + Theme.spacing.xxl * 2
          color: Theme.card
          radius: Theme.radius.card
          border.width: 2
          border.color: Theme.border

          ColumnLayout {
              id: card
              anchors.fill: parent
              anchors.margins: Theme.spacing.xxl
              spacing: Theme.spacing.lg
              Text {
                  visible: dlg.title.length > 0
                  text: dlg.title
                  color: Theme.text
                  font.family: Theme.typography.serif
                  font.pixelSize: Theme.typography.cardTitle
                  font.weight: Font.Bold
              }
              Text {
                  visible: dlg.message.length > 0
                  text: dlg.message
                  color: Theme.text
                  font.family: Theme.typography.sans
                  font.pixelSize: Theme.typography.body
                  wrapMode: Text.WordWrap
                  Layout.fillWidth: true
              }
              Item {                                 // consumer content lands here
                  id: body
                  Layout.fillWidth: true
                  implicitHeight: childrenRect.height
              }
          }
      }

      Accessible.role: Accessible.Dialog
      Accessible.name: dlg.title
  }
  ```

- [ ] **Write `qt-app/quick/qml/kiosk/GuestDialog.qml`** — four explicit fields + submit/cancel over the new `LDialog`. Drives visibility with plain `visible` via `show()`/`hide()` (no shadowed popup methods) and closes on success via a `Connections` on the VM:
  ```qml
  import QtQuick
  import QtQuick.Controls
  import QtQuick.Layouts
  import LOAMS

  // Guest sign-in modal. Show with show(); binds a GuestViewModel via `vm`.
  LDialog {
      id: dlg
      property var vm
      title: qsTr("Guest Sign-In")

      function show() { clearFields(); visible = true; nameField.forceActiveFocus() }
      function hide() { visible = false }
      function clearFields() {
          nameField.text = ""; contactField.text = "";
          companyField.text = ""; purposeField.text = "";
      }

      // Inline component (Qt 6.3+): each row is a real object with a `text`
      // alias — no objectName / Component.onCompleted handle-juggling.
      component Field: ColumnLayout {
          property alias text: input.text
          property string label
          Layout.fillWidth: true
          spacing: Theme.spacing.xs
          Text {
              text: label; color: Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.control
          }
          TextField {
              id: input
              Layout.fillWidth: true
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.body
              color: Theme.text
              background: Rectangle {
                  radius: Theme.radius.sm; color: Theme.card
                  border.width: 1; border.color: Theme.border
              }
          }
      }

      ColumnLayout {
          Layout.fillWidth: true
          spacing: Theme.spacing.md

          Field { id: nameField;    label: qsTr("Full name *") }
          Field { id: contactField; label: qsTr("Contact") }
          Field { id: companyField; label: qsTr("Company *") }
          Field { id: purposeField; label: qsTr("Purpose *") }

          RowLayout {
              Layout.alignment: Qt.AlignRight
              spacing: Theme.spacing.sm
              LButton { variant: "Ghost"; text: qsTr("Cancel"); onClicked: dlg.hide() }
              LButton {
                  variant: "Accent"; text: qsTr("Submit")
                  onClicked: if (dlg.vm) dlg.vm.submitGuest(
                      nameField.text, contactField.text,
                      companyField.text, purposeField.text)
              }
          }
      }

      Connections {
          target: dlg.vm
          function onGuestSucceeded(message) { dlg.hide() }
          // onGuestFailed surfaces via the kiosk toast (wired in Task 10).
      }
  }
  ```
  The kiosk screen calls `guestDialog.show()` (Task 10), not `open()`.

- [ ] **Wire into `qt-app/quick/CMakeLists.txt`.** Add `GuestViewModel` to module SOURCES, `GuestDialog.qml` to `QML_FILES`, and register the test:
  ```cmake
      SOURCES
          # ... existing ...
          viewmodels/GuestViewModel.h viewmodels/GuestViewModel.cpp
  ```
  ```cmake
          qml/kiosk/GuestDialog.qml
  ```
  ```cmake
  # --- GuestViewModel unit test (C++ QtTest, offscreen) ---
  wits_add_qttest(tst_guestviewmodel
      SOURCES tests/tst_guestviewmodel.cpp
      LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network
      OFFSCREEN)
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then:
  ```
  ctest --test-dir qt-app/build -R "tst_guestviewmodel|tst_qml_components" --output-on-failure
  ```
  Expected: `tst_guestviewmodel` passes (4 slots); `tst_qml_components` still passes (the `LDialog` redefinition keeps `LDialog { title: "T" }` instantiable). Full suite — `22 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/viewmodels/GuestViewModel.h`, `qt-app/quick/viewmodels/GuestViewModel.cpp`, `qt-app/quick/qml/kiosk/GuestDialog.qml`, `qt-app/quick/qml/components/LDialog.qml`, `qt-app/quick/tests/tst_guestviewmodel.cpp`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat(kiosk): add guest sign-in flow (GuestViewModel + GuestDialog)`.

---

### Task 10: Assemble `KioskScreen`, install RFID, parity checklist + gates

Compose the pieces into the real kiosk screen: `KioskScreen.qml` creates the `KioskViewModel`, lays out `BrandPanel` + `KioskMain` responsively, hosts the status toast + `GuestDialog`, routes `adminRequested` → `Navigator.showAdmin()`, and installs `RfidQuickFilter` on the `AppShell` window. Then run the app, execute the parity checklist against the legacy kiosk, and clear the phase gates.

**Files:**
- Modify: `qt-app/quick/qml/kiosk/KioskScreen.qml` (real composition), `qt-app/quick/qml/AppShell.qml` (expose the window to the kiosk for RFID install), `qt-app/quick/main.cpp` (cache-first branding at startup)
- Create: `docs/superpowers/proofs/2026-07-11-loams2-phase2-kiosk-parity.md`
- Modify: `qt-app/quick/tests/tst_qml_kiosk.qml` (a `KioskScreen` smoke case with a real `KioskViewModel`)

**Interfaces:**
- Consumes everything from Tasks 3–9: `KioskViewModel`, `GuestViewModel`, `BrandPanel`, `KioskMain`, `GuestDialog`, `LToast`, `Navigator`.

Steps:

- [ ] **Add cache-first branding to `qt-app/quick/main.cpp`** so the kiosk renders with the cached brand palette from the first frame (mirrors `mainwindow.cpp:159-162`; live background re-sync + logo re-theme are Phase 4). Insert before the engine loads, after the `--software` block:
  ```cpp
  #include <QSettings>
  #include "brandtheme.h"
  // ...
      // Cache-first branding (spec §6): apply the cached palette before the first
      // frame so there is no unbranded flash. Background re-sync lands in Phase 4.
      {
          QSettings brandingStore(QStringLiteral("MyCompany"), QStringLiteral("MyApp"));
          BrandTheme::setCurrent(BrandTheme::loadCachedConfig(brandingStore).palette);
      }
  ```

- [ ] **Expose the window to the kiosk in `qt-app/quick/qml/AppShell.qml`** by passing it into `KioskScreen`. Change the kiosk `Component` to hand the window down:
  ```qml
      Component {
          id: kioskSurface
          KioskScreen { appWindow: appShell }
      }
  ```

- [ ] **Rewrite `qt-app/quick/qml/kiosk/KioskScreen.qml`** — the real composition:
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  Rectangle {
      id: screen
      property var appWindow          // the ApplicationWindow, for RFID install
      color: Theme.appBackground

      KioskViewModel {
          id: kioskVm
          onAdminRequested: Navigator.showAdmin()
          onGuestRequested: guestDialog.show()
          onStatusChanged: if (statusMessage.length > 0) statusToast.message = statusMessage
      }
      GuestViewModel { id: guestVm }

      // Install the RFID filter on the real window once it exists.
      Component.onCompleted: if (screen.appWindow) kioskVm.installRfid(screen.appWindow)

      // Responsive split: side-by-side on wide screens, stacked when narrow.
      GridLayout {
          anchors.fill: parent
          columns: screen.width < 980 ? 1 : 2
          columnSpacing: 0; rowSpacing: 0
          BrandPanel {
              id: brand
              vm: kioskVm
              Layout.fillHeight: true
              Layout.preferredWidth: screen.width < 980 ? screen.width : 390
              Layout.fillWidth: screen.width < 980
          }
          KioskMain {
              vm: kioskVm
              Layout.fillWidth: true
              Layout.fillHeight: true
          }
      }

      // Transient status toast (bottom-center), fed by the VM.
      LToast {
          id: statusToast
          severity: kioskVm.statusSeverity
          anchors.horizontalCenter: parent.horizontalCenter
          anchors.bottom: parent.bottom
          anchors.bottomMargin: Theme.spacing.xxxl
      }

      GuestDialog {
          id: guestDialog
          vm: guestVm
          anchors.centerIn: parent
          visible: false
      }
      Connections {
          target: guestVm
          function onGuestSucceeded(message) { statusToast.severity = "Success"; statusToast.message = message }
          function onGuestFailed(message) { statusToast.severity = "Error"; statusToast.message = message }
      }
  }
  ```

- [ ] **Add a `KioskScreen` smoke case to `qt-app/quick/tests/tst_qml_kiosk.qml`** — instantiate the real screen with a real `KioskViewModel` (no window, so RFID install is skipped) and assert it loads and shows the idle empty-state without QML warnings:
  ```qml
      KioskScreen { id: realScreen; width: 1280; height: 800 }
      TestCase {
          name: "KioskScreenSmoke"
          when: windowShown
          function test_loadsIdleState() {
              verify(realScreen !== null);
              // No student yet -> hero shows the idle prompt path (hasStudent false).
              verify(realScreen.width === 1280);
          }
      }
  ```
  (This is a load/smoke assertion; behavioral parity is the human gate below.)

- [ ] **Build the whole thing and run the full suite.**
  ```
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Expected: `22 tests passed` (all green; `tst_qml_kiosk` now also covers `KioskScreen`). No new QML warnings.

- [ ] **Manual smoke test — run the kiosk and drive it (GUI app; a clean build is necessary but not sufficient):**
  ```
  qt-app/build/quick/WITSQuick.exe
  ```
  Verify against the legacy kiosk behaviors:
  - Brand panel: logo circle, "Library Attendance", school name·address, pulsing scan eyebrow, ID field, gold "LOG IN →", clock ticking every second, date + "Open <hours>".
  - Type a numeric ID + Enter → hero shows "NOW SIGNED IN" with the student, a fresh gold row prepends to the feed, stat tiles bump.
  - Type an admin key + Enter → surface switches to the Admin placeholder (`Navigator.showAdmin()`).
  - An invalid/empty submit → error toast, auto-dismisses (~2.5 s).
  - Guest button (if `kiosk/guestEnabled` is set true in `QSettings`) → dialog opens; missing required field → error; success → dialog closes + success toast.
  - Also run `qt-app/build/quick/WITSQuick.exe --software` and confirm it still renders (deployment fallback intact from Phase 1).
  - Watch the console for **zero QML warnings** (run with `QT_LOGGING_RULES="qt.qml.binding.removal.info=false"` off; observe `qml:` lines). In particular confirm no `ApplicationWindow → QQuickWindow*` conversion warning fires from `kioskVm.installRfid(screen.appWindow)` (review Round 1, Low) — if it does, change `installRfid` to accept a `QQuickItem*`/`QObject*` and cast, or pass `appShell` typed explicitly.
  - RFID: with the reader attached, a scan on the active kiosk window logs the student exactly as the legacy app (the terminating Enter is consumed — no stray submit). If no reader is available, note it and defer that line to on-device.
  - Confirm the legacy `WITS.exe` still builds and its kiosk still works (`cmake --build qt-app/build --target WITS`; run it) — the RFID base refactor (Task 5) must not have regressed it.

- [ ] **Write the parity checklist `docs/superpowers/proofs/2026-07-11-loams2-phase2-kiosk-parity.md`.** Enumerate every legacy kiosk behavior (from `mainwindow.cpp`/`guestwindow.cpp`) and check each against the new screen — at minimum:
  - Student-ID login (numeric → `student_login.php`), admin-key login (non-numeric → `admin_login.php` → admin surface), RFID login (`rfid_login.php`, 2.5 s same-card debounce, Enter consumed), guest login (`guest_login.php`, required fields).
  - Live feed prepend + newest-row highlight + cap; idle empty-state ("waiting for log in…"); "NOW SIGNED IN" hero; session stat tiles; 1 Hz clock + date + greeting bands; school name/address/hours from settings; cache-first branding (no unbranded flash).
  - **Intentional deviation (record, don't imply parity):** the feed caps at **40** rows (matching the design's scrollable feed), where the legacy kiosk capped its fixed 9-row spotlight panel at 9 (`mainwindow.cpp:273-274`). Note this explicitly as a deliberate design-feed change, not a silent parity match.
  - Deferred-with-reason (out of Phase 2 parity, tracked for later phases): student photo fetch/fade (Phase 4 asset work), logo image + poster background (Phase 4 admin logo import), live admin→kiosk updates (Phase 4 settings), real "visitors today/this hour" DB aggregate (Phase 3 dashboard — Phase 2 uses session counters), fullscreen deployment default.
  - Record each as PASS/DEFERRED with a note, plus a sign-off line (date + who ran it).

- [ ] **Run the Claude review gate.** Invoke `/claude-review` via the Skill tool (workflow §3), fix Critical/Important findings, resubmit until APPROVE (or 3 rounds). Then run the `create-pr` gate (dry-checker + security-reviewer + general-code-reviewer, diff-scoped) and address findings.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/qml/kiosk/KioskScreen.qml`, `qt-app/quick/qml/AppShell.qml`, `qt-app/quick/main.cpp`, `qt-app/quick/tests/tst_qml_kiosk.qml`, `docs/superpowers/proofs/2026-07-11-loams2-phase2-kiosk-parity.md`. Suggested message: `feat(kiosk): assemble KioskScreen, install RFID, cache-first branding; Phase 2 parity`.

---

## Deferred to later phases (explicitly out of Phase 2 scope)

Recorded so nothing silently drops. These are tracked but NOT done this phase:
- **Student photo** fetch + fade animation (`mainwindow.cpp:237-270`) and **logo image / poster background** rendering — Phase 4 (admin logo import + asset handling); Phase 2 uses a logo-circle placeholder and the flat brand gradient.
- **Live admin → kiosk updates** (school info / logo / poster / clear-attendance / guest-toggle signals) — Phase 4 (admin settings surface). Phase 2 reads these from `QSettings` at startup only.
- **Real "visitors today / this hour"** backed by a DB aggregate endpoint — Phase 3 (dashboard). Phase 2 derives them as session counters (as the design demo does).
- **Background branding re-sync + live logo re-theme** — Phase 4. Phase 2 does cache-first startup only.
- **Fullscreen-by-default for deployment** — set at deploy/launch time; keep windowed in dev for now (note in the render-check when it lands).
- **Dark mode + full a11y pass** — Phase 5.
- **Backend hardening** (parameterize queries in `student_login.php`/`rfid_login.php`/`guest_login.php`; remove `phpinfo.php`/`testupload.php`; `apiconfig.h` base URL) — Phase 6, contract-tests-first.

## Self-Review (completed against spec §7 + roadmap Phase 2)

- **Spec coverage:** §7 Kiosk elements — brand panel (Task 7), live feed with row motion (Task 8), "now signed in" hero + stat tiles (Task 8), guest flow behind the toggle (Task 9), "RFID scan behaves exactly as today" (Tasks 4–6, wired Task 10). Login parity — student/admin/RFID (Tasks 4, 6, 10). Every Phase-1 deferred follow-up is placed: `wits_add_qttest` + qmllint + CLAUDE.md (Task 1), `ThemeViewModel` single-source + `motion.toastHold` + token consolidation (Task 2), `RfidEventFilterBase` + `QPointer` (Task 5), RFID charset validation (Task 4, consumed Task 6).
- **Type consistency:** `applyStudentLogin(QJsonObject)`, `RecentLoginsModel::prepend(...)` roles (`NameRole…FreshRole`), `LoginParser::classify/parseLoginResponse/parseRfidResponse/isValidRfidCode/shouldDebounceRfid`, `Navigator::Surface{Kiosk,Admin}`, and the `KioskViewModel` property names are used identically across the tasks that define and consume them.
- **No placeholders:** every code step carries complete, runnable code; every CMake edit shows the exact lines; every command has an expected result. Component-library extension (`LDialog`) is scoped and verified against `tst_qml_components`.
- **Ordering:** friction removal (Tasks 1–2) precedes the subsystem; pure logic (Task 4) precedes its VM consumer (Task 6); the RFID base (Task 5) precedes its install (Task 10); screens (7–9) precede assembly (10). Suite count rises 17 → 22 with no regressions.

## Review gate

This plan passed a design-spec `/claude-review` (fresh-context Opus, validated against the real codebase) — **Verdict: APPROVE, Round 1**. Two Important implementation gaps it flagged were folded in before hand-off: (1) `KioskViewModel.cpp` now includes `<QQuickWindow>` (complete type for `installEventFilter`, Task 6); (2) `LDialog` is redefined as a self-contained overlay with a content slot and `GuestDialog` no longer shadows popup `open()/close()`, protecting `tst_appshell`'s zero-warning gate (Task 9). Low forward-notes (feed-cap deviation; `installRfid` window coercion smoke-check) are recorded inline in Tasks 10.
