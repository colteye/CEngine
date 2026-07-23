#include "entity/skybox_entity.h"

#include "assets/asset_database.h"
#include "engine_context.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <stdexcept>

namespace CEngine::Entities {
std::string_view SkyboxEntity::Classname() const { return "skybox"; }

void SkyboxEntity::Initialize(EngineContext& context)
{
    if (context.assets == nullptr || context.scene == nullptr)
        throw std::runtime_error("skybox requires scene assets");
    const Assets::AssetHandle panorama_asset =
        context.scene->AssetReference(panorama.index);
    if (!panorama_asset ||
        context.assets->Type(panorama_asset) != Assets::AssetType::Texture)
        throw std::runtime_error("skybox panorama reference is invalid");
    panorama_ = context.assets->LoadTexture(panorama_asset, false);
    if (!panorama_)
        throw std::runtime_error("could not load skybox panorama");
}

void SkyboxEntity::Update(EngineContext& context, float)
{
    if (context.rendering == nullptr) return;
    Renderer::ImageBasedLighting sky;
    sky.panorama = panorama_.get();
    sky.sky_intensity = intensity;
    sky.lighting_intensity = intensity;
    sky.rotation_radians = rotation_radians;
    sky.enabled = Enabled() && enabled;
    context.rendering->SetImageBasedLighting(sky);
    if (sky.enabled)
    {
        Renderer::AmbientLighting ambient = context.rendering->GetAmbientLighting();
        ambient.enabled = false;
        context.rendering->SetAmbientLighting(ambient);
    }
}

void SkyboxEntity::Shutdown(EngineContext& context)
{
    if (context.rendering != nullptr)
        context.rendering->SetImageBasedLighting(Renderer::ImageBasedLighting{});
    panorama_.reset();
}
}
