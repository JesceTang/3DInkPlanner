#include "graphics/MeshRenderer.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <vector>

namespace {

const char *kMeshVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
void main() {
    vNormal = mat3(uModel) * aNormal;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char *kMeshFrag = R"GLSL(
#version 330 core
in vec3 vNormal;
uniform vec3 uLightDir;
uniform vec3 uColor;
out vec4 fragColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 l = normalize(uLightDir);
    float diff = max(dot(n, l), 0.0);
    float ambient = 0.3;
    fragColor = vec4(uColor * (ambient + 0.8 * diff), 1.0);
}
)GLSL";

const char *kAxesVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uVP;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)GLSL";

const char *kAxesFrag = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() {
    fragColor = vec4(vColor, 1.0);
}
)GLSL";

} // namespace

void MeshRenderer::initialize() {
    QString err;
    if (!m_meshShader.build(kMeshVert, kMeshFrag, &err)) {
        qWarning() << "mesh shader 编译失败:" << err;
    }
    if (!m_axesShader.build(kAxesVert, kAxesFrag, &err)) {
        qWarning() << "axes shader 编译失败:" << err;
    }

    // 坐标轴：单位长度，渲染时用 axisLength 缩放。X 红 / Y 绿 / Z 蓝。
    const float axesData[] = {
        0, 0, 0, 1, 0, 0,   1, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 1, 0,   0, 1, 0, 0, 1, 0,
        0, 0, 0, 0, 0, 1,   0, 0, 1, 0, 0, 1,
    };
    m_axesVao.create();
    m_axesVao.bind();
    m_axesVbo.create();
    m_axesVbo.bind();
    m_axesVbo.allocate(axesData, static_cast<int>(sizeof(axesData)));
    const int stride = 6 * sizeof(float);
    m_axesShader.program().enableAttributeArray(0);
    m_axesShader.program().setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
    m_axesShader.program().enableAttributeArray(1);
    m_axesShader.program().setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, stride);
    m_axesVao.release();
    m_axesVbo.release();
}

void MeshRenderer::upload(const geometry::Mesh &mesh) {
    m_vertexCount = static_cast<int>(mesh.triangles.size() * 3);

    // interleaved: 每个顶点 position(3) + normal(3)。
    std::vector<float> data;
    data.reserve(mesh.triangles.size() * 3 * 6);
    for (const geometry::Triangle &t : mesh.triangles) {
        const geometry::Vec3f verts[3] = {t.v0, t.v1, t.v2};
        for (const geometry::Vec3f &v : verts) {
            data.push_back(v.x());
            data.push_back(v.y());
            data.push_back(v.z());
            data.push_back(t.normal.x());
            data.push_back(t.normal.y());
            data.push_back(t.normal.z());
        }
    }

    m_meshVao.create();
    m_meshVao.bind();
    m_meshVbo.create();
    m_meshVbo.bind();
    m_meshVbo.allocate(data.data(), static_cast<int>(data.size() * sizeof(float)));
    const int stride = 6 * sizeof(float);
    m_meshShader.program().enableAttributeArray(0);
    m_meshShader.program().setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
    m_meshShader.program().enableAttributeArray(1);
    m_meshShader.program().setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, stride);
    m_meshVao.release();
    m_meshVbo.release();
}

void MeshRenderer::renderMesh(const QMatrix4x4 &mvp, const QMatrix4x4 &model,
                              const QVector3D &lightDir) {
    if (m_vertexCount <= 0) {
        return;
    }

    m_meshShader.bind();
    m_meshShader.program().setUniformValue("uMVP", mvp);
    m_meshShader.program().setUniformValue("uModel", model);
    m_meshShader.program().setUniformValue("uLightDir", lightDir);
    m_meshShader.program().setUniformValue("uColor", QVector3D(0.55f, 0.58f, 0.65f));

    m_meshVao.bind();
    QOpenGLContext::currentContext()->functions()->glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
    m_meshVao.release();
    m_meshShader.release();
}

void MeshRenderer::renderAxes(const QMatrix4x4 &vp, float axisLength) {
    m_axesShader.bind();
    QMatrix4x4 scale;
    scale.scale(axisLength);
    m_axesShader.program().setUniformValue("uVP", vp * scale);

    m_axesVao.bind();
    QOpenGLContext::currentContext()->functions()->glDrawArrays(GL_LINES, 0, 6);
    m_axesVao.release();
    m_axesShader.release();
}
