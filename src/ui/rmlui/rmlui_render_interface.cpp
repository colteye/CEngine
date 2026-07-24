//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/rmlui/rmlui_render_interface.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ui/rmlui/rmlui_render_interface.h"

#include "log.h"

#include <RmlUi/Core/Matrix4.h>
#include <RmlUi/Core/Vertex.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::UI
{
namespace
{

struct Geometry
{
    std::vector<Rml::Vertex> vertices;
    std::vector<int> indices;
};

struct Texture
{
    std::shared_ptr<const Renderer::UiTexture> value;
};

} // namespace

void RmlUiRenderInterface::BeginFrame(int width, int height)
{
    frame_.Clear();
    frame_.width = std::max(width, 0);
    frame_.height = std::max(height, 0);
    scissor_ = {};
    transform_ = glm::mat4(1.0f);
    scissor_enabled_ = false;
}

Renderer::UiFrame RmlUiRenderInterface::TakeFrame()
{
    return std::move(frame_);
}

Rml::CompiledGeometryHandle RmlUiRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                  Rml::Span<const int> indices)
{
    if (vertices.empty() || indices.empty())
    {
        return {};
    }
    auto geometry = std::make_unique<Geometry>();
    geometry->vertices.assign(vertices.begin(), vertices.end());
    geometry->indices.assign(indices.begin(), indices.end());
    return reinterpret_cast<Rml::CompiledGeometryHandle>(geometry.release());
}

void RmlUiRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation,
                                          Rml::TextureHandle texture_handle)
{
    const auto *geometry = reinterpret_cast<const Geometry *>(handle);
    if (geometry == nullptr || geometry->vertices.empty() || geometry->indices.empty())
    {
        return;
    }
    if (frame_.vertices.size() >
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) - geometry->vertices.size())
    {
        Logging::Logger::Get().Error("ui", "RmlUi frame exceeded the 32-bit vertex range");
        return;
    }
    for (int index : geometry->indices)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= geometry->vertices.size())
        {
            Logging::Logger::Get().Error("ui", "RmlUi emitted an invalid geometry index");
            return;
        }
    }

    const std::uint32_t base_vertex = static_cast<std::uint32_t>(frame_.vertices.size());
    const std::uint32_t first_index = static_cast<std::uint32_t>(frame_.indices.size());
    frame_.vertices.reserve(frame_.vertices.size() + geometry->vertices.size());
    for (const Rml::Vertex &source : geometry->vertices)
    {
        Renderer::UiVertex vertex;
        vertex.position_x = source.position.x;
        vertex.position_y = source.position.y;
        vertex.red = source.colour.red;
        vertex.green = source.colour.green;
        vertex.blue = source.colour.blue;
        vertex.alpha = source.colour.alpha;
        vertex.texture_u = source.tex_coord.x;
        vertex.texture_v = source.tex_coord.y;
        frame_.vertices.push_back(vertex);
    }
    frame_.indices.reserve(frame_.indices.size() + geometry->indices.size());
    for (int index : geometry->indices)
    {
        frame_.indices.push_back(base_vertex + static_cast<std::uint32_t>(index));
    }

    Renderer::UiBatch batch;
    batch.first_index = first_index;
    batch.index_count = static_cast<std::uint32_t>(geometry->indices.size());
    batch.translation = {translation.x, translation.y};
    batch.transform = transform_;
    batch.scissor = scissor_;
    batch.scissor_enabled = scissor_enabled_;
    if (texture_handle != 0)
    {
        const auto *texture = reinterpret_cast<const Texture *>(texture_handle);
        batch.texture = texture->value;
    }
    frame_.batches.push_back(std::move(batch));
}

void RmlUiRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle handle)
{
    delete reinterpret_cast<Geometry *>(handle);
}

Rml::TextureHandle RmlUiRenderInterface::LoadTexture(Rml::Vector2i &texture_dimensions, const Rml::String &source)
{
    texture_dimensions = {};
    Logging::Logger::Get().Warning("ui",
                                   "RmlUi image source is not a cooked CEngine texture and was rejected: " + source);
    return {};
}

Rml::TextureHandle RmlUiRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source,
                                                         Rml::Vector2i source_dimensions)
{
    if (source_dimensions.x <= 0 || source_dimensions.y <= 0)
    {
        return {};
    }
    const std::size_t expected =
        static_cast<std::size_t>(source_dimensions.x) * static_cast<std::size_t>(source_dimensions.y) * 4u;
    if (source.size() != expected)
    {
        Logging::Logger::Get().Error("ui", "RmlUi generated an invalid RGBA texture");
        return {};
    }
    auto texture_value = std::make_shared<Renderer::UiTexture>();
    texture_value->width = source_dimensions.x;
    texture_value->height = source_dimensions.y;
    texture_value->rgba.assign(source.begin(), source.end());
    auto texture = std::make_unique<Texture>();
    texture->value = std::move(texture_value);
    return reinterpret_cast<Rml::TextureHandle>(texture.release());
}

void RmlUiRenderInterface::ReleaseTexture(Rml::TextureHandle handle)
{
    delete reinterpret_cast<Texture *>(handle);
}

void RmlUiRenderInterface::EnableScissorRegion(bool enable)
{
    scissor_enabled_ = enable;
}

void RmlUiRenderInterface::SetScissorRegion(Rml::Rectanglei region)
{
    scissor_.x = region.Left();
    scissor_.y = region.Top();
    scissor_.width = std::max(region.Width(), 0);
    scissor_.height = std::max(region.Height(), 0);
}

void RmlUiRenderInterface::SetTransform(const Rml::Matrix4f *transform)
{
    transform_ = transform != nullptr ? glm::make_mat4(transform->data()) : glm::mat4(1.0f);
}

} // namespace CEngine::UI
