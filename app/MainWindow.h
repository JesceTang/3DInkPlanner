#pragma once

#include <QMainWindow>

class GLWidget;

// 主窗口：承载 3D 视图（central widget），提供菜单与状态栏。
// 分层原则：MainWindow 只做 UI 编排（打开文件、显示信息），核心算法在 geometry/io/slicing/path 层。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // 加载指定路径的 STL 模型（也用于命令行直接打开）。
    void loadStl(const QString &path);

private slots:
    void openStl();

private:
    void createMenus();
    void createStatusBar();

    GLWidget *m_glWidget = nullptr;
};
