# witscore — Widgets-free (mostly) business logic shared by the legacy WITS
# Widgets app and the new WITSQuick Qt Quick app. Option (a) dependency
# envelope (proposal §7.1): reportrenderer's Qt::Charts + Qt::Widgets + QXlsx
# link leaks PUBLIC into every consumer — an accepted Phase 1–5 trade-off,
# a Phase-6 split candidate (witscore_reports), not a defect to fix now.

add_library(witscore STATIC
    apiconfig.h
    settingsdata.h
    settingscontroller.h settingscontroller.cpp
    visitordata.h
    visitorcontroller.h visitorcontroller.cpp
    importdata.h
    importcontroller.h importcontroller.cpp
    studentdata.h
    studentcontroller.h studentcontroller.cpp
    reportdata.h
    reportcontroller.h reportcontroller.cpp
    reportrenderer.h reportrenderer.cpp
    brandcolormath.h
    brandthemedata.h
    brandtheme.h brandtheme.cpp
    brandingcontroller.h brandingcontroller.cpp
    rfidscandetector.h rfidscandetector.cpp
    loginparser.h loginparser.cpp
    rfideventfilterbase.h rfideventfilterbase.cpp
)

# AUTOMOC required (7 Q_OBJECT classes: the five *Controller + BrandingController
# + RfidEventFilterBase) and free — it is already ON at directory scope.
# AUTOUIC OFF: zero .ui inputs.
set_target_properties(witscore PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
)

target_link_libraries(witscore PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Charts
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    QXlsx
)

target_include_directories(witscore
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}   # core/ — consumers resolve "settingscontroller.h" etc.
    PRIVATE ${CMAKE_SOURCE_DIR}           # qt-app/ root — brandtheme.cpp includes "theme.h"
)
