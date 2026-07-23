#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SHADER_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SHADER_H

#include <glad/glad.h>
#include <string>

namespace CEngine::Renderer::OpenGL
{

class ShaderProgram
{
  public:
    ShaderProgram() = default;
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram &operator=(const ShaderProgram &) = delete;
    ShaderProgram(ShaderProgram &&) = delete;
    ShaderProgram &operator=(ShaderProgram &&) = delete;
    ~ShaderProgram();

    bool Load(const std::string &vertex_file_path, const std::string &fragment_file_path);
    void Use() const
    {
        glUseProgram(program_id_);
    }
    [[nodiscard]] GLuint GetId() const
    {
        return program_id_;
    }

  private:
    GLuint program_id_ = 0;
};

} // namespace CEngine::Renderer::OpenGL
#endif
