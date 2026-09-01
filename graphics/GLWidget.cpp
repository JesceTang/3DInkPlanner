#include "GLWidget.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // 3.3 Core Profile，为 Milestone 1 的着色器 / VAO 打基础。
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(3, 3);
    format.setDepthBufferSize(24);
    setFormat(format);
}

void GLWidget::initializeGL()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // 深灰清屏色
    f->glClearColor(0.15f, 0.15f, 0.17f, 1.0f);
    f->glEnable(GL_DEPTH_TEST);

    const GLubyte *version = f->glGetString(GL_VERSION);
    qDebug() << "OpenGL version:"
             << (version ? reinterpret_cast<const char *>(version) : "unknown");
}

void GLWidget::resizeGL(int w, int h)
{
    QOpenGLContext::currentContext()->functions()->glViewport(0, 0, w, h);
}

void GLWidget::paintGL()
{
    QOpenGLContext::currentContext()->functions()->glClear(
        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
