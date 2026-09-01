#pragma once

#include <QMainWindow>

class GLWidget;

// 主窗口：承载 3D 视图（central widget），提供菜单与状态栏。
// 分层原则：MainWindow 只做 UI 编排，不直接操作几何/渲染/算法数据。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void createMenus();
    void createStatusBar();

    GLWidget *m_glWidget = nullptr;
};
