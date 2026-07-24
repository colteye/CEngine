// Copyright (c) CEngine contributors.

#ifndef CENGINE_UI_RMLUI_RENDER_INTERFACE_H
#define CENGINE_UI_RMLUI_RENDER_INTERFACE_H

#include "renderer/ui_frame.h"

#include <RmlUi/Core/RenderInterface.h>

namespace CEngine::UI
{

/**
 * Captures RmlUi's render callbacks into one backend-neutral CEngine frame.
 */
class RmlUiRenderInterface final : public Rml::RenderInterface
{
  public:
    void BeginFrame(int width, int height);
    Renderer::UiFrame TakeFrame();

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                       Rml::Vector2i source_dimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;
    void SetTransform(const Rml::Matrix4f *transform) override;

  private:
    Renderer::UiFrame frame_;
    Renderer::UiScissor scissor_;
    glm::mat4 transform_ = glm::mat4(1.0f);
    bool scissor_enabled_ = false;
};

} // namespace CEngine::UI

#endif
