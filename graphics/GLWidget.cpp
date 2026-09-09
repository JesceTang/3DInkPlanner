#include "graphics/GLWidget.h"

#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QWheelEvent>

#include <Eigen/Core>

#include <cstring>

namespace {

// Eigen（列主序）→ QMatrix4x4（列主序）。
QMatrix4x4 toQt(const Eigen::Matrix4f &m) {
    QMatrix4x4 q;
    std::memcpy(q.data(), m.data(), 16 * sizeof(float));
    return q;
}

} // namespace

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setDepthBufferSize(24);
    setFormat(format);
}

void GLWidget::setMesh(const geometry::Mesh &mesh) {
    m_mesh = mesh;
    m_hasMesh = true;
    m_dirty = true;
    focusView();
    update();
}

void GLWidget::initializeGL() {
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClearColor(0.15f, 0.15f, 0.17f, 1.0f);
    f->glEnable(GL_DEPTH_TEST);

    m_renderer.initialize();
}

void GLWidget::resizeGL(int w, int h) {
    m_camera.setAspectRatio(h > 0 ? w / float(h) : 1.0f);
}

void GLWidget::paintGL() {
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!m_hasMesh) {
        return;
    }

    // 首次或模型更新后上传 GPU。
    if (m_dirty) {
        m_renderer.upload(m_mesh);
        m_dirty = false;
    }

    const QMatrix4x4 view = toQt(m_camera.viewMatrix());
    const QMatrix4x4 proj = toQt(m_camera.projectionMatrix());
    const QMatrix4x4 vp = proj * view;

    // 模型矩阵暂为单位阵（按原始坐标显示），变换支持留作扩展。
    const QMatrix4x4 model;
    const QMatrix4x4 mvp = proj * view * model;

    const QVector3D lightDir(0.4f, 0.8f, 0.6f);

    m_renderer.renderMesh(mvp, model, lightDir);
    m_renderer.renderAxes(vp, m_mesh.maxDimension() * 0.5f);
}

void GLWidget::focusView() {
    m_camera.focus(m_mesh.center(), m_mesh.maxDimension() * 0.5f);
}

void GLWidget::mousePressEvent(QMouseEvent *e) {
    m_lastPos = e->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent *e) {
    const QPoint delta = e->pos() - m_lastPos;
    m_lastPos = e->pos();

    if (e->buttons() & Qt::LeftButton) {
        m_camera.orbit(delta.x() * 0.3f, delta.y() * 0.3f);
        update();
    } else if (e->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(delta.y()));
        update();
    }
}

void GLWidget::wheelEvent(QWheelEvent *e) {
    m_camera.zoom(static_cast<float>(e->angleDelta().y()));
    update();
}
