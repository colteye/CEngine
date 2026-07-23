#include "entity/fog_entity.h"

#include "context.h"
#include "renderer/render_system.h"

namespace CEngine::Entities
{
std::string_view FogEntity::Classname() const
{
    return "exponential_height_fog";
}

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

void FogEntity::Shutdown(Context &context)
{
    if (context.rendering != nullptr)
    {
        context.rendering->SetExponentialHeightFog(Renderer::ExponentialHeightFog{});
    }
}
} // namespace CEngine::Entities
