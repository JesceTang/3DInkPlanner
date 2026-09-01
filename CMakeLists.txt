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

# Eigen（header-only）
set(EIGEN3_INCLUDE_DIR "D:/eigen/eigen-3.4.0" CACHE PATH "Eigen 3 include directory")

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
qt_standard_project_setup()

find_package(OpenGL REQUIRED)

# 核心库：geometry + io + slicing，不依赖 Qt，便于单独测试。
add_library(3DInkPlannerCore STATIC
    geometry/GeometryTypes.h
    io/STLReader.h
    io/STLReader.cpp
    slicing/TrianglePlaneIntersection.h
    slicing/TrianglePlaneIntersection.cpp
)
target_include_directories(3DInkPlannerCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${EIGEN3_INCLUDE_DIR}
)

# 主程序。
qt_add_executable(3DInkPlanner
    app/main.cpp
    app/MainWindow.h
    app/MainWindow.cpp
    graphics/GLWidget.h
    graphics/GLWidget.cpp
    graphics/Camera.h
    graphics/Camera.cpp
    graphics/Shader.h
    graphics/Shader.cpp
    graphics/MeshRenderer.h
    graphics/MeshRenderer.cpp
)
target_link_libraries(3DInkPlanner PRIVATE
    3DInkPlannerCore
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    OpenGL::GL
)

# 测试（无第三方框架）。
enable_testing()
add_executable(test_stl_reader tests/test_stl_reader.cpp)
target_link_libraries(test_stl_reader PRIVATE 3DInkPlannerCore)
add_test(NAME stl_reader COMMAND test_stl_reader)

add_executable(test_intersection tests/test_intersection.cpp)
target_link_libraries(test_intersection PRIVATE 3DInkPlannerCore)
add_test(NAME intersection COMMAND test_intersection)

# 开发期保留控制台便于查看 qDebug 日志；发布时可设 WIN32_EXECUTABLE TRUE 隐藏控制台。
