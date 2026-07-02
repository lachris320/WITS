qt_add_executable(tst_rfidscandetector
    tst_rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.h
)
set_target_properties(tst_rfidscandetector PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_rfidscandetector PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_rfidscandetector PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_rfidscandetector COMMAND tst_rfidscandetector)
set_tests_properties(tst_rfidscandetector PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

qt_add_executable(tst_rfidkeyboardfilter
    tst_rfidkeyboardfilter.cpp
    ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.cpp
    ${CMAKE_SOURCE_DIR}/rfidkeyboardfilter.h
    ${CMAKE_SOURCE_DIR}/rfidscandetector.cpp
    ${CMAKE_SOURCE_DIR}/rfidscandetector.h
)
set_target_properties(tst_rfidkeyboardfilter PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_rfidkeyboardfilter PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_rfidkeyboardfilter PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME tst_rfidkeyboardfilter COMMAND tst_rfidkeyboardfilter)
set_tests_properties(tst_rfidkeyboardfilter PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(tst_apiconfig
    tst_apiconfig.cpp
)
set_target_properties(tst_apiconfig PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_apiconfig PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_apiconfig PRIVATE Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME tst_apiconfig COMMAND tst_apiconfig)
set_tests_properties(tst_apiconfig PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1"
)

qt_add_executable(tst_theme
    tst_theme.cpp
)
set_target_properties(tst_theme PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_theme PRIVATE ${CMAKE_SOURCE_DIR})
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
target_include_directories(tst_responsive_ui PRIVATE ${CMAKE_SOURCE_DIR})
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
    ${CMAKE_SOURCE_DIR}/visitorcontroller.cpp
    ${CMAKE_SOURCE_DIR}/visitorcontroller.h
    ${CMAKE_SOURCE_DIR}/visitordata.h
)
set_target_properties(tst_visitorcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_visitorcontroller PRIVATE ${CMAKE_SOURCE_DIR})
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
    ${CMAKE_SOURCE_DIR}/importcontroller.cpp
    ${CMAKE_SOURCE_DIR}/importcontroller.h
    ${CMAKE_SOURCE_DIR}/importdata.h
)
set_target_properties(tst_importcontroller PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_importcontroller PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_importcontroller PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Network
    QXlsx
)
add_test(NAME tst_importcontroller COMMAND tst_importcontroller)
set_tests_properties(tst_importcontroller PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1"
)
