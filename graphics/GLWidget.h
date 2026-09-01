#pragma once

#include <QOpenGLWidget>
#include <QPoint>

#include "geometry/GeometryTypes.h"
#include "graphics/Camera.h"
#include "graphics/MeshRenderer.h"

// OpenGL 视图：加载网格、相机交互（左键旋转 / 中键或右键平移 / 滚轮缩放）、渲染。
class GLWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit GLWidget(QWidget *parent = nullptr);

    // 加载新模型（仅存 CPU 数据，GPU 上传在 paintGL 中完成）。
    void setMesh(const geometry::Mesh &mesh);
    bool hasMesh() const { return m_hasMesh; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void focusView();

    Camera m_camera;
    MeshRenderer m_renderer;
    geometry::Mesh m_mesh;
    bool m_hasMesh = false;
    bool m_dirty = false;
    QPoint m_lastPos;
};
