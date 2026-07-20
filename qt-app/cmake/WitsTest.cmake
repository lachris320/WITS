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
    # Settings isolation is compiled into EVERY test target, not opted into per
    # test. Qt 6's QSettings(org, app) constructor is hardcoded to NativeFormat
    # and ignores setDefaultFormat(), and setPath() is a no-op for NativeFormat
    # on Windows — so a test that opens the app's scope directly reads and
    # WRITES the developer's real HKCU\Software\MyCompany\MyApp hive. That is a
    # silent data-loss bug a green suite cannot reveal, so the fix belongs here,
    # where no future test can forget it. See testsupport/settingsisolation.cpp
    # and core/appsettings.h.
    qt_add_executable(${name}
        ${T_SOURCES}
        ${CMAKE_SOURCE_DIR}/testsupport/settingsisolation.cpp)
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${name} PRIVATE
        Qt${QT_VERSION_MAJOR}::Test witsappsettings ${T_LIBS})
    if(T_INCLUDES)
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
    endif()
    if(T_DEFINES)
        target_compile_definitions(${name} PRIVATE ${T_DEFINES})
    endif()
    add_test(NAME ${name} COMMAND ${name})
    if(T_OFFSCREEN)
        # QT_QUICK_CONTROLS_STYLE=Basic: the OFFSCREEN platform still resolves
        # the native Windows Controls style by default, which ignores
        # LButton/TextField's `background`/`contentItem` customizations and
        # emits a QWARN ("does not support customization of this control")
        # per customized control instantiated. Force Basic uniformly so every
        # Quick test that instantiates a component-library control (BrandPanel,
        # GuestDialog, KioskMain, ...) both applies the intended styling and
        # runs QWARN-clean — this is also what WITSQuick.exe sets at runtime.
        set(_env "QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1;QT_QUICK_CONTROLS_STYLE=Basic")
        # This Qt SDK install has no bundled lib/fonts directory, so the
        # offscreen platform's font backend logs "QFontDatabase: Cannot find
        # font directory ...lib/fonts" the first time ANY text is rendered in
        # a process — regardless of font family, and unrelated to app/QML
        # correctness. Point it at the real system font directory instead so
        # offscreen tests see genuine fonts, same as a properly-deployed
        # environment would. Windows-only path; harmless no-op elsewhere
        # since this whole toolchain is MinGW/Windows (see project docs).
        if(WIN32)
            string(APPEND _env ";QT_QPA_FONTDIR=C:/Windows/Fonts")
        endif()
    else()
        set(_env "QT_FORCE_STDERR_LOGGING=1")
    endif()
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${_env}")
endfunction()
