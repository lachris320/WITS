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

qt_add_executable(tst_theme
    tst_theme.cpp
)
set_target_properties(tst_theme PROPERTIES WIN32_EXECUTABLE FALSE)
target_include_directories(tst_theme PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(tst_theme PRIVATE Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
add_test(NAME tst_theme COMMAND tst_theme)
set_tests_properties(tst_theme PROPERTIES
    ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1;QT_QPA_PLATFORM=offscreen"
)
