// Copyright (c) CEngine contributors.

#ifndef CENGINE_RENDERER_OPENGL_UI_RENDERER_H
#define CENGINE_RENDERER_OPENGL_UI_RENDERER_H

#include "renderer/ui_frame.h"

#include <glad/glad.h>

#include <memory>
#include <unordered_map>

namespace CEngine::Renderer::OpenGL
{

class UiRenderer
{
  public:
    ~UiRenderer();

    bool Initialize();
    void Shutdown();
    void Render(const UiFrame &frame, int drawable_width, int drawable_height);

  private:
    struct TextureResource
    {
        GLuint id = 0;
        std::weak_ptr<const UiTexture> owner;
    };

    GLuint ResolveTexture(const std::shared_ptr<const UiTexture> &texture);
    void PruneTextures();

    GLuint program_ = 0;
    GLuint vertex_array_ = 0;
    GLuint vertex_buffer_ = 0;
    GLuint index_buffer_ = 0;
    GLuint white_texture_ = 0;
    GLint viewport_location_ = -1;
    GLint translation_location_ = -1;
    GLint transform_location_ = -1;
    GLint use_texture_location_ = -1;
    std::unordered_map<const UiTexture *, TextureResource> textures_;
};

} // namespace CEngine::Renderer::OpenGL

#endif
