#pragma once

#include <QOpenGLShaderProgram>
#include <QString>

// 着色器程序封装：编译 + 链接，失败时返回错误日志。
class Shader {
public:
    // 编译并链接 vertex / fragment 着色器。失败时写入 errorOut 并返回 false。
    bool build(const char *vertexSrc, const char *fragmentSrc, QString *errorOut = nullptr);

    void bind();
    void release();

    QOpenGLShaderProgram &program() { return m_program; }

private:
    QOpenGLShaderProgram m_program;
};
