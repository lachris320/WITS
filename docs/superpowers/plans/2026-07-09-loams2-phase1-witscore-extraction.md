# LOAMS 2.0 Phase 1 — witscore Extraction + Qt Quick Shell + RFID Spike Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Each `- [ ]` is ONE action (write a failing test / run it & watch it fail / minimal implementation / run it & watch it pass / commit). Show your work; do not batch unrelated edits into one commit.

**Goal:** Land Phase 1 of the LOAMS 2.0 Qt Quick modernization (spec §9, proposal §20): (a) extract the Widgets-free business logic into a `witscore` static library by `git mv` + CMake path updates (behavior unchanged, 12/12 ctest stays green, both apps build); (b) stand up a minimal Qt Quick executable `WITSQuick` with an `AppShell.qml` window; (c) de-risk **RFID-under-QML** by reusing `RfidScanDetector` verbatim behind a re-authored `RfidQuickFilter` gated on `QQuickWindow` focus; (d) document + wire the **software-rendering fallback** for the deployment-hardware render check; (e) stand up `ThemeViewModel` + the `Theme.qml` singleton and the ten `L*` component skeletons. Nothing user-facing ships this phase — the deliverable is the gate (proposal §20 Phase 1).

**Architecture:** `qt-app/core/` becomes the `witscore` static library (`add_library(witscore STATIC ...)`) holding the five controllers + their `*data.h`, `reportrenderer`, the brand engine (`brandcolormath.h`, `brandthemedata.h`, `brandtheme.*`, `brandingcontroller.*`), `apiconfig.h`, and the `RfidScanDetector` state machine. `witscore` links `Qt::Charts`/`Qt::Widgets`/`QXlsx` **PUBLIC** (option (a), proposal §7.1 — the accepted `reportrenderer` leak). The legacy Widgets app `WITS` and the new Qt Quick app `WITSQuick` both link `witscore`. `WITSQuick`'s QML lives in one `qt_add_qml_module` (`URI LOAMS`) library `witsquick`; MVVM layering means **ViewModels are the only QML-facing C++** — Phase 1 introduces exactly one (`ThemeViewModel`) plus one non-VM QObject bridge (`RfidQuickFilter`). `Theme.qml` (a `pragma Singleton`) is the single source of every visual token, backed 1:1 by `ThemeViewModel` for brand roles and holding structural/motion scales as literals.

**Tech Stack:** Qt 6.11.1 MinGW (`C:\Qt\6.11.1\mingw_64`) — components `Widgets Network Charts Test UiTools PrintSupport Svg` (existing) + `Qml Quick QuickControls2 QuickTest` (new); vendored QXlsx; C++17; CMake + Ninja + MinGW (`C:\Qt\Tools\mingw1310_64`, `C:\Qt\Tools\CMake_64`, `C:\Qt\Tools\Ninja`); Qt Test + Qt Quick Test via ctest.

## Global Constraints

- **Design is LOCKED** (spec §2's 8 decisions). Elaborate within; do not relitigate.
- **`-DBUILD_LEGACY_WIDGETS=ON` (default ON)** keeps the legacy `WITS` Widgets app building/runnable as the rollback for all of Phases 1–5; it is deleted only at Phase 6. The `WITS` target is wrapped in `if(BUILD_LEGACY_WIDGETS)`.
- **The 12 existing ctest targets keep their exact shape** — each still direct-compiles its class-under-test `.cpp`; the ONLY change is a path prefix (`${CMAKE_SOURCE_DIR}/reportcontroller.cpp` → `${CMAKE_SOURCE_DIR}/core/reportcontroller.cpp`) and adding `${CMAKE_SOURCE_DIR}/core` to each target's include dirs. Do NOT relink any test target to `witscore`. `tst_reportcontroller` must still omit `reportrenderer.cpp`.
- The core move is **`git mv` + include-path handling, NOT a rewrite** — moved files' behavior is unchanged. No edits to the moved `.h`/`.cpp` bodies.
- `witscore` dependency envelope = **option (a)**: one `add_library(witscore STATIC ...)` whose `target_link_libraries` makes `Qt::Charts`/`Qt::Widgets`/`QXlsx` **PUBLIC** (the accepted `reportrenderer` leak, §7.1; splitting into `witscore_reports` is a Phase-6 candidate, NOT now). `target_include_directories(witscore PUBLIC <core dir>)` so existing `#include "settingscontroller.h"` etc. keep resolving in the legacy sources without editing every include. Grounded fact: `brandtheme.cpp:18` includes `theme.h`, which **stays at the legacy root**, so `witscore` also needs `PRIVATE ${CMAKE_SOURCE_DIR}` on its include path. `AUTOMOC` stays ON (6 `Q_OBJECT` classes need moc); `AUTOUIC OFF` on `witscore` (no `.ui` inputs); `resources.qrc` stays with legacy `WITS` only.
- **ViewModels are the only QML-facing C++** (no QML calls a controller directly).
- **RFID:** `RfidScanDetector` (`rfidscandetector.h/.cpp`, the pure `feedKey(QChar, qint64) -> std::optional<QString>` machine at `rfidscandetector.h:20`) is REUSED VERBATIM (moves into `core/`). `RfidKeyboardFilter` is NOT moved — it stays a legacy-Widgets-only file. Its replacement `RfidQuickFilter` is a **new file under `quick/`** that re-authors the `QApplication::activeWindow()`/`focusWidget()` gates (`rfidkeyboardfilter.cpp:25,33-36`) as `QQuickWindow` equivalents.
- Names must match the proposal exactly: `core/` contents per §8; the 8 ViewModels (KioskViewModel, DashboardViewModel, StudentsViewModel, VisitLogsViewModel, ReportsViewModel, SettingsViewModel, ThemeViewModel, Navigator) — Phase 1 builds only `ThemeViewModel`; the 10 L* components (LButton, LCard, LTable, LSegmented, LSideNav, LToast, LDialog, LStatTile, LBarChart, LPageHeader).
- **QtTest/Quick-test mechanics (this MinGW kit):** every test target needs `set_target_properties(<t> PROPERTIES WIN32_EXECUTABLE FALSE)` (console subsystem, or ctest captures zero output); every test sets ctest `ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"`; widget/GUI/Quick tests add `QT_QPA_PLATFORM=offscreen`.
- **No secrets / no PII / no personal paths** in source, tests, CMake, or commit messages (security-hygiene rule). Synthetic test data only (`"ABC1"`, `"2023-00123"`, `"BSIT"`). Use placeholders (`http://example.com/x.php`, `<ADMIN_KEY>`) — the shipped `apiconfig.h` `http://localhost/loams_api/` is pre-existing and moves unchanged.
- **Stage files by name** — never `git add -A`/`.`/`-u`. Commit via the `commit` skill. Never commit `qt-app/build/`, `*.user`, or `moc_*`/`ui_*`. **NEVER stage `qt-app/adminwindow.ui`** — it carries the user's own uncommitted edit. Markdown-only plan; the executing engineer writes the application code.
- Harmless warnings to ignore: `LF will be replaced by CRLF`; the pre-existing `QXlsx ... GuiPrivate target` warning.
- **Build/test commands (PowerShell — tools are NOT on PATH):**
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```

---

### Task 1: Extract the `witscore` static library

This is a **refactor**, so the "test" is the existing 12-target ctest suite as a regression gate: record a **12/12 baseline before** the move, then keep it 12/12 **after**. There is no new unit test — inventing one would be hollow. Order the steps so the tree never fails to build between checkpoints: move all files, write all three CMake changes, then verify green in one pass.

**Files:**
- Create: `qt-app/core/CMakeLists.txt`
- Move (`git mv`, bodies unchanged) from `qt-app/` → `qt-app/core/`: `apiconfig.h`; `settingsdata.h`, `settingscontroller.h`, `settingscontroller.cpp`; `visitordata.h`, `visitorcontroller.h`, `visitorcontroller.cpp`; `importdata.h`, `importcontroller.h`, `importcontroller.cpp`; `studentdata.h`, `studentcontroller.h`, `studentcontroller.cpp`; `reportdata.h`, `reportcontroller.h`, `reportcontroller.cpp`; `reportrenderer.h`, `reportrenderer.cpp`; `brandcolormath.h`; `brandthemedata.h`, `brandtheme.h`, `brandtheme.cpp`; `brandingcontroller.h`, `brandingcontroller.cpp`; `rfidscandetector.h`, `rfidscandetector.cpp`
- Modify: `qt-app/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`
- Do NOT move / stays at root: `main.cpp`, `mainwindow.*`, `adminwindow.*`, `guestwindow.*`, `attachfilesdialog.*`, `attachFilesDialog.ui`, `busyindicator.*`, `rfidkeyboardfilter.*`, `theme.h`, `resources.qrc`, `resources/wits.qss`, the four `.ui` files.
- Note on `apiconfig.h`: it is header-only and was **never** a listed source in `qt_add_executable(WITS ...)` (the committed `PROJECT_SOURCES` never named it). So its relocation into `core/` is a pure **file move**, not a removal of a compiled source from `WITS`. The `core/` PUBLIC include dir keeps `tst_apiconfig` and every other `#include "apiconfig.h"` consumer resolving it unchanged.

**Interfaces:**
- Produces: CMake target `witscore` (STATIC) with `PUBLIC` include dir `qt-app/core` and `PUBLIC` link set `Qt::Core Qt::Network Qt::Gui Qt::Charts Qt::Widgets Qt::Svg QXlsx`.
- Consumes (unchanged public C++ API): `SettingsController`, `VisitorController`, `ImportController`, `StudentController`, `ReportController`, `ReportRenderer`, `BrandTheme` (free functions), `BrandingController`, `ApiConfig`, `RfidScanDetector` — all resolved by consumers via the `witscore` PUBLIC include dir.

Steps:

- [ ] **Record the baseline.** Configure + build + run the full suite and confirm 12/12 pass BEFORE touching anything, so the regression gate is real:
  ```
  $env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;" + $env:PATH
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Expected: `100% tests passed, 0 tests failed out of 12`. Note the 12 names (`tst_rfidscandetector`, `tst_rfidkeyboardfilter`, `tst_apiconfig`, `tst_theme`, `tst_responsive_ui`, `tst_visitorcontroller`, `tst_importcontroller`, `tst_studentcontroller`, `tst_reportcontroller`, `tst_reportrenderer`, `tst_brandtheme`, `tst_brandingcontroller`).

- [ ] **Create the `core/` directory and `git mv` every core file into it** (preserves history; bodies untouched). Run from `qt-app/`:
  ```
  mkdir -p core
  git mv apiconfig.h core/
  git mv settingsdata.h settingscontroller.h settingscontroller.cpp core/
  git mv visitordata.h visitorcontroller.h visitorcontroller.cpp core/
  git mv importdata.h importcontroller.h importcontroller.cpp core/
  git mv studentdata.h studentcontroller.h studentcontroller.cpp core/
  git mv reportdata.h reportcontroller.h reportcontroller.cpp core/
  git mv reportrenderer.h reportrenderer.cpp core/
  git mv brandcolormath.h core/
  git mv brandthemedata.h brandtheme.h brandtheme.cpp core/
  git mv brandingcontroller.h brandingcontroller.cpp core/
  git mv rfidscandetector.h rfidscandetector.cpp core/
  ```

- [ ] **Write `qt-app/core/CMakeLists.txt`** — the new static library. Note the `PRIVATE ${CMAKE_SOURCE_DIR}` include: `brandtheme.cpp:18` includes `theme.h`, which stays at the legacy root. `Qt::Svg` is needed by `brandtheme.cpp` (QSvgRenderer); `Qt::Charts`/`Qt::Widgets`/`QXlsx` are the accepted `reportrenderer` leak (PUBLIC). No `Qt::PrintSupport` — grep confirms no core file uses `QPrinter`.
  ```cmake
  # witscore — Widgets-free (mostly) business logic shared by the legacy WITS
  # Widgets app and the new WITSQuick Qt Quick app. Option (a) dependency
  # envelope (proposal §7.1): reportrenderer's Qt::Charts + Qt::Widgets + QXlsx
  # link leaks PUBLIC into every consumer — an accepted Phase 1–5 trade-off,
  # a Phase-6 split candidate (witscore_reports), not a defect to fix now.

  add_library(witscore STATIC
      apiconfig.h
      settingsdata.h
      settingscontroller.h settingscontroller.cpp
      visitordata.h
      visitorcontroller.h visitorcontroller.cpp
      importdata.h
      importcontroller.h importcontroller.cpp
      studentdata.h
      studentcontroller.h studentcontroller.cpp
      reportdata.h
      reportcontroller.h reportcontroller.cpp
      reportrenderer.h reportrenderer.cpp
      brandcolormath.h
      brandthemedata.h
      brandtheme.h brandtheme.cpp
      brandingcontroller.h brandingcontroller.cpp
      rfidscandetector.h rfidscandetector.cpp
  )

  # AUTOMOC required (6 Q_OBJECT classes: the five *Controller + BrandingController)
  # and free — it is already ON at directory scope. AUTOUIC OFF: zero .ui inputs.
  set_target_properties(witscore PROPERTIES
      AUTOMOC ON
      AUTOUIC OFF
  )

  target_link_libraries(witscore PUBLIC
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Charts
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::Svg
      QXlsx
  )

  target_include_directories(witscore
      PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}   # core/ — consumers resolve "settingscontroller.h" etc.
      PRIVATE ${CMAKE_SOURCE_DIR}           # qt-app/ root — brandtheme.cpp includes "theme.h"
  )
  ```

- [ ] **Rewrite `qt-app/CMakeLists.txt`** — introduce `witscore`, add the `BUILD_LEGACY_WIDGETS` option, shrink the `WITS` source list to the genuinely Widgets-bound files, and link `witscore`. `add_subdirectory(libs/QXlsx)` moves ABOVE `add_subdirectory(core)` so the `QXlsx` target exists when `witscore` references it. Full replacement:
  ```cmake
  cmake_minimum_required(VERSION 3.16)

  project(WITS VERSION 0.1 LANGUAGES CXX)

  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Rollback gate (proposal §7.6 / spec §2 Strategy A): keeps the legacy Widgets
  # app building until Phase 6 declares parity. Default ON for all of Phases 1–5.
  option(BUILD_LEGACY_WIDGETS "Build the legacy WITS Widgets executable" ON)

  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Charts Test UiTools PrintSupport Svg Qml Quick QuickControls2 QuickTest)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Charts Test UiTools PrintSupport Svg Qml Quick QuickControls2 QuickTest)

  # Enable CTest BEFORE any add_subdirectory that registers tests. CMake only
  # honours add_test()/set_tests_properties() in a subdirectory if enable_testing()
  # ran in an ancestor scope FIRST; otherwise the add_test() calls inside quick/
  # (and tests/) register nothing and ctest silently reports only the 12 legacy
  # tests. This must precede core/, quick/, and tests/ below.
  enable_testing()

  # --- Add QXlsx (must precede witscore, which links it) ---
  add_subdirectory(libs/QXlsx)

  # --- Shared business-logic core (static library) ---
  add_subdirectory(core)

  # --- New Qt Quick app (WITSQuick) + its QML module ---
  add_subdirectory(quick)

  # --- Legacy Widgets app (rollback until Phase 6) ---
  if(BUILD_LEGACY_WIDGETS)
      set(PROJECT_SOURCES
          main.cpp
          mainwindow.cpp
          mainwindow.h
          mainwindow.ui
          adminwindow.cpp
          adminwindow.h
          adminwindow.ui
          rfidkeyboardfilter.cpp
          rfidkeyboardfilter.h
          resources.qrc
      )

      if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
          qt_add_executable(WITS
              MANUAL_FINALIZATION
              ${PROJECT_SOURCES}
              guestwindow.h guestwindow.cpp guestwindow.ui
              busyindicator.h busyindicator.cpp
              attachFilesDialog.ui
              attachfilesdialog.h attachfilesdialog.cpp
          )
      else()
          if(ANDROID)
              add_library(WITS SHARED ${PROJECT_SOURCES})
          else()
              add_executable(WITS ${PROJECT_SOURCES})
          endif()
      endif()

      target_link_libraries(WITS PRIVATE
          witscore
          Qt${QT_VERSION_MAJOR}::Widgets
          Qt${QT_VERSION_MAJOR}::Network
          Qt${QT_VERSION_MAJOR}::Charts
          Qt${QT_VERSION_MAJOR}::PrintSupport
          Qt${QT_VERSION_MAJOR}::Svg
          QXlsx
      )

      if(${QT_VERSION} VERSION_LESS 6.1.0)
        set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.WITS)
      endif()
      set_target_properties(WITS PROPERTIES
          ${BUNDLE_ID_OPTION}
          MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
          MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
          MACOSX_BUNDLE TRUE
          WIN32_EXECUTABLE TRUE
      )

      include(GNUInstallDirs)
      install(TARGETS WITS
          BUNDLE DESTINATION .
          LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
          RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      )

      if(QT_VERSION_MAJOR EQUAL 6)
          qt_finalize_executable(WITS)
      endif()
  endif()

  add_subdirectory(tests)
  ```
  Note: `add_subdirectory(quick)` is referenced here but its `CMakeLists.txt` is created in Task 2. To keep the tree building after THIS step, create a one-line placeholder `qt-app/quick/CMakeLists.txt` containing only `# WITSQuick — populated in Task 2` for now, or defer adding the `add_subdirectory(quick)` line until Task 2. Prefer the placeholder so the top-level file is written once.

- [ ] **Create the placeholder `qt-app/quick/CMakeLists.txt`** so the configure in the next step succeeds:
  ```cmake
  # WITSQuick + witsquick QML module — populated in Task 2.
  ```

- [ ] **Rewrite `qt-app/tests/CMakeLists.txt`** — every moved `.cpp`/`.h` gains the `core/` prefix and every target's include dirs gain `${CMAKE_SOURCE_DIR}/core` (kept alongside `${CMAKE_SOURCE_DIR}` so `theme.h`/`.ui` at the root still resolve — required by `tst_theme`, `tst_responsive_ui`, and the two brand tests that transitively compile `brandtheme.cpp`). Shape is otherwise IDENTICAL: each target still direct-compiles its class-under-test `.cpp`; `tst_reportcontroller` still omits `reportrenderer.cpp`; no target links `witscore`. Full replacement:
  ```cmake
  qt_add_executable(tst_rfidscandetector
      tst_rfidscandetector.cpp
      ${CMAKE_SOURCE_DIR}/core/rfidscandetector.cpp
      ${CMAKE_SOURCE_DIR}/core/rfidscandetector.h
  )
  set_target_properties(tst_rfidscandetector PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_rfidscandetector PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_rfidscandetector PRIVATE Qt${QT_VERSION_MAJOR}::Test)
  add_test(NAME tst_rfidscandetector COMMAND tst_rfidscandetector)
  set_tests_properties(tst_rfidscandetector PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
  )

  qt_add_executable(tst_rfidkeyboardfilter
      tst_rfidkeyboardfilter.cpp
      ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.cpp
      ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.h
      ${CMAKE_SOURCE_DIR}/core/rfidscandetector.cpp
      ${CMAKE_SOURCE_DIR}/core/rfidscandetector.h
  )
  set_target_properties(tst_rfidkeyboardfilter PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_rfidkeyboardfilter PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_rfidkeyboardfilter PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
  add_test(NAME tst_rfidkeyboardfilter COMMAND tst_rfidkeyboardfilter)
  set_tests_properties(tst_rfidkeyboardfilter PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
  )

  qt_add_executable(tst_apiconfig
      tst_apiconfig.cpp
  )
  set_target_properties(tst_apiconfig PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_apiconfig PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_apiconfig PRIVATE Qt${QT_VERSION_MAJOR}::Test)
  add_test(NAME tst_apiconfig COMMAND tst_apiconfig)
  set_tests_properties(tst_apiconfig PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
  )

  qt_add_executable(tst_theme
      tst_theme.cpp
  )
  set_target_properties(tst_theme PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_theme PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_compile_definitions(tst_theme PRIVATE WITS_QSS_PATH="${CMAKE_SOURCE_DIR}/resources/wits.qss")
  target_link_libraries(tst_theme PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
  add_test(NAME tst_theme COMMAND tst_theme)
  set_tests_properties(tst_theme PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
  )

  qt_add_executable(tst_responsive_ui
      tst_responsive_ui.cpp
  )
  set_target_properties(tst_responsive_ui PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_responsive_ui PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_compile_definitions(tst_responsive_ui PRIVATE WITS_UI_DIR="${CMAKE_SOURCE_DIR}")
  target_link_libraries(tst_responsive_ui PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::UiTools
  )
  add_test(NAME tst_responsive_ui COMMAND tst_responsive_ui)
  set_tests_properties(tst_responsive_ui PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
  )

  qt_add_executable(tst_visitorcontroller
      tst_visitorcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/visitorcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/visitorcontroller.h
      ${CMAKE_SOURCE_DIR}/core/visitordata.h
  )
  set_target_properties(tst_visitorcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_visitorcontroller PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_visitorcontroller PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Network
      QXlsx
  )
  add_test(NAME tst_visitorcontroller COMMAND tst_visitorcontroller)
  set_tests_properties(tst_visitorcontroller PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )

  qt_add_executable(tst_importcontroller
      tst_importcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/importcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/importcontroller.h
      ${CMAKE_SOURCE_DIR}/core/importdata.h
  )
  set_target_properties(tst_importcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_importcontroller PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_importcontroller PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Network
      QXlsx
  )
  add_test(NAME tst_importcontroller COMMAND tst_importcontroller)
  set_tests_properties(tst_importcontroller PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )

  qt_add_executable(tst_studentcontroller
      tst_studentcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/studentcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/studentcontroller.h
      ${CMAKE_SOURCE_DIR}/core/studentdata.h
  )
  set_target_properties(tst_studentcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_studentcontroller PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_studentcontroller PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Network
  )
  add_test(NAME tst_studentcontroller COMMAND tst_studentcontroller)
  set_tests_properties(tst_studentcontroller PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
  )

  # 9th — Core+Network+Gui, NO offscreen. Still omits reportrenderer.cpp.
  qt_add_executable(tst_reportcontroller
      tst_reportcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/reportcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/reportcontroller.h
      ${CMAKE_SOURCE_DIR}/core/reportdata.h
  )
  set_target_properties(tst_reportcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_reportcontroller PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_reportcontroller PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Gui
  )
  add_test(NAME tst_reportcontroller COMMAND tst_reportcontroller)
  set_tests_properties(tst_reportcontroller PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
  )

  # 10th — Gui+Charts+Widgets+QXlsx, offscreen. Does not link reportcontroller.cpp.
  qt_add_executable(tst_reportrenderer
      tst_reportrenderer.cpp
      ${CMAKE_SOURCE_DIR}/core/reportrenderer.cpp
      ${CMAKE_SOURCE_DIR}/core/reportrenderer.h
      ${CMAKE_SOURCE_DIR}/core/reportdata.h
  )
  set_target_properties(tst_reportrenderer PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_reportrenderer PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_reportrenderer PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Charts
      Qt${QT_VERSION_MAJOR}::Widgets
      QXlsx
  )
  add_test(NAME tst_reportrenderer COMMAND tst_reportrenderer)
  set_tests_properties(tst_reportrenderer PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )

  # 11th — brand-theme engine. Compiles brandtheme.cpp (which includes root theme.h),
  # so it keeps BOTH ${CMAKE_SOURCE_DIR} and ${CMAKE_SOURCE_DIR}/core on its include path.
  qt_add_executable(tst_brandtheme
      tst_brandtheme.cpp
      ${CMAKE_SOURCE_DIR}/core/brandtheme.cpp
      ${CMAKE_SOURCE_DIR}/core/brandtheme.h
      ${CMAKE_SOURCE_DIR}/core/brandthemedata.h
      ${CMAKE_SOURCE_DIR}/core/brandcolormath.h
  )
  set_target_properties(tst_brandtheme PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_brandtheme PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_brandtheme PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Svg
  )
  add_test(NAME tst_brandtheme COMMAND tst_brandtheme)
  set_tests_properties(tst_brandtheme PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )

  # 12th — branding network controller. Compiles brandtheme.cpp too (root theme.h).
  qt_add_executable(tst_brandingcontroller
      tst_brandingcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/brandingcontroller.cpp
      ${CMAKE_SOURCE_DIR}/core/brandingcontroller.h
      ${CMAKE_SOURCE_DIR}/core/brandtheme.cpp
      ${CMAKE_SOURCE_DIR}/core/brandtheme.h
      ${CMAKE_SOURCE_DIR}/core/brandthemedata.h
  )
  set_target_properties(tst_brandingcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
  target_include_directories(tst_brandingcontroller PRIVATE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/core)
  target_link_libraries(tst_brandingcontroller PRIVATE
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Svg
  )
  add_test(NAME tst_brandingcontroller COMMAND tst_brandingcontroller)
  set_tests_properties(tst_brandingcontroller PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )
  ```

- [ ] **Re-configure from a clean build dir** (the move + subdirectory restructure changes the generated tree; a stale cache can mis-resolve the moved paths):
  ```
  rm -rf qt-app/build
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  ```
  Expected: configures with no errors; the `QXlsx ... GuiPrivate target` warning is pre-existing and ignorable.

- [ ] **Build.** `cmake --build qt-app/build` — expected: `witscore`, `WITS`, and all 12 test targets compile and link with no new warnings/errors.

- [ ] **Run the regression gate — the "green after" half.** `ctest --test-dir qt-app/build --output-on-failure` — expected: `100% tests passed, 0 tests failed out of 12`. This is the whole verification for Task 1: same 12 tests, same shape, still green after the move.

- [ ] **Smoke-test the legacy app still launches** (a clean build is necessary but not sufficient for a GUI app): run `qt-app/build/WITS.exe`, confirm the kiosk window appears, close it. This proves the `witscore` link did not change `WITS`'s runtime behavior.

- [ ] **Commit** via the `commit` skill. Stage by name: the 26 moved files, `qt-app/core/CMakeLists.txt`, `qt-app/quick/CMakeLists.txt` (placeholder), `qt-app/CMakeLists.txt`, `qt-app/tests/CMakeLists.txt`. Do NOT stage `qt-app/adminwindow.ui` or `qt-app/build/`. Suggested message: `refactor: extract Widgets-free core into witscore static library`.

---

### Task 2: Minimal Qt Quick executable + AppShell

Stand up `WITSQuick` and an empty `AppShell.qml`. The verification is a C++ QtTest that loads the module's `AppShell` through the production path (`QQmlApplicationEngine::loadFromModule`) and asserts one root object with zero QML warnings — this is the honest "the shell loads" test (red first: it fails to compile/link before the module exists).

**Files:**
- Create: `qt-app/quick/main.cpp`, `qt-app/quick/qml/AppShell.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (replace the Task 1 placeholder)
- Create: `qt-app/quick/tests/tst_appshell.cpp`, and register it in `qt-app/quick/CMakeLists.txt`

**Interfaces:**
- Produces: QML module `witsquick` (`URI LOAMS`, `VERSION 1.0`) as a STATIC library exposing type `AppShell`; executable `WITSQuick` linking it.
- Consumes: `witscore` (linked PUBLIC by `witsquick`); `Qt::Quick`, `Qt::Qml`, `Qt::QuickControls2`.

Steps:

- [ ] **Write the failing load test `qt-app/quick/tests/tst_appshell.cpp`** first — it references the `LOAMS` module and `AppShell`, which do not exist yet, so it will fail to build (the red step):
  ```cpp
  #include <QtTest>
  #include <QGuiApplication>
  #include <QQmlApplicationEngine>
  #include <QtGlobal>
  #include <vector>

  // Capture QML warnings so a load that "succeeds" but logs binding/type
  // warnings still fails the test (premium-shell discipline: zero warnings).
  static std::vector<QString> g_messages;
  static void captureHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
  {
      if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
          g_messages.push_back(msg);
  }

  class TestAppShell : public QObject
  {
      Q_OBJECT
  private slots:
      void loadsWithZeroWarnings();
  };

  void TestAppShell::loadsWithZeroWarnings()
  {
      g_messages.clear();
      QtMessageHandler prev = qInstallMessageHandler(captureHandler);

      QQmlApplicationEngine engine;
      engine.loadFromModule("LOAMS", "AppShell");

      qInstallMessageHandler(prev);

      QCOMPARE(engine.rootObjects().size(), 1);
      if (!g_messages.empty())
          qWarning() << "Unexpected QML messages:" << g_messages;
      QVERIFY(g_messages.empty());
  }

  QTEST_MAIN(TestAppShell)
  #include "tst_appshell.moc"
  ```

- [ ] **Write `qt-app/quick/qml/AppShell.qml`** — the minimal visible window (proposal §8: "window, surface switching, navigation host"; Phase 1 = empty shell only):
  ```qml
  import QtQuick
  import QtQuick.Controls

  ApplicationWindow {
      id: appShell
      width: 1280
      height: 800
      visible: true
      title: qsTr("LOAMS 2.0")
  }
  ```

- [ ] **Write `qt-app/quick/main.cpp`** — the production entry point using `loadFromModule` (the same path the test exercises):
  ```cpp
  #include <QGuiApplication>
  #include <QQmlApplicationEngine>

  int main(int argc, char *argv[])
  {
      QGuiApplication app(argc, argv);

      QQmlApplicationEngine engine;
      QObject::connect(
          &engine, &QQmlApplicationEngine::objectCreationFailed,
          &app, []() { QCoreApplication::exit(-1); },
          Qt::QueuedConnection);
      engine.loadFromModule("LOAMS", "AppShell");
      if (engine.rootObjects().isEmpty())
          return -1;

      return app.exec();
  }
  ```

- [ ] **Replace `qt-app/quick/CMakeLists.txt`** — define the `witsquick` QML module (STATIC so tests can link it), the `WITSQuick` executable, and register the load test:
  ```cmake
  qt_standard_project_setup(REQUIRES 6.5)

  # The QML module as a STATIC library so both WITSQuick and the Quick/QtTest
  # targets can link it and get its generated qmldir + embedded resources.
  qt_add_library(witsquick STATIC)

  qt_add_qml_module(witsquick
      URI LOAMS
      VERSION 1.0
      QML_FILES
          qml/AppShell.qml
  )

  target_link_libraries(witsquick PUBLIC
      witscore
      Qt${QT_VERSION_MAJOR}::Quick
      Qt${QT_VERSION_MAJOR}::Qml
      Qt${QT_VERSION_MAJOR}::QuickControls2
      Qt${QT_VERSION_MAJOR}::Gui
  )

  qt_add_executable(WITSQuick main.cpp)
  target_link_libraries(WITSQuick PRIVATE witsquick)
  set_target_properties(WITSQuick PROPERTIES WIN32_EXECUTABLE TRUE)

  # --- AppShell load test (C++ QtTest, offscreen) ---
  qt_add_executable(tst_appshell tests/tst_appshell.cpp)
  set_target_properties(tst_appshell PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(tst_appshell PRIVATE
      witsquick
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Qml
      Qt${QT_VERSION_MAJOR}::Quick
  )
  add_test(NAME tst_appshell COMMAND tst_appshell)
  set_tests_properties(tst_appshell PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
  )
  ```
  Note: linking the STATIC `witsquick` module into `tst_appshell`/`WITSQuick` triggers Qt's automatic QML plugin import (CMake ≥ 6.2), so `loadFromModule("LOAMS", "AppShell")` resolves the statically-embedded module with no manual import-path setup.

- [ ] **Confirm Qt Quick is present in the kit** (verify-not-doubt — proposal environment facts already confirm `Qt6Qml`/`Qt6Quick`/`Qt6QuickControls2`): re-configure and watch the `find_package(... Qml Quick QuickControls2 QuickTest)` line succeed:
  ```
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64"
  ```
  Expected: configures with no "Could NOT find Qt6Qml/Quick/QuickControls2/QuickTest" errors.

- [ ] **Build and watch the test go red→green.** `cmake --build qt-app/build` builds `witsquick`, `WITSQuick`, and `tst_appshell`. Run `ctest --test-dir qt-app/build --output-on-failure -R tst_appshell` — expected PASS (one root object, zero QML warnings). Then the full `ctest` — expected `13 tests passed`.

- [ ] **Manual smoke test:** run `qt-app/build/quick/WITSQuick.exe` — an empty 1280×800 window titled "LOAMS 2.0" appears. Close it. (With Ninja single-config and no `RUNTIME_OUTPUT_DIRECTORY` override, the binary lands in the target's build dir, `qt-app/build/quick/`.)

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/main.cpp`, `qt-app/quick/qml/AppShell.qml`, `qt-app/quick/CMakeLists.txt`, `qt-app/quick/tests/tst_appshell.cpp`. Suggested message: `feat: add minimal WITSQuick Qt Quick executable with AppShell`.

---

### Task 3: RFID-in-QML spike (`RfidQuickFilter`)

Re-author the install/focus layer against `QQuickWindow` semantics while reusing `RfidScanDetector::feedKey` verbatim from `core/`. `RfidScanDetector`'s own logic is already covered by `tst_rfidscandetector`; this task's new test covers the `QQuickWindow` wiring — the burst→scan-token path and terminator consumption through the new filter. TDD mirrors the existing `tst_rfidkeyboardfilter` idiom (a `PublicFilter` subclass calling `eventFilter` directly), with a protected `gateOpen()` seam so the test can force the active-window gate open under the offscreen QPA (where window activation is not deliverable).

**Files:**
- Create: `qt-app/quick/RfidQuickFilter.h`, `qt-app/quick/RfidQuickFilter.cpp`
- Create: `qt-app/quick/tests/tst_rfidquickfilter.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add `RfidQuickFilter` to the `witsquick` module `SOURCES` + register the test)

**Interfaces:**
- Produces: `class RfidQuickFilter : public QObject` — ctor `RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent = nullptr)`; `signals: void rfidScanned(const QString &code)`; `static bool isTerminator(int key)`; `protected: bool eventFilter(QObject *, QEvent *) override` and `virtual bool gateOpen(QObject *watched) const`.
- Consumes: `RfidScanDetector::feedKey(QChar, qint64) -> std::optional<QString>` and `RfidScanDetector::reset()` (from `core/rfidscandetector.h:20`), reused verbatim.

Steps:

- [ ] **Write the failing test `qt-app/quick/tests/tst_rfidquickfilter.cpp`** first (red — `RfidQuickFilter` does not exist). Mirrors `tst_rfidkeyboardfilter.cpp`: a subclass promotes `eventFilter` to public and forces `gateOpen()` true so the burst is deterministic offscreen:
  ```cpp
  #include <QtTest>
  #include <QGuiApplication>
  #include <QKeyEvent>
  #include <QQuickWindow>
  #include <QSignalSpy>
  #include "RfidQuickFilter.h"

  // Promotes eventFilter to public and forces the active-window gate open so
  // the burst can be driven synthetically under the offscreen QPA (which does
  // not deliver real window activation). This mirrors tst_rfidkeyboardfilter's
  // PublicFilter idiom (tst_rfidkeyboardfilter.cpp:11-20).
  class PublicQuickFilter : public RfidQuickFilter
  {
  public:
      using RfidQuickFilter::RfidQuickFilter;
      bool callEventFilter(QObject *watched, QEvent *event)
      {
          return eventFilter(watched, event);
      }
  protected:
      bool gateOpen(QObject *) const override { return m_forceOpen; }
  public:
      bool m_forceOpen = true;
  };

  class TestRfidQuickFilter : public QObject
  {
      Q_OBJECT
  private slots:
      void terminatorIsRecognized();
      void recognizedScanEmitsAndConsumesEnter();
      void closedGateIgnoresScan();
      void printableKeyNotConsumed();
  };

  static bool feedPrintable(PublicQuickFilter &f, QQuickWindow &w,
                            Qt::Key key, const QString &text)
  {
      QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier, text);
      return f.callEventFilter(&w, &ev);
  }

  static bool feedTerminator(PublicQuickFilter &f, QQuickWindow &w)
  {
      QKeyEvent term(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier,
                     QStringLiteral("\r"));
      return f.callEventFilter(&w, &term);
  }

  void TestRfidQuickFilter::terminatorIsRecognized()
  {
      QVERIFY(RfidQuickFilter::isTerminator(Qt::Key_Return));
      QVERIFY(RfidQuickFilter::isTerminator(Qt::Key_Enter));
      QVERIFY(!RfidQuickFilter::isTerminator(Qt::Key_A));
  }

  void TestRfidQuickFilter::recognizedScanEmitsAndConsumesEnter()
  {
      QQuickWindow win;
      PublicQuickFilter filter(&win);
      QSignalSpy spy(&filter, &RfidQuickFilter::rfidScanned);

      QCOMPARE(feedPrintable(filter, win, Qt::Key_A, QStringLiteral("A")), false);
      QCOMPARE(feedPrintable(filter, win, Qt::Key_B, QStringLiteral("B")), false);
      QCOMPARE(feedPrintable(filter, win, Qt::Key_C, QStringLiteral("C")), false);
      QCOMPARE(feedPrintable(filter, win, Qt::Key_1, QStringLiteral("1")), false);

      QVERIFY(feedTerminator(filter, win));           // consumed
      QCOMPARE(spy.count(), 1);
      QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("ABC1"));
  }

  void TestRfidQuickFilter::closedGateIgnoresScan()
  {
      QQuickWindow win;
      PublicQuickFilter filter(&win);
      filter.m_forceOpen = false;                     // window not active
      QSignalSpy spy(&filter, &RfidQuickFilter::rfidScanned);

      feedPrintable(filter, win, Qt::Key_A, QStringLiteral("A"));
      feedPrintable(filter, win, Qt::Key_B, QStringLiteral("B"));
      feedPrintable(filter, win, Qt::Key_C, QStringLiteral("C"));
      QVERIFY(!feedTerminator(filter, win));          // not consumed
      QCOMPARE(spy.count(), 0);
  }

  void TestRfidQuickFilter::printableKeyNotConsumed()
  {
      QQuickWindow win;
      PublicQuickFilter filter(&win);
      QVERIFY(!feedPrintable(filter, win, Qt::Key_X, QStringLiteral("X")));
  }

  QTEST_MAIN(TestRfidQuickFilter)
  #include "tst_rfidquickfilter.moc"
  ```

- [ ] **Write `qt-app/quick/RfidQuickFilter.h`** — the QObject bridge (re-authored gates; contrast `rfidkeyboardfilter.h`):
  ```cpp
  #ifndef RFIDQUICKFILTER_H
  #define RFIDQUICKFILTER_H

  #include <QObject>
  #include <QElapsedTimer>
  #include "rfidscandetector.h"

  class QQuickWindow;

  // Quick-side replacement for RfidKeyboardFilter. Reuses the RfidScanDetector
  // state machine verbatim; the QApplication::activeWindow()/focusWidget() gates
  // (rfidkeyboardfilter.cpp:25,33-36) are re-authored as QQuickWindow focus
  // semantics: Qt Quick delivers key events to the QQuickWindow itself (a
  // QWindow), so this filter installs on the specific login window and gates on
  // that window being the active window — there is no qApp-global ancestor
  // double-delivery to de-duplicate.
  class RfidQuickFilter : public QObject
  {
      Q_OBJECT
  public:
      explicit RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent = nullptr);

      static bool isTerminator(int key);

  signals:
      void rfidScanned(const QString &code);

  protected:
      bool eventFilter(QObject *watched, QEvent *event) override;

      // Seam: production gate is "watched is the login window AND it is active".
      // Tests override to force it open under the offscreen QPA.
      virtual bool gateOpen(QObject *watched) const;

  private:
      QQuickWindow *m_loginWindow;
      RfidScanDetector m_detector;
      QElapsedTimer m_clock;
  };

  #endif // RFIDQUICKFILTER_H
  ```

- [ ] **Write `qt-app/quick/RfidQuickFilter.cpp`** — the minimal implementation (structurally faithful to `rfidkeyboardfilter.cpp` minus the ancestor de-dup, which QQuickWindow delivery makes unnecessary):
  ```cpp
  #include "RfidQuickFilter.h"

  #include <QQuickWindow>
  #include <QKeyEvent>

  RfidQuickFilter::RfidQuickFilter(QQuickWindow *loginWindow, QObject *parent)
      : QObject(parent)
      , m_loginWindow(loginWindow)
  {
      m_clock.start();
  }

  bool RfidQuickFilter::isTerminator(int key)
  {
      return key == Qt::Key_Return || key == Qt::Key_Enter;
  }

  bool RfidQuickFilter::gateOpen(QObject *watched) const
  {
      return watched == m_loginWindow
          && m_loginWindow != nullptr
          && m_loginWindow->isActive();
  }

  bool RfidQuickFilter::eventFilter(QObject *watched, QEvent *event)
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
              return true; // consume Enter so no default activation fires
          }
          return QObject::eventFilter(watched, event);
      }

      const QString text = ke->text();
      if (text.isEmpty()) // modifiers / function keys
          return QObject::eventFilter(watched, event);

      m_detector.feedKey(text.at(0), ts);
      return QObject::eventFilter(watched, event); // do NOT consume printable keys
  }
  ```

- [ ] **Wire `RfidQuickFilter` into `qt-app/quick/CMakeLists.txt`.** Add it to the module `SOURCES` (so it is compiled into `witsquick`) and register the test. Change the `qt_add_qml_module(witsquick ...)` call to include a `SOURCES` block, and append the test target:
  ```cmake
  qt_add_qml_module(witsquick
      URI LOAMS
      VERSION 1.0
      QML_FILES
          qml/AppShell.qml
      SOURCES
          RfidQuickFilter.h RfidQuickFilter.cpp
  )
  ```
  Then expose the `quick/` root on `witsquick`'s PUBLIC include path so the test can find `RfidQuickFilter.h` (see the note below for why this is required, analogous to Task 5's `viewmodels/` include line):
  ```cmake
  target_include_directories(witsquick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  ```
  and append:
  ```cmake
  # --- RFID-in-QML spike test (C++ QtTest, offscreen) ---
  qt_add_executable(tst_rfidquickfilter tests/tst_rfidquickfilter.cpp)
  set_target_properties(tst_rfidquickfilter PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(tst_rfidquickfilter PRIVATE
      witsquick
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Quick
  )
  add_test(NAME tst_rfidquickfilter COMMAND tst_rfidquickfilter)
  set_tests_properties(tst_rfidquickfilter PROPERTIES
      ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
  )
  ```
  Note: two distinct include-path resolutions are in play here.
  1. `RfidQuickFilter.h` includes `"rfidscandetector.h"` (from `core/`). This already resolves transitively — `witsquick` links `witscore` PUBLIC, whose PUBLIC include dir is `core/`. No extra line needed for this one.
  2. `tst_rfidquickfilter.cpp` (in `quick/tests/`) does `#include "RfidQuickFilter.h"`, but `RfidQuickFilter.h` lives at the `quick/` root. Quote-includes search the including file's own directory (`quick/tests/`) then the `-I` paths; the `quick/` root is on neither — `witscore`'s PUBLIC include is `core/`, not `quick/`. So the test target would fail to compile. The `target_include_directories(witsquick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})` line above puts the `quick/` root on `witsquick`'s PUBLIC include path, and because `tst_rfidquickfilter` links `witsquick` it inherits it and `#include "RfidQuickFilter.h"` resolves. This is the analogue of Task 5's `viewmodels/` include line for `ThemeViewModel.h`.

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then `ctest --test-dir qt-app/build --output-on-failure -R tst_rfidquickfilter` — expected PASS. Full `ctest` — expected `14 tests passed`.

- [ ] **Document the manual demo note** (for the Phase 1 proof, Task 7): the automated test proves the detector wiring + terminator consumption; the real-window activation gate (`QQuickWindow::isActive()`) is exercised by the on-device kiosk walkthrough in Phase 2. Record in the Task 7 proof doc that R1 ("RFID under QML focus", proposal §19/spec §10 Risk 1) is de-risked at the wiring level in Phase 1, with observable-behavior sign-off deferred to the Phase 2 kiosk parity checklist.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/RfidQuickFilter.h`, `qt-app/quick/RfidQuickFilter.cpp`, `qt-app/quick/tests/tst_rfidquickfilter.cpp`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat: add RfidQuickFilter reusing RfidScanDetector under QQuickWindow`.

---

### Task 4: Deployment-hardware render check + software-rendering fallback

Wire the software scene-graph backend toggle into `WITSQuick` (`--software` flag or `QT_QUICK_BACKEND=software`), then write the on-device checklist. This task's **sign-off is a human gate on the real library PC — it cannot be run from CI, and this plan says so plainly.** The codeable part is the fallback path in `main.cpp`; there is no automated unit test for GPU/rendering-backend selection (a hollow test would assert nothing meaningful), so the "test" here is the documented on-device procedure + a build check.

**Files:**
- Modify: `qt-app/quick/main.cpp`
- Create: `docs/superpowers/proofs/2026-07-09-loams2-phase1-render-check.md` (the on-device checklist; assembled into the Task 7 proof)

**Interfaces:**
- Produces: a launch-time branch in `main()` that calls `QQuickWindow::setGraphicsApi(QSGRendererInterface::Software)` before the first window is created when `--software` is passed or `QT_QUICK_BACKEND=software` is set.
- Consumes: `QQuickWindow::setGraphicsApi`, `QSGRendererInterface::Software`, `QCoreApplication::arguments()`, `qEnvironmentVariable`.

Steps:

- [ ] **Edit `qt-app/quick/main.cpp`** to add the fallback branch. `setGraphicsApi` must be called after `QGuiApplication` is constructed (so `arguments()` is populated) and before the engine loads any `QQuickWindow`:
  ```cpp
  #include <QGuiApplication>
  #include <QQmlApplicationEngine>
  #include <QQuickWindow>
  #include <QSGRendererInterface>

  int main(int argc, char *argv[])
  {
      QGuiApplication app(argc, argv);

      // Deployment-hardware fallback (proposal §19/spec §10 Risk 4): the library
      // PC may lack a working OpenGL/RHI path. "--software" or
      // QT_QUICK_BACKEND=software forces the software rasterizer scene-graph
      // backend. Must precede the first QQuickWindow creation.
      const QStringList args = QCoreApplication::arguments();
      if (args.contains(QStringLiteral("--software"))
          || qEnvironmentVariable("QT_QUICK_BACKEND") == QLatin1String("software")) {
          QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
      }

      QQmlApplicationEngine engine;
      QObject::connect(
          &engine, &QQmlApplicationEngine::objectCreationFailed,
          &app, []() { QCoreApplication::exit(-1); },
          Qt::QueuedConnection);
      engine.loadFromModule("LOAMS", "AppShell");
      if (engine.rootObjects().isEmpty())
          return -1;

      return app.exec();
  }
  ```

- [ ] **Build and verify both backend paths launch on the dev machine** (a proxy check before the on-device gate). `cmake --build qt-app/build`, then:
  ```
  qt-app/build/quick/WITSQuick.exe                 # default (hardware/RHI) — window appears
  qt-app/build/quick/WITSQuick.exe --software      # forced software backend — window appears
  ```
  Both must show the empty AppShell window. Re-run full `ctest` — still `14 tests passed` (the `tst_appshell` load test is unaffected).

- [ ] **Write `docs/superpowers/proofs/2026-07-09-loams2-phase1-render-check.md`** — the on-device checklist to be executed on the actual library PC (human gate). Include, at minimum:
  - Environment captured: Windows version, GPU/driver, whether an RHI backend initializes (run with `QSG_INFO=1` and record the chosen backend line).
  - Default-backend launch result (window renders, no freeze) — PASS/FAIL + notes.
  - `--software` launch result — PASS/FAIL + notes.
  - Decision recorded: does the library PC need `--software` as the default launch mode? If yes, note that the deployment shortcut/launcher should pass `--software` (or set `QT_QUICK_BACKEND=software`).
  - Sign-off line: date + who ran it on the real hardware. Mark this as the R4 retirement evidence for Task 7.
  - Use synthetic/no PII; no machine-local `C:\Users\<name>` paths — describe the deploy PC generically.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/main.cpp` and `docs/superpowers/proofs/2026-07-09-loams2-phase1-render-check.md`. Suggested message: `feat: add software-rendering fallback and deployment render-check checklist`.

---

### Task 5: `ThemeViewModel` + `Theme.qml` singleton

`ThemeViewModel` wraps the free functions `BrandTheme::current()`/`setCurrent()` (`brandtheme.h:67-68`) **without modifying `BrandTheme`** — it exposes each `BrandPalette` role as a read-only `QColor Q_PROPERTY` (`NOTIFY changed`), a `refresh()` to re-notify QML after an external `setCurrent`, and `regenerateFromImportedLogo(path)` (the live-re-theme hook, §13.2). `Theme.qml` is a `pragma Singleton` backed by a `ThemeViewModel` instance, mirroring brand roles and adding the structural/motion token scales as literals (the one allowed place for hex, §12.8). Full cache-first `BrandingController` startup sync (§13.2/13.3) is deferred to Phase 2's kiosk where the branding network path runs; Phase 1's `ThemeViewModel` reads the engine's process-wide default (`current()` defaults to `fallbackPalette()`).

**Files:**
- Create: `qt-app/quick/viewmodels/ThemeViewModel.h`, `qt-app/quick/viewmodels/ThemeViewModel.cpp`
- Create: `qt-app/quick/qml/theme/Theme.qml`
- Create: `qt-app/quick/tests/tst_themeviewmodel.cpp`
- Modify: `qt-app/quick/CMakeLists.txt` (add VM to module `SOURCES`, add `Theme.qml` to `QML_FILES`, register the test; add `find_package` already covers `QuickTest`)

**Interfaces:**
- Produces: `class ThemeViewModel : public QObject` with `QML_ELEMENT`; `Q_PROPERTY(QColor <role> READ <role> NOTIFY changed)` for all 17 `BrandPalette` roles (`brandthemedata.h:19-40`); `Q_INVOKABLE void refresh()`; `Q_INVOKABLE bool regenerateFromImportedLogo(const QString &path)`; `signals: void changed()`. QML singleton type `Theme` (module `LOAMS`).
- Consumes: `BrandTheme::current()`, `BrandTheme::setCurrent()`, `BrandTheme::regenerateFromLogo(BrandingConfig&, const QString&, QString*)`, `BrandingConfig` / `BrandPalette` (`brandthemedata.h`).

Steps:

- [ ] **Write the failing unit test `qt-app/quick/tests/tst_themeviewmodel.cpp`** first (red — `ThemeViewModel` does not exist). Tests the role mapping + `changed()` notify on an external `setCurrent`, and the logo-regeneration hook via a synthetic solid PNG (no network — mirrors `tst_brandtheme`'s image-writing idiom):
  ```cpp
  #include <QtTest>
  #include <QColor>
  #include <QImage>
  #include <QSignalSpy>
  #include <QTemporaryDir>
  #include "ThemeViewModel.h"
  #include "brandtheme.h"
  #include "brandthemedata.h"

  class TestThemeViewModel : public QObject
  {
      Q_OBJECT
  private slots:
      void mapsCurrentBrandRole();
      void refreshEmitsChangedAfterExternalSetCurrent();
      void regenerateFromLogoRethemesAndNotifies();

  private:
      QString writeSolidPng(const QString &path, const QColor &fill);
  };

  QString TestThemeViewModel::writeSolidPng(const QString &path, const QColor &fill)
  {
      QImage img(48, 48, QImage::Format_ARGB32);
      img.fill(fill);
      img.save(path, "PNG");
      return path;
  }

  void TestThemeViewModel::mapsCurrentBrandRole()
  {
      BrandTheme::setCurrent(BrandTheme::fallbackPalette());
      ThemeViewModel vm;
      QCOMPARE(vm.adminPrimary(), BrandTheme::current().adminPrimary);
  }

  void TestThemeViewModel::refreshEmitsChangedAfterExternalSetCurrent()
  {
      BrandTheme::setCurrent(BrandTheme::fallbackPalette());
      ThemeViewModel vm;
      QSignalSpy spy(&vm, &ThemeViewModel::changed);

      BrandPalette custom = BrandTheme::fallbackPalette();
      custom.adminPrimary = QColor(0x12, 0x34, 0x56);
      BrandTheme::setCurrent(custom);

      vm.refresh();
      QCOMPARE(spy.count(), 1);
      QCOMPARE(vm.adminPrimary(), QColor(0x12, 0x34, 0x56));
  }

  void TestThemeViewModel::regenerateFromLogoRethemesAndNotifies()
  {
      BrandTheme::setCurrent(BrandTheme::fallbackPalette());
      ThemeViewModel vm;
      QSignalSpy spy(&vm, &ThemeViewModel::changed);

      QTemporaryDir dir;
      QVERIFY(dir.isValid());
      const QString logo = writeSolidPng(dir.filePath("logo.png"), QColor(0x2E, 0x86, 0xC1));

      QVERIFY(vm.regenerateFromImportedLogo(logo));   // Auto mode -> re-extracts
      QCOMPARE(spy.count(), 1);
      // A chromatic logo yields a branded admin role distinct from the fallback.
      QVERIFY(vm.adminPrimary() != BrandTheme::fallbackPalette().adminPrimary);
  }

  QTEST_MAIN(TestThemeViewModel)
  #include "tst_themeviewmodel.moc"
  ```

- [ ] **Write `qt-app/quick/viewmodels/ThemeViewModel.h`** — one `QColor` property per `BrandPalette` role (`brandthemedata.h:19-40`), all `NOTIFY changed`:
  ```cpp
  #ifndef THEMEVIEWMODEL_H
  #define THEMEVIEWMODEL_H

  #include <QObject>
  #include <QColor>
  #include <qqml.h>
  #include "brandthemedata.h"

  // QML-facing wrapper over the free-function brand engine (brandtheme.h:67-68).
  // Does NOT modify BrandTheme: it only calls the engine's public API and emits
  // changed() so QML property bindings re-evaluate (BrandTheme is deliberately
  // not a QObject). Exposes every BrandPalette role as a read-only QColor.
  class ThemeViewModel : public QObject
  {
      Q_OBJECT
      QML_ELEMENT

      Q_PROPERTY(QColor adminPrimary      READ adminPrimary      NOTIFY changed)
      Q_PROPERTY(QColor adminPrimaryHover READ adminPrimaryHover NOTIFY changed)
      Q_PROPERTY(QColor adminOnPrimary    READ adminOnPrimary    NOTIFY changed)
      Q_PROPERTY(QColor adminPrimarySoft  READ adminPrimarySoft  NOTIFY changed)
      Q_PROPERTY(QColor kioskPrimary      READ kioskPrimary      NOTIFY changed)
      Q_PROPERTY(QColor kioskPrimaryHover READ kioskPrimaryHover NOTIFY changed)
      Q_PROPERTY(QColor kioskOnPrimary    READ kioskOnPrimary    NOTIFY changed)
      Q_PROPERTY(QColor kioskPrimarySoft  READ kioskPrimarySoft  NOTIFY changed)
      Q_PROPERTY(QColor secondary         READ secondary         NOTIFY changed)
      Q_PROPERTY(QColor sidebarBase       READ sidebarBase       NOTIFY changed)
      Q_PROPERTY(QColor card              READ card              NOTIFY changed)
      Q_PROPERTY(QColor appBackground     READ appBackground     NOTIFY changed)
      Q_PROPERTY(QColor border            READ border            NOTIFY changed)
      Q_PROPERTY(QColor text              READ text              NOTIFY changed)
      Q_PROPERTY(QColor mutedText         READ mutedText         NOTIFY changed)
      Q_PROPERTY(QColor success           READ success           NOTIFY changed)
      Q_PROPERTY(QColor error             READ error             NOTIFY changed)

  public:
      explicit ThemeViewModel(QObject *parent = nullptr);

      QColor adminPrimary() const      { return BrandTheme::current().adminPrimary; }
      QColor adminPrimaryHover() const { return BrandTheme::current().adminPrimaryHover; }
      QColor adminOnPrimary() const    { return BrandTheme::current().adminOnPrimary; }
      QColor adminPrimarySoft() const  { return BrandTheme::current().adminPrimarySoft; }
      QColor kioskPrimary() const      { return BrandTheme::current().kioskPrimary; }
      QColor kioskPrimaryHover() const { return BrandTheme::current().kioskPrimaryHover; }
      QColor kioskOnPrimary() const    { return BrandTheme::current().kioskOnPrimary; }
      QColor kioskPrimarySoft() const  { return BrandTheme::current().kioskPrimarySoft; }
      QColor secondary() const         { return BrandTheme::current().secondary; }
      QColor sidebarBase() const       { return BrandTheme::current().sidebarBase; }
      QColor card() const              { return BrandTheme::current().card; }
      QColor appBackground() const     { return BrandTheme::current().appBackground; }
      QColor border() const            { return BrandTheme::current().border; }
      QColor text() const              { return BrandTheme::current().text; }
      QColor mutedText() const         { return BrandTheme::current().mutedText; }
      QColor success() const           { return BrandTheme::current().success; }
      QColor error() const             { return BrandTheme::current().error; }

      // Re-notify QML after an external BrandTheme::setCurrent (e.g. a remote
      // branding config arriving in a later phase).
      Q_INVOKABLE void refresh();

      // Live re-theme hook (§13.2). Auto mode re-extracts from the logo, applies
      // it via BrandTheme::setCurrent, and emits changed(); Manual mode is a
      // no-op returning false (brandtheme.cpp:414-419). Returns success.
      Q_INVOKABLE bool regenerateFromImportedLogo(const QString &path);

  signals:
      void changed();

  private:
      BrandingConfig m_config;
  };

  #endif // THEMEVIEWMODEL_H
  ```
  Note the header includes `brandthemedata.h` for `BrandPalette`/`BrandingConfig`; the read accessors call `BrandTheme::current()` from `brandtheme.h`, so the `.h` also needs `#include "brandtheme.h"` — add it alongside `brandthemedata.h`.

- [ ] **Write `qt-app/quick/viewmodels/ThemeViewModel.cpp`**:
  ```cpp
  #include "ThemeViewModel.h"
  #include "brandtheme.h"

  ThemeViewModel::ThemeViewModel(QObject *parent)
      : QObject(parent)
  {
      // Phase 1: read the engine's process-wide default (current() defaults to
      // fallbackPalette(), brandtheme.h:67). Cache-first BrandingController sync
      // (§13.2/13.3) lands in Phase 2 with the kiosk branding network path.
      m_config.palette = BrandTheme::current();
  }

  void ThemeViewModel::refresh()
  {
      emit changed();
  }

  bool ThemeViewModel::regenerateFromImportedLogo(const QString &path)
  {
      QString err;
      const bool ok = BrandTheme::regenerateFromLogo(m_config, path, &err);
      if (ok) {
          BrandTheme::setCurrent(m_config.palette);
          emit changed();
      }
      return ok;
  }
  ```

- [ ] **Write `qt-app/quick/qml/theme/Theme.qml`** — the `pragma Singleton` token surface. Brand roles bind to an internal `ThemeViewModel`; the extra design tokens, structural scales, and motion are literals (allowed only here, §12.8). Values grounded in proposal §12.1/§12.2/§12.3/§12.6/§12.7/§15:
  ```qml
  pragma Singleton
  import QtQuick
  import LOAMS

  QtObject {
      id: root

      // Brand roles come from the C++ engine via ThemeViewModel; live re-theme
      // works because these are property bindings that re-evaluate on changed().
      readonly property ThemeViewModel _vm: ThemeViewModel {}

      readonly property QtObject brand: QtObject {
          readonly property color admin:        root._vm.adminPrimary
          readonly property color adminHover:   root._vm.adminPrimaryHover
          readonly property color adminSoft:    root._vm.adminPrimarySoft
          readonly property color kiosk:        root._vm.kioskPrimary
          readonly property color kioskHover:   root._vm.kioskPrimaryHover
          readonly property color kioskSoft:    root._vm.kioskPrimarySoft
          readonly property color onPrimary:    root._vm.adminOnPrimary
      }

      readonly property color secondary:     root._vm.secondary
      readonly property color card:          root._vm.card
      readonly property color appBackground: root._vm.appBackground
      readonly property color border:        root._vm.border
      readonly property color text:          root._vm.text
      readonly property color mutedText:     root._vm.mutedText
      readonly property color success:       root._vm.success
      readonly property color error:         root._vm.error
      readonly property color sidebarBase:   root._vm.sidebarBase

      // Extra design tokens (no BrandPalette field — literals, §12.1).
      readonly property color secondarySoft:    "#FDF3E0"
      readonly property color mutedTextCaption: "#B0A08A"
      readonly property color tableHeaderBg:    "#F7F1E6"
      readonly property color rowHairline:      "#F3ECDD"
      readonly property color errorSoft:        "#FDF4F3"
      readonly property color errorBorder:      "#F3D9D6"

      // Structural scales (§12.2/12.3).
      readonly property QtObject spacing: QtObject {
          readonly property int xs: 4
          readonly property int sm: 8
          readonly property int md: 12
          readonly property int base: 14
          readonly property int lg: 16
          readonly property int xl: 18
          readonly property int xl2: 22
          readonly property int xxl: 24
          readonly property int xxxl: 28
      }
      readonly property QtObject radius: QtObject {
          readonly property int xs: 6
          readonly property int sm: 8
          readonly property int sm2: 10
          readonly property int md: 12
          readonly property int card: 16
          readonly property int hero: 18
          readonly property int pill: 999
      }
      // Typography (§12.6): family + weight + pixel size per role.
      readonly property QtObject typography: QtObject {
          readonly property string serif: "Source Serif 4"
          readonly property string sans:  "Public Sans"
          readonly property int pageTitle: 30
          readonly property int heroName: 27
          readonly property int cardTitle: 17
          readonly property int statValue: 34
          readonly property int body: 14
          readonly property int control: 13
          readonly property int eyebrow: 11
      }
      // Elevation (§12.7): shadow specs as data (a component maps these to a
      // DropShadow / layer effect). Kept as strings so no widget dep leaks in.
      readonly property QtObject elevation: QtObject {
          readonly property string resting: ""
          readonly property string hover:   "0 12px 26px rgba(94,14,11,0.10)"
          readonly property string heroFill: "0 12px 30px rgba(94,14,11,0.25)"
          readonly property string ctaGold: "0 5px 14px rgba(232,177,14,0.30)"
          readonly property string modal:   "0 24px 48px rgba(0,0,0,0.30)"
      }
      // Motion (§15): one shared easing for one-shot transitions + a reduce switch.
      readonly property QtObject motion: QtObject {
          readonly property bool enabled: true
          readonly property int pageIn: 400
          readonly property int rowIn: 400
          readonly property int cardIn: 450
          readonly property int barGrow: 600
          readonly property int deptBarFill: 800
          readonly property int hoverLift: 180
          readonly property int toastIn: 200
          readonly property int toastOut: 150
          readonly property var easing: [0.2, 0.8, 0.3, 1.0]
      }

      // Mode (§13.5) — Phase 1 defaults to light; full light/dark derivation is
      // a Phase-5 item (spec §10 Risk 3).
      readonly property string mode: "Light"
      readonly property bool isDark: false
  }
  ```

- [ ] **Write the failing Quick test `qt-app/quick/tests/tst_qml_theme.qml`** — asserts a Theme token resolves. Kept assertion robust: structural scales are exact constants; the brand color is only checked as a valid, opaque color (its exact fallback hex is engine-owned, not hardcoded here):
  ```qml
  import QtQuick
  import QtTest
  import LOAMS

  TestCase {
      name: "ThemeTokens"

      function test_spacingScaleExposed() {
          compare(Theme.spacing.lg, 16);
          compare(Theme.radius.card, 16);
      }

      function test_brandAdminResolvesToOpaqueColor() {
          verify(Theme.brand.admin.a === 1.0);
          verify(Theme.brand.admin !== Theme.card);
      }
  }
  ```

- [ ] **Add the Quick-test C++ harness `qt-app/quick/tests/tst_qml_theme.cpp`** — `QUICK_TEST_MAIN` pointed at the test-source dir. The `LOAMS` module (and its `ThemeViewModel`/`Theme` singleton) is linked in via `witsquick`; add the module import path so `import LOAMS` resolves:
  ```cpp
  #include <QtQuickTest/quicktest.h>
  #include <QQmlEngine>
  #include <QQmlContext>

  class Setup : public QObject
  {
      Q_OBJECT
  public:
      Setup() = default;

  public slots:
      void qmlEngineAvailable(QQmlEngine *engine)
      {
          // The statically-linked witsquick module embeds its qmldir under
          // qrc:/qt/qml; make it importable from the .qml test files.
          engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
      }
  };

  QUICK_TEST_MAIN_WITH_SETUP(tst_qml_theme, Setup)
  #include "tst_qml_theme.moc"
  ```

- [ ] **Update `qt-app/quick/CMakeLists.txt`** — add the VM to the module `SOURCES`, `Theme.qml` to `QML_FILES`, and register both tests. **`Theme.qml` is a `pragma Singleton`, and with `qt_add_qml_module` the `pragma` alone is NOT sufficient** — Qt 6 requires the source-file property `QT_QML_SINGLETON_TYPE TRUE` so the generated `qmldir` emits `singleton Theme` (without it, `import LOAMS; Theme.spacing.lg` fails to resolve the singleton). Set that property **before** `Theme.qml` is listed in the module:
  ```cmake
  set_source_files_properties(qml/theme/Theme.qml PROPERTIES
      QT_QML_SINGLETON_TYPE TRUE)
  ```
  Then the `qt_add_qml_module` becomes:
  ```cmake
  qt_add_qml_module(witsquick
      URI LOAMS
      VERSION 1.0
      QML_FILES
          qml/AppShell.qml
          qml/theme/Theme.qml
      SOURCES
          RfidQuickFilter.h RfidQuickFilter.cpp
          viewmodels/ThemeViewModel.h viewmodels/ThemeViewModel.cpp
  )
  ```
  Add a module include dir so `ThemeViewModel.h` (in `viewmodels/`) is found by `Theme.qml`'s generated registration and by tests including it:
  ```cmake
  target_include_directories(witsquick PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels)
  ```
  Append the two test targets:
  ```cmake
  # --- ThemeViewModel unit test (C++ QtTest, offscreen: paints QImages) ---
  qt_add_executable(tst_themeviewmodel tests/tst_themeviewmodel.cpp)
  set_target_properties(tst_themeviewmodel PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(tst_themeviewmodel PRIVATE
      witsquick
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Gui
      Qt${QT_VERSION_MAJOR}::Svg
  )
  add_test(NAME tst_themeviewmodel COMMAND tst_themeviewmodel)
  set_tests_properties(tst_themeviewmodel PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )

  # --- Theme singleton Quick test (offscreen) ---
  qt_add_executable(tst_qml_theme tests/tst_qml_theme.cpp)
  set_target_properties(tst_qml_theme PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(tst_qml_theme PRIVATE
      witsquick
      Qt${QT_VERSION_MAJOR}::QuickTest
      Qt${QT_VERSION_MAJOR}::Qml
      Qt${QT_VERSION_MAJOR}::Quick
  )
  target_compile_definitions(tst_qml_theme PRIVATE
      QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_test(NAME tst_qml_theme COMMAND tst_qml_theme)
  set_tests_properties(tst_qml_theme PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )
  ```
  Note: `QUICK_TEST_SOURCE_DIR` tells `QUICK_TEST_MAIN_WITH_SETUP` where to find `tst_qml_theme.qml`.

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then `ctest --test-dir qt-app/build --output-on-failure -R "tst_themeviewmodel|tst_qml_theme"` — both PASS. Full `ctest` — expected `16 tests passed`.

- [ ] **Commit** via the `commit` skill. Stage `qt-app/quick/viewmodels/ThemeViewModel.h`, `qt-app/quick/viewmodels/ThemeViewModel.cpp`, `qt-app/quick/qml/theme/Theme.qml`, `qt-app/quick/tests/tst_themeviewmodel.cpp`, `qt-app/quick/tests/tst_qml_theme.cpp`, `qt-app/quick/tests/tst_qml_theme.qml`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat: add ThemeViewModel and Theme.qml singleton backed by brand engine`.

---

### Task 6: The ten `L*` component skeletons

Create the ten design-system primitives (proposal §11), each binding `Theme` tokens (no local hardcoded colors, §12.8) and setting an `Accessible` role. Phase 1 delivers skeletons — enough structure to instantiate, bind `Theme`, and expose the key properties — not the full visual fidelity (that lands per-screen in Phases 2–4). A single Quick test instantiates each and asserts it creates without error and binds a `Theme` token.

**Files:**
- Create: `qt-app/quick/qml/components/LButton.qml`, `LCard.qml`, `LTable.qml`, `LSegmented.qml`, `LSideNav.qml`, `LToast.qml`, `LDialog.qml`, `LStatTile.qml`, `LBarChart.qml`, `LPageHeader.qml`
- Create: `qt-app/quick/tests/tst_qml_components.cpp`, `qt-app/quick/tests/tst_qml_components.qml`
- Modify: `qt-app/quick/CMakeLists.txt` (add the 10 `.qml` to `QML_FILES`, register the test)

**Interfaces:**
- Produces: QML types `LButton`, `LCard`, `LTable`, `LSegmented`, `LSideNav`, `LToast`, `LDialog`, `LStatTile`, `LBarChart`, `LPageHeader` in module `LOAMS`, each with the §11 key properties and `Accessible.role`.
- Consumes: `Theme.*` tokens (from Task 5).

Steps:

- [ ] **Write the failing Quick test `qt-app/quick/tests/tst_qml_components.qml`** first (red — the components do not exist). It instantiates each primitive and asserts creation + a `Theme` binding:
  ```qml
  import QtQuick
  import QtTest
  import LOAMS

  Item {
      id: host
      width: 400; height: 400

      LButton    { id: b;  text: "OK" }
      LCard      { id: c }
      LTable     { id: t }
      LSegmented { id: sg }
      LSideNav   { id: sn }
      LToast     { id: to; message: "hi" }
      LDialog    { id: dg; title: "T" }
      LStatTile  { id: st; label: "L"; value: "1" }
      LBarChart  { id: bc }
      LPageHeader{ id: ph; title: "P" }

      TestCase {
          name: "ComponentsInstantiateAndBindTheme"
          function test_allCreated() {
              verify(b !== null); verify(c !== null); verify(t !== null);
              verify(sg !== null); verify(sn !== null); verify(to !== null);
              verify(dg !== null); verify(st !== null); verify(bc !== null);
              verify(ph !== null);
          }
          function test_buttonBindsBrandToken() {
              // LButton Primary fill binds Theme.brand.admin — not a local literal.
              compare(b.fillColor, Theme.brand.admin);
          }
          function test_cardBindsCardToken() {
              compare(c.color, Theme.card);
          }
      }
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LButton.qml`** — variant-driven action primitive (§11 `LButton`). Exposes `fillColor` so the test can assert the brand binding:
  ```qml
  import QtQuick
  import QtQuick.Controls
  import LOAMS

  // The one clickable-action primitive (§11). variant selects the fill role;
  // colors come only from Theme (§12.8), never local literals.
  Button {
      id: control
      property string variant: "Primary"   // Primary | Accent | Outline | Danger | Ghost
      property bool compact: false
      readonly property color fillColor: variant === "Accent" ? Theme.secondary
                                       : variant === "Danger" ? Theme.error
                                       : Theme.brand.admin

      padding: compact ? Theme.spacing.sm : Theme.spacing.md
      font.family: Theme.typography.sans
      font.pixelSize: Theme.typography.control

      background: Rectangle {
          radius: Theme.radius.md
          color: control.variant === "Outline" || control.variant === "Ghost"
                 ? "transparent" : control.fillColor
          border.width: control.variant === "Outline" ? 2 : 0
          border.color: Theme.border
      }
      contentItem: Text {
          text: control.text
          color: control.variant === "Outline" || control.variant === "Ghost"
                 ? Theme.text : Theme.brand.onPrimary
          font: control.font
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
      }

      Accessible.role: Accessible.Button
      Accessible.name: control.text
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LCard.qml`** — the one bordered-container primitive (§11 `LCard`):
  ```qml
  import QtQuick
  import LOAMS

  // The one bordered-container primitive (§11).
  Rectangle {
      id: card
      property int padding: Theme.spacing.xl
      property bool filled: false
      property bool elevated: false
      default property alias content: body.data

      color: filled ? Theme.brand.admin : Theme.card
      radius: Theme.radius.card
      border.width: filled ? 0 : 2
      border.color: Theme.border
      implicitWidth: 160
      implicitHeight: 96

      Item {
          id: body
          anchors.fill: parent
          anchors.margins: card.padding
      }

      Accessible.role: Accessible.Grouping
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LTable.qml`** — row-grid primitive skeleton (§11 `LTable`):
  ```qml
  import QtQuick
  import QtQuick.Controls
  import LOAMS

  // Row-grid primitive (§11). Phase 1 skeleton: header tint + hairline rows;
  // full column/model wiring lands with Database/Visit Logs screens.
  Rectangle {
      id: table
      property var columns: []
      property var model: null
      property bool selectable: false
      property string emptyStateText: qsTr("No records")

      color: Theme.card
      radius: Theme.radius.card
      border.width: 2
      border.color: Theme.border
      implicitWidth: 320
      implicitHeight: 200

      Rectangle {
          id: header
          width: parent.width
          height: 36
          color: Theme.tableHeaderBg
          radius: Theme.radius.card
      }

      Accessible.role: Accessible.Table
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LSegmented.qml`** — N-way segmented toggle (§11 `LSegmented`):
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // N-way segmented toggle (§11) — e.g. the Today/This-Week visit-log control.
  Rectangle {
      id: seg
      property var options: []          // list of {value, label}
      property var currentValue: null
      signal selectionChanged(var value)

      color: Theme.card
      radius: Theme.radius.sm2
      border.width: 2
      border.color: Theme.border
      implicitHeight: 34
      implicitWidth: 200

      RowLayout {
          anchors.fill: parent
          anchors.margins: 2
          spacing: 2
          Repeater {
              model: seg.options
              delegate: Rectangle {
                  Layout.fillWidth: true
                  Layout.fillHeight: true
                  radius: Theme.radius.sm2
                  color: modelData.value === seg.currentValue ? Theme.brand.admin
                                                              : "transparent"
                  Text {
                      anchors.centerIn: parent
                      text: modelData.label
                      color: modelData.value === seg.currentValue ? Theme.brand.onPrimary
                                                                 : Theme.text
                      font.family: Theme.typography.sans
                      font.pixelSize: Theme.typography.control
                  }
                  MouseArea {
                      anchors.fill: parent
                      onClicked: {
                          seg.currentValue = modelData.value;
                          seg.selectionChanged(modelData.value);
                      }
                  }
                  Accessible.role: Accessible.PageTab
              }
          }
      }

      Accessible.role: Accessible.PageTabList
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LSideNav.qml`** — persistent left sidebar (§11 `LSideNav`; reads brand roles directly, §12.5):
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // Persistent left sidebar (§11). Brand-gradient background reads the brand
  // roles directly (§12.5), not Theme.sidebarBase.
  Rectangle {
      id: nav
      property var items: []            // bound to Navigator's registered pages (later)
      property string currentPage: ""
      property Item footer: null

      color: Theme.brand.admin
      implicitWidth: 240
      implicitHeight: 600

      ColumnLayout {
          anchors.fill: parent
          anchors.margins: Theme.spacing.lg
          spacing: Theme.spacing.sm
          Repeater {
              model: nav.items
              delegate: Text {
                  text: modelData.label !== undefined ? modelData.label : modelData
                  color: Theme.brand.onPrimary
                  font.family: Theme.typography.sans
                  font.pixelSize: Theme.typography.body
                  Accessible.role: Accessible.ListItem
              }
          }
      }

      Accessible.role: Accessible.List
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LToast.qml`** — inline non-blocking status (§11 `LToast`):
  ```qml
  import QtQuick
  import LOAMS

  // Inline, non-blocking status surface (§11). Assertive live-region semantics.
  Rectangle {
      id: toast
      property string message: ""
      property string severity: "Info"   // Info | Success | Warning | Error
      property int autoDismissMs: Theme.motion.toastIn

      visible: message.length > 0
      color: severity === "Error" ? Theme.errorSoft : Theme.card
      radius: Theme.radius.md
      border.width: 2
      border.color: severity === "Error" ? Theme.errorBorder : Theme.border
      implicitHeight: 40
      implicitWidth: contentText.implicitWidth + Theme.spacing.xxl

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

- [ ] **Write `qt-app/quick/qml/components/LDialog.qml`** — the one modal primitive (§11 `LDialog`):
  ```qml
  import QtQuick
  import QtQuick.Controls
  import LOAMS

  // The only modal-interruption primitive 2.0 keeps (§11).
  Dialog {
      id: dlg
      property string message: ""
      modal: true

      background: Rectangle {
          color: Theme.card
          radius: Theme.radius.card
          border.width: 2
          border.color: Theme.border
      }
      contentItem: Text {
          text: dlg.message
          color: Theme.text
          font.family: Theme.typography.sans
          font.pixelSize: Theme.typography.body
          wrapMode: Text.WordWrap
      }

      Accessible.role: Accessible.Dialog
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LStatTile.qml`** — label/value/caption stat card (§11 `LStatTile`):
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // Dashboard/kiosk stat card (§11): label + value + caption, Neutral or Hero.
  Rectangle {
      id: tile
      property string label: ""
      property string value: ""
      property string caption: ""
      property string variant: "Neutral"   // Neutral | Hero

      color: variant === "Hero" ? Theme.brand.admin : Theme.card
      radius: Theme.radius.card
      border.width: variant === "Hero" ? 0 : 2
      border.color: Theme.border
      implicitWidth: 200
      implicitHeight: 110

      ColumnLayout {
          anchors.fill: parent
          anchors.margins: Theme.spacing.xl
          spacing: Theme.spacing.xs
          Text {
              text: tile.label
              color: tile.variant === "Hero" ? Theme.brand.onPrimary : Theme.mutedTextCaption
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.eyebrow
          }
          Text {
              text: tile.value
              color: tile.variant === "Hero" ? Theme.brand.onPrimary : Theme.text
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.statValue
          }
          Text {
              visible: tile.caption.length > 0
              text: tile.caption
              color: Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.body
          }
      }

      Accessible.role: Accessible.Grouping
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LBarChart.qml`** — custom (non-QtCharts) bar primitive (§11 `LBarChart`; per-bar color from the model, structural tokens from Theme):
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // Custom bar visualization (§11) — NOT QtCharts (Risk 2). Per-bar color comes
  // from the model item; the component supplies only structural tokens.
  Item {
      id: chart
      property string orientation: "Vertical"   // Vertical | Horizontal
      property var model: []                     // list of {label, value, color}
      property real maxValue: 100
      property int barRadius: Theme.radius.xs

      implicitWidth: 320
      implicitHeight: 160

      RowLayout {
          anchors.fill: parent
          spacing: Theme.spacing.sm
          Repeater {
              model: chart.model
              delegate: Rectangle {
                  Layout.fillWidth: true
                  Layout.alignment: Qt.AlignBottom
                  height: chart.maxValue > 0
                          ? (parent.height * (modelData.value / chart.maxValue)) : 0
                  radius: chart.barRadius
                  color: modelData.color !== undefined ? modelData.color : Theme.brand.admin
                  Accessible.role: Accessible.StaticText
                  Accessible.name: (modelData.label !== undefined ? modelData.label : "")
                                   + " " + modelData.value
              }
          }
      }

      Accessible.role: Accessible.Pane
  }
  ```

- [ ] **Write `qt-app/quick/qml/components/LPageHeader.qml`** — admin page header (§11 `LPageHeader`):
  ```qml
  import QtQuick
  import QtQuick.Layouts
  import LOAMS

  // Admin page-header region (§11): title, subtitle, live clock.
  Item {
      id: hdr
      property string title: ""
      property string subtitle: ""
      property string clockText: ""
      property Item actions: null

      implicitHeight: 64
      implicitWidth: 480

      ColumnLayout {
          anchors.left: parent.left
          anchors.verticalCenter: parent.verticalCenter
          spacing: Theme.spacing.xs
          Text {
              text: hdr.title
              color: Theme.text
              font.family: Theme.typography.serif
              font.pixelSize: Theme.typography.pageTitle
              font.weight: Font.Bold
              Accessible.role: Accessible.StaticText
          }
          Text {
              visible: hdr.subtitle.length > 0 || hdr.clockText.length > 0
              text: hdr.clockText.length > 0 ? hdr.clockText : hdr.subtitle
              color: Theme.mutedText
              font.family: Theme.typography.sans
              font.pixelSize: Theme.typography.body
              Accessible.role: Accessible.StaticText
          }
      }
  }
  ```

- [ ] **Write the Quick-test harness `qt-app/quick/tests/tst_qml_components.cpp`** — same `QUICK_TEST_MAIN_WITH_SETUP` pattern as Task 5 (adds `qrc:/qt/qml` to the import path):
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

  QUICK_TEST_MAIN_WITH_SETUP(tst_qml_components, Setup)
  #include "tst_qml_components.moc"
  ```

- [ ] **Update `qt-app/quick/CMakeLists.txt`** — add all ten components to `QML_FILES` and register the components test. The `qt_add_qml_module` `QML_FILES` block becomes:
  ```cmake
      QML_FILES
          qml/AppShell.qml
          qml/theme/Theme.qml
          qml/components/LButton.qml
          qml/components/LCard.qml
          qml/components/LTable.qml
          qml/components/LSegmented.qml
          qml/components/LSideNav.qml
          qml/components/LToast.qml
          qml/components/LDialog.qml
          qml/components/LStatTile.qml
          qml/components/LBarChart.qml
          qml/components/LPageHeader.qml
  ```
  Append the test target:
  ```cmake
  # --- L* component skeletons Quick test (offscreen) ---
  qt_add_executable(tst_qml_components tests/tst_qml_components.cpp)
  set_target_properties(tst_qml_components PROPERTIES WIN32_EXECUTABLE FALSE)
  target_link_libraries(tst_qml_components PRIVATE
      witsquick
      Qt${QT_VERSION_MAJOR}::QuickTest
      Qt${QT_VERSION_MAJOR}::Qml
      Qt${QT_VERSION_MAJOR}::Quick
  )
  target_compile_definitions(tst_qml_components PRIVATE
      QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
  add_test(NAME tst_qml_components COMMAND tst_qml_components)
  set_tests_properties(tst_qml_components PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
  )
  ```

- [ ] **Build and watch red→green.** `cmake --build qt-app/build`, then `ctest --test-dir qt-app/build --output-on-failure -R tst_qml_components` — PASS (all ten instantiate, `LButton.fillColor == Theme.brand.admin`, `LCard.color == Theme.card`). Full `ctest` — expected `17 tests passed`.

- [ ] **Run the no-stray-hex grep audit** (§12.8) — every color literal in components must resolve through `Theme.*`; a raw hex should only appear in `Theme.qml`:
  ```
  grep -rnE '#[0-9A-Fa-f]{3,6}' qt-app/quick/qml/components/
  ```
  Expected: zero matches. (`"transparent"` is allowed; it is not a hex literal.)

- [ ] **Commit** via the `commit` skill. Stage the ten `qt-app/quick/qml/components/L*.qml`, `qt-app/quick/tests/tst_qml_components.cpp`, `qt-app/quick/tests/tst_qml_components.qml`, `qt-app/quick/CMakeLists.txt`. Suggested message: `feat: add ten L* design-system component skeletons bound to Theme`.

---

### Task 7: Phase 1 verification & proof assembly (the gate)

The four-check phase gate (spec §8, proposal §20 Phase 1): Debug **and** Release configure/build clean with no new warnings; full ctest green (the 12 existing + the 5 new Quick/VM tests); the RFID spike + render-check evidence assembled; `/claude-review` to APPROVE-or-cap; the human visual walkthrough noted. This task is mostly verification + assembling the proof — the only file it creates is the proof doc. Honest framing: the render-check (Task 4) and visual walkthrough are **human gates on real hardware**, not CI-runnable.

**Files:**
- Create: `docs/superpowers/proofs/2026-07-09-loams2-phase1-proof.md`
- (Reference: `docs/superpowers/proofs/2026-07-09-loams2-phase1-render-check.md` from Task 4)

**Interfaces:**
- Consumes: all prior task deliverables (`witscore`, `WITSQuick`, `RfidQuickFilter`, `ThemeViewModel`, `Theme.qml`, the ten `L*` components) and their test results.
- Produces: the Phase 1 proof document + a `/claude-review` verdict.

Steps:

- [ ] **Debug build check.** Clean-configure Debug and build; confirm zero new warnings (ignore only the two known-harmless ones):
  ```
  rm -rf qt-app/build
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Debug
  cmake --build qt-app/build
  ```
  Expected: `witscore`, `WITS`, `witsquick`, `WITSQuick`, and all 17 test targets build cleanly.

- [ ] **Full ctest (Debug).** `ctest --test-dir qt-app/build --output-on-failure` — expected `100% tests passed, 0 tests failed out of 17`: the 12 baseline (`tst_reportcontroller` still omitting `reportrenderer.cpp`) + `tst_appshell` + `tst_rfidquickfilter` + `tst_themeviewmodel` + `tst_qml_theme` + `tst_qml_components`.

- [ ] **Release build check.** Clean-configure Release and build; confirm no new warnings:
  ```
  rm -rf qt-app/build
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Release
  cmake --build qt-app/build
  ctest --test-dir qt-app/build --output-on-failure
  ```
  Expected: clean build + `17 tests passed`.

- [ ] **Rollback-flag check.** Verify `-DBUILD_LEGACY_WIDGETS=OFF` still configures and builds the Quick side without the legacy app (proves the gate works both ways for later phases):
  ```
  rm -rf qt-app/build
  cmake -S qt-app -B qt-app/build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DBUILD_LEGACY_WIDGETS=OFF
  cmake --build qt-app/build
  ```
  Expected: no `WITS` target built; `witscore`/`witsquick`/`WITSQuick`/tests build. Then restore the default (`ON`) build for the walkthrough.

- [ ] **Manual visual walkthrough (human gate — premium look is a human judgment, spec §8).** Run `qt-app/build/quick/WITSQuick.exe`: the empty AppShell window opens. This phase ships no screens, so the walkthrough scope is: window launches, no console QML warnings (`QT_LOGGING_RULES="qt.qml.binding.removal.info=false"` not needed — expect a clean log), `--software` path also launches. Record observations in the proof doc.

- [ ] **Write `docs/superpowers/proofs/2026-07-09-loams2-phase1-proof.md`** — the assembled Phase 1 gate evidence. Include:
  - **Check 1 — witscore extraction:** 12/12 ctest green before and after the move; each of the 12 still direct-compiles its class-under-test `.cpp` (list them); `WITS` still launches. Confirms proposal §20 Phase 1 deliverable (a).
  - **Check 2 — AppShell + Theme + components:** `WITSQuick` builds and launches; `tst_appshell` loads AppShell via `loadFromModule` with zero QML warnings; `ThemeViewModel` maps brand roles + notifies (`tst_themeviewmodel`); `Theme.qml` singleton resolves tokens (`tst_qml_theme`); all ten `L*` skeletons instantiate + bind Theme (`tst_qml_components`); no-stray-hex grep clean. Deliverable (b).
  - **Check 3 — RFID spike:** `tst_rfidquickfilter` proves `RfidScanDetector` reuse + terminator consumption under the re-authored `QQuickWindow` gate; note the real-window-activation observable-behavior sign-off is deferred to the Phase 2 kiosk checklist. Retires R1 at the wiring level. Deliverable (c).
  - **Check 4 — render check:** link to `2026-07-09-loams2-phase1-render-check.md`; record whether the deployment PC needs `--software`. Retires R4. Deliverable (d).
  - **Test inventory:** 12 → 17 targets, with the 5 additions named.
  - **`/claude-review` verdict** (filled in after the next step).
  - No PII; no machine-local paths.

- [ ] **Run `/claude-review`** via the Skill tool (`skill: "claude-review"`) on the Phase 1 branch. Fix Critical + Important findings (dispatch a subagent per the workflow rule — the main agent orchestrates), re-submit until APPROVE or the 3-round cap. Record the verdict + any capped findings in the proof doc.

- [ ] **Commit** via the `commit` skill. Stage `docs/superpowers/proofs/2026-07-09-loams2-phase1-proof.md` (and any review-driven fixes as separate concern-grouped commits). Suggested message: `docs: assemble LOAMS 2.0 Phase 1 gate proof`.

---

## Self-Review

I performed the required self-review passes against this plan and the source files I read; findings and fixes are inlined above. Summary:

**Spec-coverage (proposal §20 Phase 1 deliverables a–d + spec §9):**
- (a) `witscore` extraction, both apps build, 12/12 green — Task 1. ✅ Include-path subtlety caught: `brandtheme.cpp:18` includes root-staying `theme.h`, so `witscore` needs `PRIVATE ${CMAKE_SOURCE_DIR}` and the two brand tests keep both root + core include dirs. Grounded against actual grep, not assumed.
- (b) AppShell + Theme singleton + ThemeViewModel + ten `L*` components — Tasks 2, 5, 6. ✅
- (c) RFID-in-QML spike — Task 3. ✅ `RfidScanDetector` reused verbatim (moved, not rewritten); `RfidKeyboardFilter` NOT moved (stays legacy); `RfidQuickFilter` is a new `quick/` file re-authoring the gates.
- (d) deployment-hardware render check + software fallback — Task 4, human gate stated plainly. ✅

**Placeholder scan:** No "TBD"/"similar to Task N"/"add error handling here" placeholders. Every file-changing step shows real, complete CMake/C++/QML. Repeated content (e.g. the full `tests/CMakeLists.txt`, the whole top-level `CMakeLists.txt`) is shown in full rather than cross-referenced, per the format rule.

**Type/name consistency:**
- `feedKey(QChar, qint64) -> std::optional<QString>` — matches `rfidscandetector.h:20`. ✅
- `BrandTheme::current()`/`setCurrent()`/`regenerateFromLogo(BrandingConfig&, const QString&, QString*)`/`fallbackPalette()` — match `brandtheme.h:62-68`, `27`. ✅ `BrandPalette` 17 roles + `BrandingConfig` — match `brandthemedata.h:19-73`. ✅ `ThemeViewModel` exposes exactly those 17 roles.
- The 8 ViewModel names and 10 `L*` names match proposal §8/§9/§11 exactly (Phase 1 builds `ThemeViewModel` + all 10 `L*`). ✅
- CMake target names: `witscore`, `WITS`, `witsquick`, `WITSQuick`, and the 17 test targets are internally consistent across the three CMakeLists edits. ✅ QML module `URI LOAMS`, `VERSION 1.0` used consistently in `qt_add_qml_module`, `loadFromModule`, and every `import LOAMS`.
- Test env flags (`WIN32_EXECUTABLE FALSE`, `QT_FORCE_STDERR_LOGGING=1`, `QT_QPA_PLATFORM=offscreen` for GUI/Quick) applied to every new target, matching the existing 12 and the verified-environment facts. ✅

**Two documented, deliberate divergences from the proposal's literal §8 tree (flagged, not silent):**
1. The proposal §8 tree shows hand-written `theme/qmldir` and `components/qmldir`. This plan uses a single `qt_add_qml_module(witsquick URI LOAMS ...)`, whose **generated** module manifest subsumes those hand-authored `qmldir` files (the modern Qt 6 CMake QML-module mechanism the proposal's tree pre-dates). Under this CMake path the `pragma Singleton` in `Theme.qml` is **not** auto-registered as a module singleton on its own — the generated `qmldir` only emits `singleton Theme` when the source-file property `QT_QML_SINGLETON_TYPE TRUE` is set on `Theme.qml` (Task 5), which the plan does. This is a faithful modernization of the same structure, not a design change.
2. Full cache-first `BrandingController` startup sync inside `ThemeViewModel` (§13.2/13.3) is scoped to Phase 2 (where the branding network path actually runs with the kiosk); Phase 1's `ThemeViewModel` reads the engine's process-wide default (`current()`), which already defaults to `fallbackPalette()`. Task 5 states this scoping explicitly. Consistent with §20's Phase 1 being scaffolding + the RFID/render risk retirements, with kiosk parity (and its network wiring) in Phase 2.

**Ordering safety (Task 1):** steps move all files, then write all three CMakeLists changes (with a `quick/` placeholder so the tree configures before Task 2), then verify green — the tree never fails to build at a commit boundary. `add_subdirectory(libs/QXlsx)` is ordered before `add_subdirectory(core)` so `witscore`'s `QXlsx` link resolves. `enable_testing()` is hoisted to immediately after `find_package(...)`, before **every** `add_subdirectory` — `core/`, `quick/`, and `tests/` — because CMake only registers a subdirectory's `add_test()` calls when `enable_testing()` ran in an ancestor scope first; leaving it just above `add_subdirectory(tests)` (its original spot) would silently drop the `quick/` tests from ctest.
