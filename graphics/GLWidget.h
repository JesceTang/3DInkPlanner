#pragma once

#include <QOpenGLWidget>

// OpenGL 视图：Milestone 0 仅负责创建 GL 上下文并清屏。
// Camera / 着色器 / 网格渲染在后续里程碑加入。
class GLWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
};
