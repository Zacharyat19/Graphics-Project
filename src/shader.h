#pragma once

#include <glm/glm.hpp>
#include <vector>

class ShaderProgram
{
public:
    // Returns the OpenGL id of the shader program.
    inline unsigned id() const { return _program; }

    void use();
    void done();
    
    void compile_and_attach(unsigned type, const char *shader_path);
    void link();

    ShaderProgram();
    ~ShaderProgram();
private:
    unsigned _program;
    std::vector<unsigned> _shaders;
};
