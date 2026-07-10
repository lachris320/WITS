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

qt_add_qml_module(witsquickmodule
    URI LOAMS
    VERSION 1.0
    QML_FILES
        qml/AppShell.qml
    SOURCES
        RfidQuickFilter.h RfidQuickFilter.cpp
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
