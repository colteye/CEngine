//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/texture.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_TEXTURE_H
#define CENGINE_RENDERER_OPENGL_TEXTURE_H

#include "renderer/texture.h"

#include <glad/glad.h>

namespace CEngine::Renderer::OpenGL
{

// Sampling transfer function is a binding semantic. Base-color texels use
// sRGB; normals, material data, lightmaps, and HDR environments stay linear.
enum class TextureColorSpace
{
    Linear,
    Srgb,
};

/**
 * @brief TODO: Describe TextureLoader.
 */
class TextureLoader
{
  public:
    /**
     * @brief TODO: Describe Load.
     *
     * @param source TODO: Describe this parameter.
     * @param color_space TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    static GLuint Load(const Texture &source, TextureColorSpace color_space);
    /**
     * @brief TODO: Describe CreateSolid.
     *
     * @param red TODO: Describe this parameter.
     * @param green TODO: Describe this parameter.
     * @param blue TODO: Describe this parameter.
     * @param alpha TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    static GLuint CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha = 255);
};

} // namespace CEngine::Renderer::OpenGL
#endif
