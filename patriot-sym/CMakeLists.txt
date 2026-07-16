cmake_minimum_required(VERSION 3.20)
project(patriot-sym VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

set(SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    Simulation.h
    Simulation.cpp
    SimRenderer.h
    SimRenderer.cpp
    RadarDisplay.h
    RadarDisplay.cpp
    GameState.h
    GameState.cpp
    entities/Missile.h
    entities/ThreatType.h
    entities/ProtectedAsset.h
    physics/Ballistic.h
    physics/Ballistic.cpp
    physics/Interceptor.h
    physics/Interceptor.cpp
    resources.qrc
)

qt_add_executable(patriot-sym ${SOURCES})

target_link_libraries(patriot-sym PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

set_target_properties(patriot-sym PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE    TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(patriot-sym)
endif()
