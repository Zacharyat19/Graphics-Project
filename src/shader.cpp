#include "shader.h"

#include <cstdio>
#include <cstdlib>

#include <learnply2_resources.h>

#include <glad/gl.h>

void ShaderProgram::use()
{
    glUseProgram(_program);
}

void ShaderProgram::done()
{
    glUseProgram(0);
}

void ShaderProgram::compile_and_attach(unsigned type, const char *shader_path)
{
    printf("Compiling `%s'...", shader_path);

    GLuint shader = glCreateShader(type);
    if (!shader)
    {
        printf(" Failed (glCreateShader).\n");
        exit(1);
    }

    uint32_t size;
    const uint8_t *data = learnply2_find_resource(shader_path, &size);
    if (!data)
    {
        printf(" Failed (resource not found).\n");
        exit(1);
    }

    glShaderSource(shader, 1, reinterpret_cast<const GLchar *const *>(&data), reinterpret_cast<const GLint *>(&size));

    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        char log[1024];
        GLint size;
        glGetShaderInfoLog(shader, sizeof(log), &size, log);

        printf(" Failed (compiler error).\n");
        fwrite(log, size, 1, stdout);

        exit(1);
    }

    glAttachShader(_program, shader);
    _shaders.push_back(shader);

    printf(" OK.\n");
}

void ShaderProgram::link()
{
    printf("Linking...");

    glLinkProgram(_program);

    GLint status;
    glGetProgramiv(_program, GL_LINK_STATUS, &status);
    if (!status)
    {
        char log[1024];
        GLint size;
        glGetProgramInfoLog(_program, sizeof(log), &size, log);

        printf(" Failed (link error).\n");
        fwrite(log, size, 1, stdout);

        exit(1);
    }

    for (GLuint shader : _shaders)
        glDeleteShader(shader);
        
    printf(" OK.\n");
}

ShaderProgram::ShaderProgram() :
    _program(0)
{
    _program = glCreateProgram();
}

ShaderProgram::~ShaderProgram()
{
    glDeleteProgram(_program);

    for (GLuint shader : _shaders)
        glDeleteShader(shader);
}
