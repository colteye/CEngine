#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>

namespace CEngine::Renderer
{

class ShaderProgram
{
  public:
    ShaderProgram() = default;
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram &operator=(const ShaderProgram &) = delete;
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

} // namespace CEngine::Renderer
#endif
