#include "slicing/Slicer.h"

#include "slicing/ContourBuilder.h"
#include "slicing/SegmentConnector.h"
#include "slicing/TrianglePlaneIntersection.h"

namespace slicing {

std::vector<geometry::Polyline> Slicer::slice(
    const geometry::Mesh &mesh, double z, double tolerance) const {
    std::vector<geometry::Segment2D> segments;
    segments.reserve(mesh.triangles.size());

    // 1. 逐三角形求交，收集该层的全部线段。
    for (const geometry::Triangle &tri : mesh.triangles) {
        auto seg = intersectTrianglePlane(tri, z, tolerance);
        if (seg) {
            segments.push_back(*seg);
        }
    }

    // 2. 拼接成有序轮廓。
    SegmentConnector connector;
    auto polys = connector.connect(segments, tolerance);

    // 3. 移除共线冗余顶点（相邻三角形共享边产生的中间点）。
    for (geometry::Polyline &poly : polys) {
        poly = simplifyContour(poly, tolerance);
    }
    return polys;
}

std::vector<Layer> Slicer::sliceAll(
    const geometry::Mesh &mesh, double layerHeight, double tolerance) const {
    std::vector<Layer> layers;
    if (mesh.triangles.empty() || layerHeight <= 0.0) {
        return layers;
    }

    const double zMin = mesh.minBound.z();
    const double zMax = mesh.maxBound.z();

    // 第一层偏移半个层高，避免切片面正好落在底面；后续每层递增 layerHeight。
    double z = zMin + layerHeight * 0.5;
    while (z < zMax) {
        Layer layer;
        layer.z = z;
        layer.contours = slice(mesh, z, tolerance);
        layers.push_back(std::move(layer));
        z += layerHeight;
    }
    return layers;
}

} // namespace slicing
