//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/shader.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SHADER_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SHADER_H

#include <glad/glad.h>
#include <string>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe ShaderProgram.
 */
class ShaderProgram
{
  public:
    /**
     * @brief TODO: Describe ShaderProgram.
     */
    ShaderProgram() = default;
    /**
     * @brief TODO: Describe ShaderProgram.
     */
    ShaderProgram(const ShaderProgram &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShaderProgram &operator=(const ShaderProgram &) = delete;
    /**
     * @brief TODO: Describe ShaderProgram.
     */
    ShaderProgram(ShaderProgram &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShaderProgram &operator=(ShaderProgram &&) = delete;
    /**
     * @brief TODO: Describe ~ShaderProgram.
     */
    ~ShaderProgram();

    /**
     * @brief TODO: Describe Load.
     *
     * @param vertex_file_path TODO: Describe this parameter.
     * @param fragment_file_path TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Load(const std::string &vertex_file_path, const std::string &fragment_file_path);
    /**
     * @brief TODO: Describe Use.
     */
    void Use() const
    {
        glUseProgram(program_id_);
    }
    /**
     * @brief TODO: Describe GetId.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] GLuint GetId() const
    {
        return program_id_;
    }

  private:
    GLuint program_id_ = 0;
};

} // namespace CEngine::Renderer::OpenGL
#endif
