#include "app/MainWindow.h"

#include <QAction>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

#include "graphics/GLWidget.h"
#include "io/PathExporter.h"
#include "io/STLReader.h"
#include "path/RasterFillGenerator.h"
#include "widgets/SliceView.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_glWidget(new GLWidget(this))
    , m_sliceView(new SliceView(this))
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_glWidget);
    splitter->addWidget(m_sliceView);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(splitter, 1);
    layout->addWidget(createControlBar());
    setCentralWidget(central);

    createMenus();
    createStatusBar();

    setWindowTitle(QStringLiteral("3DInkPlanner"));
    resize(1280, 800);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *openAction = fileMenu->addAction(QStringLiteral("&Open STL..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openStl);
    fileMenu->addSeparator();
    QAction *exportAction = fileMenu->addAction(QStringLiteral("&Export Current Layer CSV..."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportCurrentLayer);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("E&xit"), this, &QWidget::close);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&About"), this, [this] {
        statusBar()->showMessage(
            QStringLiteral("3DInkPlanner - 3D inkjet pre-processing demo"), 3000);
    });
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(QStringLiteral("No model loaded"));
}

QWidget *MainWindow::createControlBar()
{
    auto *bar = new QWidget(this);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    layout->addWidget(new QLabel(QStringLiteral("层高:"), bar));
    m_layerHeightSpin = new QDoubleSpinBox(bar);
    m_layerHeightSpin->setRange(0.01, 100.0);
    m_layerHeightSpin->setDecimals(3);
    m_layerHeightSpin->setSingleStep(0.05);
    m_layerHeightSpin->setValue(0.2);
    m_layerHeightSpin->setSuffix(QStringLiteral(" mm"));
    layout->addWidget(m_layerHeightSpin);

    layout->addWidget(new QLabel(QStringLiteral("填充间距:"), bar));
    m_spacingSpin = new QDoubleSpinBox(bar);
    m_spacingSpin->setRange(0.01, 100.0);
    m_spacingSpin->setDecimals(3);
    m_spacingSpin->setSingleStep(0.05);
    m_spacingSpin->setValue(0.4);
    m_spacingSpin->setSuffix(QStringLiteral(" mm"));
    layout->addWidget(m_spacingSpin);

    auto *sliceButton = new QPushButton(QStringLiteral("切片并生成路径"), bar);
    connect(sliceButton, &QPushButton::clicked, this, &MainWindow::sliceAndGeneratePath);
    layout->addWidget(sliceButton);

    layout->addSpacing(12);

    m_layerSlider = new QSlider(Qt::Horizontal, bar);
    m_layerSlider->setEnabled(false);
    connect(m_layerSlider, &QSlider::valueChanged, this, &MainWindow::onLayerChanged);
    layout->addWidget(m_layerSlider, 1);

    m_layerLabel = new QLabel(QStringLiteral("—"), bar);
    m_layerLabel->setMinimumWidth(120);
    layout->addWidget(m_layerLabel);

    m_exportButton = new QPushButton(QStringLiteral("导出当前层 CSV"), bar);
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &MainWindow::exportCurrentLayer);
    layout->addWidget(m_exportButton);

    return bar;
}

void MainWindow::openStl()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 STL 模型"), QString(), QStringLiteral("STL 模型 (*.stl)"));
    if (!path.isEmpty()) {
        loadStl(path);
    }
}

void MainWindow::loadStl(const QString &path)
{
    // Windows 下用宽字符路径，正确处理中文/空格。
    const io::StlReadResult result =
        io::readBinaryStl(std::filesystem::path(path.toStdWString()));
    if (!result.ok) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
                             QString::fromStdString(result.error));
        return;
    }

    m_mesh = result.mesh;
    m_hasMesh = true;
    m_glWidget->setMesh(m_mesh);

    // 新模型：清空旧的切片/路径结果。
    m_layers.clear();
    m_pathsPerLayer.clear();
    m_sliceView->clear();
    m_layerSlider->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_layerLabel->setText(QStringLiteral("—"));

    const auto size = m_mesh.maxBound - m_mesh.minBound;
    statusBar()->showMessage(
        QStringLiteral("三角面数: %1    尺寸: %2 × %3 × %4 mm")
            .arg(m_mesh.triangles.size())
            .arg(size.x(), 0, 'f', 2)
            .arg(size.y(), 0, 'f', 2)
            .arg(size.z(), 0, 'f', 2));
}

std::vector<path::PathSegment> MainWindow::generatePaths(
    const slicing::Layer &layer, double spacing) const {
    path::RasterFillGenerator raster;
    std::vector<path::PathSegment> result;
    for (const auto &contour : layer.contours) {
        // 仅闭合轮廓参与填充；开放/退化轮廓（非流形网格）暂不处理。
        if (!contour.closed || contour.points.size() < 3) {
            continue;
        }
        geometry::Polygon poly;
        poly.vertices = contour.points;
        auto segs = raster.generate(poly, spacing);
        result.insert(result.end(), segs.begin(), segs.end());
    }
    return result;
}

void MainWindow::sliceAndGeneratePath()
{
    if (!m_hasMesh) {
        QMessageBox::information(this, QStringLiteral("切片"),
                                 QStringLiteral("请先打开 STL 模型。"));
        return;
    }

    const double layerHeight = m_layerHeightSpin->value();
    const double spacing = m_spacingSpin->value();
    const double tolerance = geometry::kGeometryEpsilon;

    slicing::Slicer slicer;
    m_layers = slicer.sliceAll(m_mesh, layerHeight, tolerance);

    m_pathsPerLayer.clear();
    m_pathsPerLayer.reserve(m_layers.size());
    for (const auto &layer : m_layers) {
        m_pathsPerLayer.push_back(generatePaths(layer, spacing));
    }

    const int layerCount = static_cast<int>(m_layers.size());
    m_layerSlider->blockSignals(true);
    m_layerSlider->setEnabled(layerCount > 0);
    m_layerSlider->setRange(0, layerCount > 0 ? layerCount - 1 : 0);
    m_layerSlider->setValue(0);
    m_layerSlider->blockSignals(false);
    m_exportButton->setEnabled(layerCount > 0);

    statusBar()->showMessage(
        QStringLiteral("切片完成：%1 层（层高 %2 mm，填充间距 %3 mm）")
            .arg(layerCount)
            .arg(layerHeight, 0, 'f', 3)
            .arg(spacing, 0, 'f', 3));
    updateSliceView();
}

void MainWindow::onLayerChanged(int index)
{
    (void)index;
    updateSliceView();
}

void MainWindow::updateSliceView()
{
    const int idx = m_layerSlider->value();
    if (idx < 0 || idx >= static_cast<int>(m_layers.size())) {
        m_sliceView->clear();
        m_layerLabel->setText(QStringLiteral("—"));
        return;
    }
    m_sliceView->setData(m_layers[idx].contours, m_pathsPerLayer[idx]);
    m_layerLabel->setText(QStringLiteral("层 %1 / %2  (z=%3 mm)")
                              .arg(idx + 1)
                              .arg(m_layers.size())
                              .arg(m_layers[idx].z, 0, 'f', 2));
}

void MainWindow::exportCurrentLayer()
{
    const int idx = m_layerSlider->value();
    if (idx < 0 || idx >= static_cast<int>(m_layers.size())) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出路径 CSV"), QString(), QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    std::string err;
    const bool ok = io::writePathCsv(std::filesystem::path(path.toStdWString()),
                                     m_pathsPerLayer[idx], m_layers[idx].z, &err);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QString::fromStdString(err));
        return;
    }
    statusBar()->showMessage(
        QStringLiteral("已导出 %1 段路径: %2")
            .arg(m_pathsPerLayer[idx].size())
            .arg(path),
        5000);
}
