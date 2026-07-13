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
    ${CMAKE_SOURCE_DIR}/core/rfideventfilterbase.cpp
    ${CMAKE_SOURCE_DIR}/core/rfideventfilterbase.h
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

# 13th — pure kiosk login decode/decision logic. Core-only (no Gui/Widgets),
# so QTEST_MAIN expands to the QCoreApplication variant; no offscreen needed.
wits_add_qttest(tst_loginparser
    SOURCES
        tst_loginparser.cpp
        ${CMAKE_SOURCE_DIR}/core/loginparser.cpp
        ${CMAKE_SOURCE_DIR}/core/loginparser.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)

# --- dashboard_summary.php response parser (pure core, no offscreen) ---
wits_add_qttest(tst_dashboardparser
    SOURCES
        tst_dashboardparser.cpp
        ${CMAKE_SOURCE_DIR}/core/dashboardparser.cpp
        ${CMAKE_SOURCE_DIR}/core/dashboardparser.h
        ${CMAKE_SOURCE_DIR}/core/dashboarddata.h
    INCLUDES ${CMAKE_SOURCE_DIR}/core)
