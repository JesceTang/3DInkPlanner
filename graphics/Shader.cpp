#include "graphics/Shader.h"

#include <QOpenGLShader>

bool Shader::build(const char *vertexSrc, const char *fragmentSrc, QString *errorOut) {
    const bool ok = m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSrc) &&
                    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSrc) &&
                    m_program.link();
    if (!ok && errorOut) {
        *errorOut = m_program.log();
    }
    return ok;
}

void Shader::bind() {
    m_program.bind();
}

void Shader::release() {
    m_program.release();
}
