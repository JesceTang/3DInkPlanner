#pragma once

#include <QMainWindow>

#include <vector>

#include "geometry/GeometryTypes.h"
#include "path/ToolPath.h"
#include "slicing/Slicer.h"

class GLWidget;
class SliceView;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

// 主窗口：UI 编排层。打开 STL、切片、生成路径、预览、导出。
// 分层原则：MainWindow 只做流程编排与参数传递，核心几何/切片/路径算法在 geometry/slicing/path 层。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // 加载指定路径的 STL 模型（也用于命令行直接打开）。
    void loadStl(const QString &path);

private slots:
    void openStl();
    void sliceAndGeneratePath();
    void exportCurrentLayer();
    void onLayerChanged(int index);

private:
    void createMenus();
    void createStatusBar();
    QWidget *createControlBar();
    void updateSliceView();

    // 将一层轮廓转换为喷印路径（仅编排：遍历闭合轮廓调用 RasterFillGenerator）。
    std::vector<path::PathSegment> generatePaths(const slicing::Layer &layer,
                                                 double spacing) const;

    GLWidget *m_glWidget = nullptr;
    SliceView *m_sliceView = nullptr;

    geometry::Mesh m_mesh;
    bool m_hasMesh = false;

    std::vector<slicing::Layer> m_layers;
    std::vector<std::vector<path::PathSegment>> m_pathsPerLayer;

    QDoubleSpinBox *m_layerHeightSpin = nullptr;
    QDoubleSpinBox *m_spacingSpin = nullptr;
    QSlider *m_layerSlider = nullptr;
    QLabel *m_layerLabel = nullptr;
    QPushButton *m_exportButton = nullptr;
};
