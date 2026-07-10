qt_standard_project_setup(REQUIRES 6.5)

# The QML module as a STATIC library so both WITSQuick and the Quick/QtTest
# targets can link it and get its generated qmldir + embedded resources.
#
# CMake target name deliberately NOT "witsquick": on this Windows/NTFS kit,
# CMake target build directories are case-preserving but the filesystem is
# case-insensitive, so a target "witsquick" and a target "WITSQuick" (the
# executable below) collide on disk (witsquick_autogen == WITSQuick_autogen)
# and CMake's generate step fails with "already has a custom rule". The QML
# module's externally-visible identity (URI LOAMS, type AppShell) is
# unaffected by this internal CMake target name.
qt_add_library(witsquickmodule STATIC)

# Theme.qml is a pragma-Singleton QML file. The pragma alone is not enough
# for qt_add_qml_module to emit "singleton Theme" into the generated qmldir
# — this source-file property must be set BEFORE Theme.qml is listed below,
# or "import LOAMS; Theme.<token>" fails to resolve the singleton.
set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE)

# Theme.qml lives in a qml/theme/ subdirectory (not qml/'s top level like
# AppShell.qml), which triggers a QTP0004 policy dev-warning ("You need
# qmldir files for each extra directory..."). Opt in to NEW: it generates a
# small "prefer :<prefix>" qmldir for the extra directory for tooling
# (qmllint) — it does not change the LOAMS URI or move Theme into a nested
# namespace, so "import LOAMS; Theme.<token>" keeps resolving exactly as
# verified by tst_qml_theme.
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_qml_module(witsquickmodule
    URI LOAMS
    VERSION 1.0
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
    SOURCES
        RfidQuickFilter.h RfidQuickFilter.cpp
        viewmodels/ThemeViewModel.h viewmodels/ThemeViewModel.cpp
)

target_link_libraries(witsquickmodule PUBLIC
    witscore
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::Gui
)

# Exposes the quick/ root on witsquickmodule's PUBLIC include path so
# tst_rfidquickfilter (in quick/tests/) can #include "RfidQuickFilter.h" by
# quote-include, which searches the including file's own directory then the
# -I paths. witscore's PUBLIC include dir is core/, not quick/, so without
# this line the header would not resolve. Analogous to Task 5's viewmodels/
# include line for ThemeViewModel.h.
target_include_directories(witsquickmodule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Module include dir so "ThemeViewModel.h" (in viewmodels/) resolves for
# Theme.qml's generated registration and for tests that quote-include it.
target_include_directories(witsquickmodule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/viewmodels)

# qt_add_qml_module's automatic static-plugin import (qmlimportscanner, run
# via a deferred call at the end of this directory) only fires when a target
# statically analyzable QML/C++ source contains a literal "import LOAMS"
# statement. Neither WITSQuick (main.cpp only) nor tst_appshell has one — both
# resolve the module purely at runtime via loadFromModule("LOAMS", "AppShell")
# — so the scanner finds zero imports and never links the plugin. Link the
# generated plugin target and its Q_IMPORT_PLUGIN(LOAMSPlugin) "_init" object
# explicitly so the statically-embedded LOAMS module actually registers.
qt_add_executable(WITSQuick main.cpp)
target_link_libraries(WITSQuick PRIVATE
    witsquickmodule
    witsquickmoduleplugin
    witsquickmoduleplugin_init
)
set_target_properties(WITSQuick PROPERTIES WIN32_EXECUTABLE TRUE)

# --- AppShell load test (C++ QtTest, offscreen) ---
qt_add_executable(tst_appshell tests/tst_appshell.cpp)
set_target_properties(tst_appshell PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(tst_appshell PRIVATE
    witsquickmodule
    witsquickmoduleplugin
    witsquickmoduleplugin_init
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Quick
)
add_test(NAME tst_appshell COMMAND tst_appshell)
set_tests_properties(tst_appshell PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
)

# --- RFID-in-QML spike test (C++ QtTest, offscreen) ---
qt_add_executable(tst_rfidquickfilter tests/tst_rfidquickfilter.cpp)
set_target_properties(tst_rfidquickfilter PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(tst_rfidquickfilter PRIVATE
    witsquickmodule
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Quick
)
add_test(NAME tst_rfidquickfilter COMMAND tst_rfidquickfilter)
set_tests_properties(tst_rfidquickfilter PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
)

# --- ThemeViewModel unit test (C++ QtTest, offscreen: paints QImages) ---
qt_add_executable(tst_themeviewmodel tests/tst_themeviewmodel.cpp)
set_target_properties(tst_themeviewmodel PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(tst_themeviewmodel PRIVATE
    witsquickmodule
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Svg
)
add_test(NAME tst_themeviewmodel COMMAND tst_themeviewmodel)
set_tests_properties(tst_themeviewmodel PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)

# --- Theme singleton Quick test (offscreen) ---
# Escape hatch (same precedent as WITSQuick/tst_appshell above): the literal
# "import LOAMS" lives only in tst_qml_theme.qml, a QUICK_TEST_MAIN data file
# that qmlimportscanner never scans (it only scans tst_qml_theme.cpp, which
# has no import statement) — so the automatic static-plugin import never
# fires and the module fails to resolve at runtime ("plugin
# witsquickmoduleplugin not found", confirmed by running this target before
# adding the two lines below). Link the generated plugin target and its
# Q_IMPORT_PLUGIN "_init" object explicitly so the statically-embedded LOAMS
# module (and its Theme singleton) actually registers.
qt_add_executable(tst_qml_theme tests/tst_qml_theme.cpp)
set_target_properties(tst_qml_theme PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(tst_qml_theme PRIVATE
    witsquickmodule
    witsquickmoduleplugin
    witsquickmoduleplugin_init
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

# --- L* component skeletons Quick test (offscreen) ---
# Same escape hatch as tst_qml_theme above, and for the same reason: the
# literal "import LOAMS" lives only in tst_qml_components.qml (a
# QUICK_TEST_MAIN data file qmlimportscanner never scans), so the automatic
# static-plugin import never fires. Link the generated plugin target and its
# Q_IMPORT_PLUGIN "_init" object explicitly. Also note the CMake target is
# "witsquickmodule", not "witsquick" (see the comment atop this file).
qt_add_executable(tst_qml_components tests/tst_qml_components.cpp)
set_target_properties(tst_qml_components PROPERTIES WIN32_EXECUTABLE FALSE)
target_link_libraries(tst_qml_components PRIVATE
    witsquickmodule
    witsquickmoduleplugin
    witsquickmoduleplugin_init
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
