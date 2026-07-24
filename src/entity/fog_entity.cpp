//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/fog_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/fog_entity.h"

#include "context.h"
#include "renderer/render_system.h"

namespace CEngine::Entities
{
/**
 * @brief TODO: Describe FogEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view FogEntity::Classname() const
{
    return "fog";
}

/**
 * @brief TODO: Describe FogEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 */
void FogEntity::Update(Context &context, float /*unused*/)
{
    if (context.rendering == nullptr)
    {
        return;
    }
    Renderer::ExponentialHeightFog fog;
    fog.inscattering_color = {inscattering_color.x, inscattering_color.y, inscattering_color.z};
    fog.density = density;
    fog.height_falloff = height_falloff;
    fog.base_height = GetTransform().position.z;
    fog.start_distance = start_distance;
    fog.max_opacity = max_opacity;
    fog.cutoff_distance = cutoff_distance;
    fog.enabled = Enabled() && enabled;
    context.rendering->SetExponentialHeightFog(fog);
}

void FogEntity::OnEnabledChanged(Context &context, bool /*enabled*/)
{
    Update(context, 0.0f);
}

/**
 * @brief TODO: Describe FogEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void FogEntity::Shutdown(Context &context)
{
    if (context.rendering != nullptr)
    {
        context.rendering->SetExponentialHeightFog(Renderer::ExponentialHeightFog{});
    }
}
} // namespace CEngine::Entities
