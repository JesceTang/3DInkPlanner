#include "app/MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>

#include <filesystem>

#include "graphics/GLWidget.h"
#include "io/STLReader.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_glWidget(new GLWidget(this))
{
    setCentralWidget(m_glWidget);
    createMenus();
    createStatusBar();

    setWindowTitle(QStringLiteral("3DInkPlanner"));
    resize(1024, 768);
}

void MainWindow::createMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction *openAction = fileMenu->addAction(QStringLiteral("&Open STL..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openStl);
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

    m_glWidget->setMesh(result.mesh);

    const auto size = result.mesh.maxBound - result.mesh.minBound;
    statusBar()->showMessage(
        QStringLiteral("三角面数: %1    尺寸: %2 × %3 × %4 mm")
            .arg(result.mesh.triangles.size())
            .arg(size.x(), 0, 'f', 2)
            .arg(size.y(), 0, 'f', 2)
            .arg(size.z(), 0, 'f', 2));
}
