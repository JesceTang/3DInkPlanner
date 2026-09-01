#pragma once

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QVector3D>

#include "geometry/GeometryTypes.h"
#include "graphics/Shader.h"

// 网格渲染器：把 Mesh 上传为 VAO/VBO，用基础 Lambert 光照渲染；并绘制世界坐标轴。
class MeshRenderer {
public:
    // 在 GL 上下文就绪后调用一次：编译着色器、建立坐标轴 VBO。
    void initialize();

    // 上传网格（须在 GL 上下文 current 时调用）。
    void upload(const geometry::Mesh &mesh);

    // 渲染网格（MVP + 模型矩阵 + 方向光）。
    void renderMesh(const QMatrix4x4 &mvp, const QMatrix4x4 &model, const QVector3D &lightDir);

    // 渲染世界坐标轴（红 X / 绿 Y / 蓝 Z），axisLength 为轴长度。
    void renderAxes(const QMatrix4x4 &vp, float axisLength);

    bool hasMesh() const { return m_vertexCount > 0; }

private:
    Shader m_meshShader;
    Shader m_axesShader;

    QOpenGLVertexArrayObject m_meshVao;
    QOpenGLBuffer m_meshVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_axesVao;
    QOpenGLBuffer m_axesVbo{QOpenGLBuffer::VertexBuffer};

    int m_vertexCount = 0;
};
