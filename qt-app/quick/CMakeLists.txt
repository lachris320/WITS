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
        qml/admin/StudentEditDialog.qml
        qml/admin/RegisterStudentDialog.qml
        qml/admin/ImportStudentsDialog.qml
        qml/admin/BulkEditDialog.qml
        qml/admin/ReportingScreen.qml
        qml/admin/SettingsScreen.qml
        qml/admin/AccessControlScreen.qml
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
        qml/components/LLogoCircle.qml
        qml/components/LCircleImage.qml
        qml/components/LAvatar.qml
        qml/components/LSpinner.qml
        qml/components/LSidebarBrand.qml
        qml/components/LConfirmDialog.qml
        qml/components/LComboBox.qml
        qml/components/LCascadingSelect.qml
        qml/components/LCheckbox.qml
        qml/components/LDatePicker.qml
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
        viewmodels/SchoolInfoUtil.h viewmodels/SchoolInfoUtil.cpp
        viewmodels/GuestViewModel.h viewmodels/GuestViewModel.cpp
        viewmodels/DashboardViewModel.h viewmodels/DashboardViewModel.cpp
        models/RecentLoginsModel.h models/RecentLoginsModel.cpp
        models/BarsModel.h models/BarsModel.cpp
        models/VisitLogRowsModel.h models/VisitLogRowsModel.cpp
        viewmodels/VisitLogsViewModel.h viewmodels/VisitLogsViewModel.cpp
        models/SearchResultsModel.h models/SearchResultsModel.cpp
        models/StudentsTableModel.cpp models/StudentsTableModel.h
        models/ReportRowsModel.h models/ReportRowsModel.cpp
        models/RankingModel.cpp
        models/RankingModel.h
        models/AccessEntriesModel.h models/AccessEntriesModel.cpp
        viewmodels/SearchViewModel.h viewmodels/SearchViewModel.cpp
        viewmodels/DatabaseViewModel.cpp viewmodels/DatabaseViewModel.h
        viewmodels/SettingsViewModel.h viewmodels/SettingsViewModel.cpp
        viewmodels/ImportViewModel.cpp viewmodels/ImportViewModel.h
        viewmodels/ReportingViewModel.h viewmodels/ReportingViewModel.cpp
        viewmodels/AccessControlViewModel.h viewmodels/AccessControlViewModel.cpp
        AccessControlHub.h AccessControlHub.cpp
        AccessControlSingleton.h
)

target_link_libraries(witsquickmodule PUBLIC
    witscore
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::QuickControls2
    Qt${QT_VERSION_MAJOR}::QuickDialogs2
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::PrintSupport
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

# Static-plugin note: qt_add_qml_module's automatic static-plugin import
# (qmlimportscanner, run via a deferred call at the end of this directory)
# only fires when a source the scanner analyzes contains a literal
# "import LOAMS" statement. No consumer here has one: WITSQuick (main.cpp
# only) and tst_appshell resolve the module purely at runtime via
# loadFromModule("LOAMS", "AppShell"), and the QuickTest targets
# (wits_add_qmltest) have "import LOAMS" only in their QUICK_TEST_MAIN .qml
# data file, which the scanner never sees. So the plugin is never linked
# automatically, and every consumer links WITS_LOAMS_STATIC_LIBS — the module,
# its generated plugin, and the Q_IMPORT_PLUGIN(LOAMSPlugin) "_init" object —
# explicitly so the statically-embedded LOAMS module actually registers.
# It is a variable, not an INTERFACE library, because witsquickmoduleplugin_init
# is an OBJECT library: CMake links an object library's object files only into
# targets that list it directly; via an INTERFACE library's
# INTERFACE_LINK_LIBRARIES it is treated as usage requirements only, so the
# Q_IMPORT_PLUGIN object would be dropped and LOAMS would silently never
# register.
set(WITS_LOAMS_STATIC_LIBS
    witsquickmodule witsquickmoduleplugin witsquickmoduleplugin_init)

qt_add_executable(WITSQuick main.cpp)
target_link_libraries(WITSQuick PRIVATE
    ${WITS_LOAMS_STATIC_LIBS}
    Qt${QT_VERSION_MAJOR}::Widgets
)
set_target_properties(WITSQuick PROPERTIES WIN32_EXECUTABLE TRUE)

# --- AppShell load test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_appshell
    SOURCES tests/tst_appshell.cpp
    LIBS ${WITS_LOAMS_STATIC_LIBS}
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

# wits_add_qmltest(<name>)
#
# Registers one QuickTest target from tests/<name>.qml alone: the target runs
# exactly that file, and its main is generated from tests/QuickTestMain.cpp.in
# (configure_file, so it is rewritten only when its content changes) and uses
# the shared tests/QuickTestSetup.h. The main passes quick_test_main the
# single .qml FILE, not the tests/ directory: a directory makes it run every
# tst_*.qml in it, so each target re-ran the whole QML suite.
#
# The absolute .qml path is embedded in the generated main as ASCII-only \xNN
# byte escapes of its UTF-8 (not a -D define, whose non-ASCII bytes do not
# survive the Windows compiler command line reliably) and checked at startup
# by tests/QuickTestSourcePath.h. quick_test_main decodes the path with
# QString::fromLocal8Bit and, if it does not exist, silently scans the current
# working directory instead — so a missing or code-page-unrepresentable path
# (non-ASCII checkout on Windows) now exits 1 with a message. The configure-
# time check below catches a missing file earlier.
#
# Links WITS_LOAMS_STATIC_LIBS (see the static-plugin note above WITSQuick).
function(wits_add_qmltest name)
    set(_qml "${CMAKE_CURRENT_SOURCE_DIR}/tests/${name}.qml")
    if(NOT EXISTS "${_qml}")
        message(FATAL_ERROR "wits_add_qmltest(${name}): ${_qml} does not exist")
    endif()
    set(WITS_QMLTEST_NAME ${name})
    # UTF-8 bytes -> "\x2f\x68..." via a scratch file: string(HEX) needs 3.18.
    set(_pathfile "${CMAKE_CURRENT_BINARY_DIR}/qmltest_main/${name}.path")
    file(WRITE "${_pathfile}" "${_qml}")
    file(READ "${_pathfile}" _hex HEX)
    string(REGEX REPLACE "(..)" "\\\\x\\1" WITS_QMLTEST_PATH_ESCAPED "${_hex}")
    set(_main "${CMAKE_CURRENT_BINARY_DIR}/qmltest_main/${name}.cpp")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/QuickTestMain.cpp.in
        "${_main}" @ONLY)
    wits_add_qttest(${name}
        SOURCES "${_main}" tests/QuickTestSetup.h
        LIBS ${WITS_LOAMS_STATIC_LIBS}
             Qt${QT_VERSION_MAJOR}::QuickTest Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/tests
        OFFSCREEN)
    set_property(GLOBAL APPEND PROPERTY WITS_QMLTEST_TARGETS ${name})
endfunction()

# --- QuickTestSourcePath pure-helper unit test (C++ QtTest). Guards the
# path check every generated QuickTest main runs (missing/code-page-
# unrepresentable path -> loud error, -input detection). Pure QString/
# QFileInfo logic (no GUI -> QTEST_APPLESS_MAIN, no OFFSCREEN). ---
wits_add_qttest(tst_quicktestsourcepath
    SOURCES tests/tst_quicktestsourcepath.cpp
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/tests)

# --- Theme singleton Quick test (offscreen) ---
wits_add_qmltest(tst_qml_theme)

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

# --- SchoolInfoUtil pure-helper unit test (C++ QtTest). Pure logic over
# QString/QUrl/QFileInfo (no GUI, no rendering -> QTEST_APPLESS_MAIN, no
# OFFSCREEN). Guards the shared logo graceful-degradation seam that both
# KioskViewModel and SchoolInfoViewModel delegate to. ---
wits_add_qttest(tst_schoolinfoutil
    SOURCES tests/tst_schoolinfoutil.cpp
    LIBS witsquickmodule)

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

# --- StudentsTableModel unit test (C++ QtTest). Pure QAbstractListModel logic
# over StudentRecord (selection tracked by schoolId, survives setRecords) with
# NO QNetworkAccessManager -> QTEST_APPLESS_MAIN, no OFFSCREEN. Links the module
# only (which now contains StudentsTableModel.cpp) to avoid a duplicate symbol.
wits_add_qttest(tst_studentstablemodel
    SOURCES tests/tst_studentstablemodel.cpp
    LIBS witsquickmodule)

# --- SearchResultsModel unit test (C++ QtTest). PhotoRole joins ApiConfig base;
# pure model logic, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_searchresultsmodel
    SOURCES tests/tst_searchresultsmodel.cpp
    LIBS witsquickmodule)

# --- ReportRowsModel unit test (C++ QtTest). Pure QAbstractListModel over
# JSON rows, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_reportrowsmodel
    SOURCES tests/tst_reportrowsmodel.cpp
    LIBS witsquickmodule)

# --- AccessEntriesModel unit test (C++ QtTest). Pure QAbstractListModel over
# LoginParser::RecentEntry, no NAM -> QTEST_APPLESS_MAIN, no OFFSCREEN. ---
wits_add_qttest(tst_accessentriesmodel
    SOURCES tests/tst_accessentriesmodel.cpp
    LIBS witsquickmodule)

# --- L* component skeletons Quick test (offscreen) ---
wits_add_qmltest(tst_qml_components)

# --- Kiosk screens Quick test (offscreen) ---
wits_add_qmltest(tst_qml_kiosk)

# --- AccessControlHub unit test (C++ QtTest, offscreen). SequencedNam drives
# the provider with no live network; AppSettings isolation is compiled in by
# wits_add_qttest. ---
wits_add_qttest(tst_accesscontrolhub
    SOURCES
        tests/tst_accesscontrolhub.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/sequencednam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)

# --- DashboardViewModel unit test (C++ QtTest, offscreen). Phase 6a threads
# AdminSession's key through refresh(), captured via the CapturingNam harness
# (qt-app/testsupport) — mirrors tst_visitlogsviewmodel's approach. ---
wits_add_qttest(tst_dashboardviewmodel
    SOURCES tests/tst_dashboardviewmodel.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)

# --- VisitLogsViewModel unit test (C++ QtTest, offscreen). Phase 6a threads
# AdminSession's key through refresh(), captured via the CapturingNam harness
# (qt-app/testsupport) — mirrors tst_databaseviewmodel's approach. ---
wits_add_qttest(tst_visitlogsviewmodel
    SOURCES tests/tst_visitlogsviewmodel.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)

# --- AccessControlViewModel unit test (C++ QtTest, offscreen). admin_key in
# the POST body, 401/5xx/transport paths and the generation guard, all via the
# CapturingNam harness (qt-app/testsupport) — no live network. ---
wits_add_qttest(tst_accesscontrolviewmodel
    SOURCES tests/tst_accesscontrolviewmodel.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.cpp
        ${CMAKE_SOURCE_DIR}/testsupport/capturingnam.h
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    INCLUDES ${CMAKE_SOURCE_DIR}/testsupport
    OFFSCREEN)

# --- SearchViewModel unit test (C++ QtTest, offscreen) ---
wits_add_qttest(tst_searchviewmodel
    SOURCES tests/tst_searchviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- DatabaseViewModel unit test (C++ QtTest, offscreen). The VM builds a real
# QNetworkAccessManager + StudentController and some paths (setDepartment) fire a
# fire-and-forget post(), so this needs a QCoreApplication (QTEST_MAIN, not
# APPLESS) + Network/Gui + OFFSCREEN — mirrors tst_searchviewmodel. ---
wits_add_qttest(tst_databaseviewmodel
    SOURCES tests/tst_databaseviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- Admin screens Quick test (offscreen) ---
wits_add_qmltest(tst_qml_admin)

# --- AdminScreen shell Quick test (offscreen) ---
wits_add_qmltest(tst_qml_adminshell)

# --- AccessControlScreen Quick test (offscreen) ---
wits_add_qmltest(tst_qml_accesscontrol)

# --- SettingsViewModel unit test (C++ QtTest, offscreen). Uses Network for
# the async POST paths (T10-T13) and Gui via witsquickmodule propagation. ---
wits_add_qttest(tst_settingsviewmodel
    SOURCES tests/tst_settingsviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- ImportViewModel unit test (C++ QtTest, offscreen). The VM builds a real
# QNetworkAccessManager + ImportController; startImport() fires a
# fire-and-forget post() (harmless, never resolves in-test), and the state
# machine is driven via the public on* slots — mirrors tst_databaseviewmodel. ---
wits_add_qttest(tst_importviewmodel
    SOURCES tests/tst_importviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- ReportingViewModel unit test (C++ QtTest, offscreen). Builds a real
# QNetworkAccessManager + ReportController; generateReport() fires a
# fire-and-forget post() (harmless in-test), and the reply handlers are driven
# via the public on* slots — mirrors tst_databaseviewmodel. ---
wits_add_qttest(tst_reportingviewmodel
    SOURCES tests/tst_reportingviewmodel.cpp
    LIBS witsquickmodule Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Gui
    OFFSCREEN)

# --- Deprecated-token grep-guard (Phase 4d Task 12, C++ QtTest). Pure file
# I/O over the QML source + QuickTest data dirs (no GUI, no rendering ->
# QTEST_APPLESS_MAIN, no OFFSCREEN). Fails if any deprecated brand/accent alias
# name reappears in *.qml CODE (comments are stripped first). ---
wits_add_qttest(tst_notokenaliases
    SOURCES tests/tst_notokenaliases.cpp
    DEFINES SRC_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
            SRC_QML_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")

# Coverage guard: before wits_add_qmltest, every QuickTest target scanned the
# whole tests/ directory, so a new tst_*.qml was picked up automatically. Now
# each target runs one file, so a tst_*.qml without its own wits_add_qmltest
# would silently never run — fail the configure instead.
# MUST stay the last statement in this file: it only sees wits_add_qmltest()
# calls made above it. tests/ is scanned non-recursively (QuickTest .qml files
# live flat in tests/).
file(GLOB _wits_qml_tests CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_*.qml)
get_property(_wits_qml_targets GLOBAL PROPERTY WITS_QMLTEST_TARGETS)
set(_wits_qml_orphans "")
foreach(_f IN LISTS _wits_qml_tests)
    get_filename_component(_n "${_f}" NAME_WE)
    if(NOT _n IN_LIST _wits_qml_targets)
        list(APPEND _wits_qml_orphans "tests/${_n}.qml")
    endif()
endforeach()
if(_wits_qml_orphans)
    message(FATAL_ERROR "QuickTest file(s) with no wits_add_qmltest() target "
        "(they would never run): ${_wits_qml_orphans}")
endif()
unset(_wits_qml_tests)
unset(_wits_qml_targets)
unset(_wits_qml_orphans)
unset(_f)
unset(_n)
