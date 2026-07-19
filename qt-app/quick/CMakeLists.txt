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
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/LOAMS
    QML_FILES
        qml/AppShell.qml
        qml/theme/Theme.qml
        qml/kiosk/KioskScreen.qml
        qml/kiosk/BrandPanel.qml
        qml/kiosk/KioskMain.qml
        qml/kiosk/KioskFeedRow.qml
        qml/admin/AdminScreen.qml
        qml/admin/DashboardScreen.qml
        qml/admin/SearchScreen.qml
        qml/admin/VisitLogsScreen.qml
        qml/admin/DatabaseScreen.qml
        qml/admin/ReportingScreen.qml
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
        qml/components/LTextField.qml
        qml/components/LPulseDot.qml
        qml/components/LEyebrow.qml
        qml/components/LKioskGradient.qml
        qml/components/LSidebarBrand.qml
        qml/components/LConfirmDialog.qml
        qml/components/LComboBox.qml
        qml/kiosk/GuestDialog.qml
    SOURCES
        HttpForm.h HttpForm.cpp
        Initials.h Initials.cpp
        AdminSession.h AdminSession.cpp
        RfidQuickFilter.h RfidQuickFilter.cpp
        viewmodels/ThemeViewModel.h viewmodels/ThemeViewModel.cpp
        viewmodels/Navigator.h viewmodels/Navigator.cpp
        viewmodels/KioskViewModel.h viewmodels/KioskViewModel.cpp
        viewmodels/SchoolInfoViewModel.h viewmodels/SchoolInfoViewModel.cpp
        viewmodels/GuestViewModel.h viewmodels/GuestViewModel.cpp
        viewmodels/DashboardViewModel.h viewmodels/DashboardViewModel.cpp
        models/RecentLoginsModel.h models/RecentLoginsModel.cpp
        models/BarsModel.h models/BarsModel.cpp
        models/VisitLogRowsModel.h models/VisitLogRowsModel.cpp
        viewmodels/VisitLogsViewModel.h viewmodels/VisitLogsViewModel.cpp
        models/SearchResultsModel.h models/SearchResultsModel.cpp
        viewmodels/SearchViewModel.h viewmodels/SearchViewModel.cpp
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

# models/ on the include path (analogous to the viewmodels/ line above), so
# KioskViewModel.h's #include "RecentLoginsModel.h" and the test's
# quote-includes resolve.
target_include_directories(witsquickmodule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/models)

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
# The literal "import LOAMS" lives only in tst_qml_theme.qml (a QUICK_TEST_MAIN
# data file qmlimportscanner never scans), so the automatic static-plugin import
# never fires — link witsquickmoduleplugin + its Q_IMPORT_PLUGIN "_init" object
# explicitly or the statically-embedded LOAMS module never registers.
wits_add_qttest(tst_qml_theme
    SOURCES tests/tst_qml_theme.cpp
    LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
         Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    OFFSCREEN)

# --- Navigator unit test (C++ QtTest, offscreen: house consistency with the
# other quick tests — witsquickmodule PUBLIC-propagates Qt::Gui, which would
# otherwise make QTEST_MAIN build a QGuiApplication) ---
wits_add_qttest(tst_navigator
    SOURCES tests/tst_navigator.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Qml
    OFFSCREEN)

# --- AdminSession unit test (C++ QtTest, offscreen: house consistency —
# witsquickmodule PUBLIC-propagates Qt::Gui). ---
wits_add_qttest(tst_adminsession
    SOURCES tests/tst_adminsession.cpp
    LIBS witsquickmodule
    OFFSCREEN)

# --- KioskViewModel + RecentLoginsModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_kioskviewmodel
    SOURCES tests/tst_kioskviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Network
    OFFSCREEN)

# --- SchoolInfoViewModel unit test (C++ QtTest, offscreen: house
# consistency with tst_initials/tst_navigator above — witsquickmodule
# PUBLIC-propagates Qt::Gui, which would otherwise make QTEST_MAIN build a
# QGuiApplication) ---
wits_add_qttest(tst_schoolinfoviewmodel
    SOURCES tests/tst_schoolinfoviewmodel.cpp
    LIBS witsquickmodule
    OFFSCREEN)

# --- GuestViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_guestviewmodel
    SOURCES tests/tst_guestviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network
    OFFSCREEN)

# --- HttpForm pure-helper unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_httpform
    SOURCES tests/tst_httpform.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network
    OFFSCREEN)

# --- Initials pure-helper unit test (C++ QtTest, offscreen: house
# consistency with tst_httpform above — witsquickmodule PUBLIC-propagates
# Qt::Gui, which would otherwise make QTEST_MAIN build a QGuiApplication) ---
wits_add_qttest(tst_initials
    SOURCES tests/tst_initials.cpp
    LIBS witsquickmodule
    OFFSCREEN)

# --- BarsModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_barsmodel
    SOURCES tests/tst_barsmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- L* component skeletons Quick test (offscreen) ---
# Same escape hatch as tst_qml_theme above: "import LOAMS" lives only in the
# QUICK_TEST_MAIN data file, so the plugin + "_init" object must be linked
# explicitly.
wits_add_qttest(tst_qml_components
    SOURCES tests/tst_qml_components.cpp
    LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
         Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    OFFSCREEN)

# --- Kiosk screens Quick test (offscreen) ---
# Same escape hatch as tst_qml_theme/tst_qml_components above: "import LOAMS"
# lives only in the QUICK_TEST_MAIN data file, so the plugin + "_init" object
# must be linked explicitly.
wits_add_qttest(tst_qml_kiosk
    SOURCES tests/tst_qml_kiosk.cpp
    LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
         Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    OFFSCREEN)

# --- DashboardViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_dashboardviewmodel
    SOURCES tests/tst_dashboardviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- VisitLogsViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_visitlogsviewmodel
    SOURCES tests/tst_visitlogsviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- SearchViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_searchviewmodel
    SOURCES tests/tst_searchviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- Admin screens Quick test (offscreen). "import LOAMS" lives only in the
# QUICK_TEST_MAIN data file, so link the plugin + "_init" object explicitly. ---
wits_add_qttest(tst_qml_admin
    SOURCES tests/tst_qml_admin.cpp
    LIBS witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init
         Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
    DEFINES QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    OFFSCREEN)
