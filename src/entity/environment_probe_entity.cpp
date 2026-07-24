//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#include "entity/environment_probe_entity.h"

#include "assets/store.h"
#include "context.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <stdexcept>

namespace CEngine::Entities
{

std::string_view EnvironmentProbeEntity::Classname() const
{
    return "environment_probe";
}

Renderer::EnvironmentProbe EnvironmentProbeEntity::RenderProbe() const
{
    Renderer::EnvironmentProbe probe;
    probe.panorama = panorama_;
    probe.transform = GetTransform().world_matrix;
    probe.blend_distance = blend_distance;
    probe.intensity = intensity;
    probe.enabled = Enabled() && enabled;
    return probe;
}

void EnvironmentProbeEntity::Initialize(Context &context)
{
    if (context.assets == nullptr || context.scene == nullptr)
    {
        throw std::runtime_error("environment probe requires scene assets");
    }
    const Assets::Reference *panorama_asset = context.scene->AssetReference(panorama.index);
    if (panorama_asset == nullptr || panorama_asset->type != Assets::Type::Texture)
    {
        throw std::runtime_error("environment probe panorama reference is invalid");
    }
    panorama_ = context.assets->LoadTexture(*panorama_asset, false);
    if (!panorama_)
    {
        throw std::runtime_error("could not load environment probe panorama");
    }
    if (context.rendering != nullptr)
    {
        renderer_probe_ = context.rendering->RegisterEnvironmentProbe(RenderProbe());
        if (!renderer_probe_)
        {
            throw std::runtime_error("renderer rejected environment probe");
        }
    }
}

void EnvironmentProbeEntity::Update(Context &context, float /*delta_seconds*/)
{
    if (context.rendering != nullptr && renderer_probe_)
    {
        context.rendering->UpdateEnvironmentProbe(renderer_probe_, RenderProbe());
    }
}

void EnvironmentProbeEntity::OnEnabledChanged(Context &context, bool /*enabled*/)
{
    Update(context, 0.0f);
}

void EnvironmentProbeEntity::Shutdown(Context &context)
{
    if (context.rendering != nullptr && renderer_probe_)
    {
        context.rendering->RemoveEnvironmentProbe(renderer_probe_);
    }
    renderer_probe_ = {};
    panorama_.reset();
}

} // namespace CEngine::Entities
