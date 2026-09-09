#include "app/MainWindow.h"

#include <QAction>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>

#include "graphics/GLWidget.h"
#include "io/PathExporter.h"
#include "io/STLReader.h"
#include "path/PathOptimizer.h"
#include "topology/MeshBuilder.h"
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
    QAction *exportGcodeAction = fileMenu->addAction(QStringLiteral("Export &All Layers G-code..."));
    connect(exportGcodeAction, &QAction::triggered, this, &MainWindow::exportAllLayersGcode);
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

    layout->addWidget(new QLabel(QStringLiteral("z表:"), bar));
    m_zListEdit = new QLineEdit(bar);
    m_zListEdit->setPlaceholderText(QStringLiteral("留空=等距；或逗号分隔，如 0.5,1.2,3.0"));
    m_zListEdit->setMinimumWidth(200);
    m_zListEdit->setClearButtonEnabled(true);
    layout->addWidget(m_zListEdit);

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
        this, QStringLiteral("打开 STL 模型"), QString(),
        QStringLiteral("STL 模型 (*.stl);;所有文件 (*)"));
    if (!path.isEmpty()) {
        loadStl(path);
    }
}

void MainWindow::loadStl(const QString &path)
{
    // Windows 下用宽字符路径，正确处理中文/空格。readStl 自动检测 Binary/ASCII。
    const io::StlReadResult result =
        io::readStl(std::filesystem::path(path.toStdWString()));
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

    // 拓扑体检：顶点焊接 + 边表统计（只报告，不修改原始网格）。
    const topology::MeshBuilder builder;
    int degenerate = 0;
    const topology::IndexedMesh indexed = builder.buildIndexedMesh(
        m_mesh, m_mesh.adaptiveTolerance(), &degenerate);
    const topology::TopologyReport topo = builder.analyze(indexed);

    statusBar()->showMessage(
        QStringLiteral("三角面数: %1    尺寸: %2 × %3 × %4 mm    顶点: %5    边界边: %6    非流形边: %7    分量: %8    体积: %9 mm³")
            .arg(m_mesh.triangles.size())
            .arg(size.x(), 0, 'f', 2)
            .arg(size.y(), 0, 'f', 2)
            .arg(size.z(), 0, 'f', 2)
            .arg(topo.vertexCount)
            .arg(topo.boundaryEdgeCount)
            .arg(topo.nonManifoldEdgeCount)
            .arg(topo.componentCount)
            .arg(topo.signedVolume, 0, 'f', 1));

    if (topo.boundaryEdgeCount > 0 || topo.nonManifoldEdgeCount > 0 || degenerate > 0) {
        QMessageBox::warning(
            this, QStringLiteral("拓扑体检"),
            QStringLiteral("网格非完全闭合：边界边 %1，非流形边 %2，退化面 %3。\n"
                           "切片仍可进行，但断链层将在预览中以红色虚线标出。")
                .arg(topo.boundaryEdgeCount)
                .arg(topo.nonManifoldEdgeCount)
                .arg(degenerate));
    }
}

std::vector<path::PathSegment> MainWindow::generatePaths(
    const slicing::Layer &layer, double spacing) const {
    // PathOptimizer：多岛最近邻排序 + 断点插入 Travel 连接段（含长度统计）。
    const path::PathOptimizer optimizer;
    return optimizer.optimize(layer.classified.polygons, spacing).segments;
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
    const double tolerance = m_mesh.adaptiveTolerance();  // 按模型尺寸自适应

    // 可变层厚：z 表非空时按显式高度切片，否则等距层高。
    slicing::Slicer slicer;
    const QString zText = m_zListEdit->text().trimmed();
    bool variableMode = false;
    if (zText.isEmpty()) {
        m_layers = slicer.sliceAll(m_mesh, layerHeight, tolerance);
    } else {
        std::vector<double> zList;
        const QStringList parts = zText.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            bool ok = false;
            const double z = part.trimmed().toDouble(&ok);
            if (!ok) {
                QMessageBox::warning(this, QStringLiteral("切片"),
                                     QStringLiteral("z 表包含非法数值: %1").arg(part.trimmed()));
                return;
            }
            zList.push_back(z);
        }
        m_layers = slicer.sliceAll(m_mesh, zList, tolerance);
        variableMode = true;
    }

    m_pathsPerLayer.clear();
    m_pathsPerLayer.reserve(m_layers.size());
    size_t openChainTotal = 0;
    size_t warningTotal = 0;
    for (const auto &layer : m_layers) {
        m_pathsPerLayer.push_back(generatePaths(layer, spacing));
        openChainTotal += layer.classified.openChains.size();
        warningTotal += layer.classified.warnings.size();
    }

    const int layerCount = static_cast<int>(m_layers.size());
    m_layerSlider->blockSignals(true);
    m_layerSlider->setEnabled(layerCount > 0);
    m_layerSlider->setRange(0, layerCount > 0 ? layerCount - 1 : 0);
    m_layerSlider->setValue(0);
    m_layerSlider->blockSignals(false);
    m_exportButton->setEnabled(layerCount > 0);

    QString msg = variableMode
        ? QStringLiteral("切片完成：%1 层（可变层厚，填充间距 %2 mm）")
              .arg(layerCount).arg(spacing, 0, 'f', 3)
        : QStringLiteral("切片完成：%1 层（层高 %2 mm，填充间距 %3 mm）")
              .arg(layerCount).arg(layerHeight, 0, 'f', 3).arg(spacing, 0, 'f', 3);
    if (openChainTotal > 0 || warningTotal > 0) {
        msg += QStringLiteral("    ⚠ 断链 %1 条，告警 %2 条")
                   .arg(openChainTotal).arg(warningTotal);
    }
    statusBar()->showMessage(msg);
    updateSliceView();
}

void MainWindow::exportAllLayersGcode()
{
    if (m_layers.empty()) {
        QMessageBox::information(this, QStringLiteral("导出 G-code"),
                                 QStringLiteral("请先切片并生成路径。"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出全部层 G-code"), QString(),
        QStringLiteral("G-code 文件 (*.gcode *.txt)"));
    if (path.isEmpty()) {
        return;
    }

    std::vector<double> layerZs;
    layerZs.reserve(m_layers.size());
    for (const auto &layer : m_layers) {
        layerZs.push_back(layer.z);
    }

    std::string err;
    const bool ok = io::writeAllLayersGcode(
        std::filesystem::path(path.toStdWString()), m_pathsPerLayer, layerZs, &err);
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("导出失败"),
                             QString::fromStdString(err));
        return;
    }
    statusBar()->showMessage(
        QStringLiteral("已导出 %1 层 G-code: %2").arg(m_layers.size()).arg(path),
        5000);
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
    const auto &layer = m_layers[idx];
    m_sliceView->setData(layer.classified.polygons, layer.classified.openChains,
                         m_pathsPerLayer[idx]);
    QString label = QStringLiteral("层 %1 / %2  (z=%3 mm)")
                        .arg(idx + 1)
                        .arg(m_layers.size())
                        .arg(layer.z, 0, 'f', 2);
    if (!layer.classified.openChains.empty()) {
        label += QStringLiteral("  ⚠断链 %1").arg(layer.classified.openChains.size());
    }
    m_layerLabel->setText(label);
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
