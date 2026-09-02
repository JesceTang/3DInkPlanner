#include "widgets/SliceView.h"

#include <QColor>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>

#include <cmath>
#include <limits>

namespace {

// 在段中点画一个小箭头（“‹”形）指示路径方向。
void drawDirectionArrow(QPainter &painter, const QPointF &a, const QPointF &b,
                        double arrowLen) {
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double len = std::hypot(dx, dy);
    if (len < 1e-9) {
        return;
    }
    const double ux = dx / len;
    const double uy = dy / len;

    const QPointF mid((a.x() + b.x()) * 0.5, (a.y() + b.y()) * 0.5);
    const QPointF tip(mid.x() + ux * arrowLen * 0.5, mid.y() + uy * arrowLen * 0.5);
    const QPointF wing1(tip.x() - ux * arrowLen - uy * arrowLen,
                        tip.y() - uy * arrowLen + ux * arrowLen);
    const QPointF wing2(tip.x() - ux * arrowLen + uy * arrowLen,
                        tip.y() - uy * arrowLen - ux * arrowLen);
    painter.drawLine(tip, wing1);
    painter.drawLine(tip, wing2);
}

} // namespace

SliceView::SliceView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(240, 240);
    setAutoFillBackground(false);
}

void SliceView::setData(const std::vector<geometry::Polyline> &contours,
                        const std::vector<path::PathSegment> &segments) {
    m_contours = contours;
    m_segments = segments;
    update();
}

void SliceView::clear() {
    m_contours.clear();
    m_segments.clear();
    update();
}

void SliceView::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x1e, 0x1e, 0x24));
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_contours.empty() && m_segments.empty()) {
        painter.setPen(QColor(0x80, 0x80, 0x88));
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("加载模型并切片后，此处显示路径预览"));
        return;
    }

    // 计算所有几何点的包围盒。
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    const auto include = [&](const geometry::Point2D &p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    };
    for (const auto &c : m_contours) {
        for (const auto &p : c.points) {
            include(p);
        }
    }
    for (const auto &s : m_segments) {
        include(s.start);
        include(s.end);
    }

    // fit 到控件：保持比例、居中、留边距。
    const double bw = maxX - minX;
    const double bh = maxY - minY;
    const double pad = 24.0;
    double scale = 1.0;
    if (bw > 1e-9 && bh > 1e-9) {
        scale = std::min((width() - 2.0 * pad) / bw, (height() - 2.0 * pad) / bh);
    } else if (bw > 1e-9) {
        scale = (width() - 2.0 * pad) / bw;
    } else if (bh > 1e-9) {
        scale = (height() - 2.0 * pad) / bh;
    }
    const double cx = (minX + maxX) * 0.5;
    const double cy = (minY + maxY) * 0.5;

    // 世界坐标（y 向上）→ 控件坐标（y 向下）。
    const auto toWidget = [&](const geometry::Point2D &p) -> QPointF {
        return QPointF(width() * 0.5 + (p.x - cx) * scale,
                       height() * 0.5 - (p.y - cy) * scale);
    };

    // 1) 轮廓：浅灰闭合/开放折线。
    painter.setPen(QPen(QColor(0xc8, 0xc8, 0xd0), 1.0));
    for (const auto &c : m_contours) {
        if (c.points.empty()) {
            continue;
        }
        QPolygonF poly;
        poly.reserve(static_cast<int>(c.points.size() + (c.closed ? 1 : 0)));
        for (const auto &p : c.points) {
            poly << toWidget(p);
        }
        if (c.closed && poly.size() > 1) {
            poly << poly.first();
            painter.drawPolygon(poly);
        } else {
            painter.drawPolyline(poly);
        }
    }

    // 2) 扫描路径：Print 实线、Travel 虚线，带方向箭头。
    const double arrowLen = std::max(3.0, scale * 0.3);
    for (const auto &s : m_segments) {
        const QPointF a = toWidget(s.start);
        const QPointF b = toWidget(s.end);
        if (s.type == path::PathType::Print) {
            painter.setPen(QPen(QColor(0x2a, 0x9d, 0xe6), 1.5));
            painter.drawLine(a, b);
            drawDirectionArrow(painter, a, b, arrowLen);
        } else {
            painter.setPen(QPen(QColor(0xe0, 0x90, 0x30), 1.0, Qt::DashLine));
            painter.drawLine(a, b);
            drawDirectionArrow(painter, a, b, arrowLen);
        }
    }

    // 3) 起点 / 终点。
    if (!m_segments.empty()) {
        const QPointF start = toWidget(m_segments.front().start);
        const QPointF end = toWidget(m_segments.back().end);

        painter.setBrush(QColor(0x3c, 0xd6, 0x5c));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(start, 4.0, 4.0);

        painter.setPen(QPen(QColor(0xf0, 0x50, 0x50), 2.0));
        painter.drawLine(QPointF(end.x() - 4, end.y() - 4),
                         QPointF(end.x() + 4, end.y() + 4));
        painter.drawLine(QPointF(end.x() - 4, end.y() + 4),
                         QPointF(end.x() + 4, end.y() - 4));
    }
}
