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

# 核心库：geometry + io + slicing + path，不依赖 Qt，便于单独测试。
add_library(3DInkPlannerCore STATIC
    geometry/GeometryTypes.h
    geometry/Polyline.h
    geometry/Polygon.h
    geometry/Transform.h
    geometry/Transform.cpp
    io/STLReader.h
    io/STLReader.cpp
    io/PathExporter.h
    io/PathExporter.cpp
    slicing/TrianglePlaneIntersection.h
    slicing/TrianglePlaneIntersection.cpp
    slicing/SegmentConnector.h
    slicing/SegmentConnector.cpp
    slicing/ContourBuilder.h
    slicing/ContourBuilder.cpp
    slicing/ContourClassifier.h
    slicing/ContourClassifier.cpp
    slicing/Slicer.h
    slicing/Slicer.cpp
    path/ToolPath.h
    path/RasterFillGenerator.h
    path/RasterFillGenerator.cpp
    path/PathOptimizer.h
    path/PathOptimizer.cpp
    topology/IndexedMesh.h
    topology/MeshBuilder.h
    topology/MeshBuilder.cpp
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
    widgets/SliceView.h
    widgets/SliceView.cpp
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

add_executable(test_segment_connector tests/test_segment_connector.cpp)
target_link_libraries(test_segment_connector PRIVATE 3DInkPlannerCore)
add_test(NAME segment_connector COMMAND test_segment_connector)

add_executable(test_slicer tests/test_slicer.cpp)
target_link_libraries(test_slicer PRIVATE 3DInkPlannerCore)
add_test(NAME slicer COMMAND test_slicer)

add_executable(test_raster_fill tests/test_raster_fill.cpp)
target_link_libraries(test_raster_fill PRIVATE 3DInkPlannerCore)
add_test(NAME raster_fill COMMAND test_raster_fill)

add_executable(test_transform tests/test_transform.cpp)
target_link_libraries(test_transform PRIVATE 3DInkPlannerCore)
add_test(NAME transform COMMAND test_transform)

add_executable(test_path_export tests/test_path_export.cpp)
target_link_libraries(test_path_export PRIVATE 3DInkPlannerCore)
add_test(NAME path_export COMMAND test_path_export)

add_executable(test_sample_stl tests/test_sample_stl.cpp)
target_link_libraries(test_sample_stl PRIVATE 3DInkPlannerCore)
target_compile_definitions(test_sample_stl PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME sample_stl COMMAND test_sample_stl)

add_executable(test_topology tests/test_topology.cpp)
target_link_libraries(test_topology PRIVATE 3DInkPlannerCore)
target_compile_definitions(test_topology PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME topology COMMAND test_topology)

add_executable(test_contour_classifier tests/test_contour_classifier.cpp)
target_link_libraries(test_contour_classifier PRIVATE 3DInkPlannerCore)
target_compile_definitions(test_contour_classifier PRIVATE SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME contour_classifier COMMAND test_contour_classifier)

add_executable(test_path_optimizer tests/test_path_optimizer.cpp)
target_link_libraries(test_path_optimizer PRIVATE 3DInkPlannerCore)
add_test(NAME path_optimizer COMMAND test_path_optimizer)

# 性能基准工具（不进 ctest，手动运行）：bench_pipeline.exe <stl> [layerHeight] [spacing]
add_executable(bench_pipeline bench/bench_pipeline.cpp)
target_link_libraries(bench_pipeline PRIVATE 3DInkPlannerCore)

# 开发期保留控制台便于查看 qDebug 日志；发布时可设 WIN32_EXECUTABLE TRUE 隐藏控制台。
