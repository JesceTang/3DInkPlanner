#pragma once

#include <QWidget>

#include <vector>

#include "geometry/Polygon.h"
#include "geometry/Polyline.h"
#include "path/ToolPath.h"

// 二维切片/路径预览控件（QPainter 绘制）。
// 只负责可视化：带孔轮廓（外环/内环分色）、断链、扫描路径、路径方向、起点/终点。
// 核心算法在 geometry/slicing/path 层。
class SliceView : public QWidget {
    Q_OBJECT

public:
    explicit SliceView(QWidget *parent = nullptr);

    // 设置当前层数据（带孔多边形 + 断链 + 路径段）并重绘。
    void setData(const std::vector<geometry::Polygon> &polygons,
                 const std::vector<geometry::Polyline> &openChains,
                 const std::vector<path::PathSegment> &segments);

    // 清空显示（无模型/无切片时）。
    void clear();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    std::vector<geometry::Polygon> m_polygons;
    std::vector<geometry::Polyline> m_openChains;
    std::vector<path::PathSegment> m_segments;
};
