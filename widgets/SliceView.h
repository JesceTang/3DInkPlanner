#pragma once

#include <QWidget>

#include <vector>

#include "geometry/Polyline.h"
#include "path/ToolPath.h"

// 二维切片/路径预览控件（QPainter 绘制，指导 §4.1 H 路径预览）。
// 只负责可视化：轮廓、扫描路径、路径方向、起点/终点。核心算法在 geometry/path 层。
class SliceView : public QWidget {
    Q_OBJECT

public:
    explicit SliceView(QWidget *parent = nullptr);

    // 设置当前层数据（轮廓 + 路径段）并重绘。
    void setData(const std::vector<geometry::Polyline> &contours,
                 const std::vector<path::PathSegment> &segments);

    // 清空显示（无模型/无切片时）。
    void clear();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    std::vector<geometry::Polyline> m_contours;
    std::vector<path::PathSegment> m_segments;
};
