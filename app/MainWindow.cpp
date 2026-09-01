#include "app/MainWindow.h"

#include <QMenuBar>
#include <QStatusBar>

#include "graphics/GLWidget.h"

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
    fileMenu->addAction(QStringLiteral("&Open STL..."), this, [this] {
        // Milestone 1 再实现 STL 导入，此处仅占位提示。
        statusBar()->showMessage(QStringLiteral("STL import: Milestone 1"), 3000);
    });
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
