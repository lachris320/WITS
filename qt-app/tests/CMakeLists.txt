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
