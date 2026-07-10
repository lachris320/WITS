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
