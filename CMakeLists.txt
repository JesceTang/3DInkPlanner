cmake_minimum_required(VERSION 3.21)

project(3DInkPlanner
    VERSION 0.1.0
    DESCRIPTION "3D inkjet printing pre-processing and path planning demo"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: 源码按 UTF-8 编译 + 多核编译
if(MSVC)
    add_compile_options(/utf-8 /MP)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
qt_standard_project_setup()

find_package(OpenGL REQUIRED)

qt_add_executable(3DInkPlanner
    app/main.cpp
    app/MainWindow.h
    app/MainWindow.cpp
    graphics/GLWidget.h
    graphics/GLWidget.cpp
)

target_include_directories(3DInkPlanner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics
)

target_link_libraries(3DInkPlanner PRIVATE
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    OpenGL::GL
)

# 开发期保留控制台便于查看 qDebug 日志；发布时可设 WIN32_EXECUTABLE TRUE 隐藏控制台。
