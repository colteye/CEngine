//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/light_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/light_entity.h"

#include "context.h"
#include "renderer/render_system.h"

#include <cmath>
#include <stdexcept>

namespace CEngine::Entities
{
/**
 * @brief TODO: Describe LightEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view LightEntity::Classname() const
{
    return "light";
}

/**
 * @brief TODO: Describe LightEntity::RenderLight.
 *
 * @return TODO: Describe the return value.
 */
Renderer::Light LightEntity::RenderLight() const
{
    Renderer::Light light;
    if (type == Generated::EngineEntities::LightType::Sun)
    {
        light.type = Renderer::LightType::Directional;
    }
    else if (type == Generated::EngineEntities::LightType::Spot)
    {
        light.type = Renderer::LightType::Spot;
    }
    light.position = GetTransform().position;
    light.direction = glm::normalize(glm::vec3(GetTransform().world_matrix * glm::vec4(0, 0, -1, 0)));
    light.color = {color.x, color.y, color.z};
    light.intensity = intensity;
    light.range = range;
    light.spot_inner_cos = std::cos(inner_angle_radians);
    light.spot_outer_cos = std::cos(outer_angle_radians);
    light.enabled = Enabled() && enabled && HasRuntimeDirectLighting(mode);
    light.casts_shadows = casts_shadows;
    return light;
}

/**
 * @brief TODO: Describe LightEntity::Initialize.
 *
 * @param context TODO: Describe this parameter.
 */
void LightEntity::Initialize(Context &context)
{
    if (context.rendering == nullptr)
    {
        return;
    }
    renderer_light_ = context.rendering->RegisterLight(RenderLight());
    if (!renderer_light_)
    {
        throw std::runtime_error("renderer rejected light entity");
    }
}

/**
 * @brief TODO: Describe LightEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 */
void LightEntity::Update(Context &context, float /*unused*/)
{
    if (context.rendering == nullptr || !renderer_light_)
    {
        return;
    }
    context.rendering->UpdateLight(renderer_light_, RenderLight());
}

void LightEntity::OnEnabledChanged(Context &context, bool /*enabled*/)
{
    Update(context, 0.0f);
}

/**
 * @brief TODO: Describe LightEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void LightEntity::Shutdown(Context &context)
{
    if (context.rendering != nullptr && renderer_light_)
    {
        context.rendering->RemoveLight(renderer_light_);
    }
    renderer_light_ = {};
}
} // namespace CEngine::Entities
