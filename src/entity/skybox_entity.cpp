//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/skybox_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/skybox_entity.h"

#include "assets/asset_store.h"
#include "context.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <stdexcept>

namespace CEngine::Entities
{
/**
 * @brief TODO: Describe SkyboxEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view SkyboxEntity::Classname() const
{
    return "skybox";
}

/**
 * @brief TODO: Describe SkyboxEntity::Initialize.
 *
 * @param context TODO: Describe this parameter.
 */
void SkyboxEntity::Initialize(Context &context)
{
    if (context.assets == nullptr || context.scene == nullptr)
    {
        throw std::runtime_error("skybox requires scene assets");
    }
    const Assets::AssetReference *panorama_asset = context.scene->AssetReference(panorama.index);
    if (panorama_asset == nullptr || panorama_asset->type != Assets::AssetType::Texture)
    {
        throw std::runtime_error("skybox panorama reference is invalid");
    }
    panorama_ = context.assets->LoadTexture(*panorama_asset, false);
    if (!panorama_)
    {
        throw std::runtime_error("could not load skybox panorama");
    }
}

/**
 * @brief TODO: Describe SkyboxEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 */
void SkyboxEntity::Update(Context &context, float /*unused*/)
{
    if (context.rendering == nullptr)
    {
        return;
    }
    Renderer::ImageBasedLighting sky;
    sky.panorama = panorama_;
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

/**
 * @brief TODO: Describe SkyboxEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void SkyboxEntity::Shutdown(Context &context)
{
    if (context.rendering != nullptr)
    {
        context.rendering->SetImageBasedLighting(Renderer::ImageBasedLighting{});
    }
    panorama_.reset();
}
} // namespace CEngine::Entities
