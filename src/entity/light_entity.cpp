#include "entity/light_entity.h"

#include "engine_context.h"
#include "renderer/render_system.h"

#include <cmath>
#include <stdexcept>

namespace CEngine::Entities
{
std::string_view LightEntity::Classname() const
{
    return "light";
}

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

void LightEntity::Initialize(EngineContext &context)
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

void LightEntity::Update(EngineContext &context, float /*unused*/)
{
    if (context.rendering == nullptr || !renderer_light_)
    {
        return;
    }
    context.rendering->UpdateLight(renderer_light_, RenderLight());
}

void LightEntity::Shutdown(EngineContext &context)
{
    if (context.rendering != nullptr && renderer_light_)
    {
        context.rendering->RemoveLight(renderer_light_);
    }
    renderer_light_ = {};
}
} // namespace CEngine::Entities
