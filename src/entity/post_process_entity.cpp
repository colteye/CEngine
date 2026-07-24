//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/post_process_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/post_process_entity.h"

#include "context.h"
#include "renderer/render_system.h"

namespace CEngine::Entities
{

/**
 * @brief TODO: Describe PostProcessEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view PostProcessEntity::Classname() const
{
    return "post_process";
}

/**
 * @brief TODO: Describe PostProcessEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 */
void PostProcessEntity::Update(Context &context, float /*unused*/)
{
    if (context.rendering == nullptr || !Enabled())
    {
        return;
    }

    Renderer::PostProcessSettings post_process;
    post_process.bloom_enabled = bloom_enabled;
    post_process.bloom_threshold = bloom_threshold;
    post_process.bloom_intensity = bloom_intensity;
    post_process.tone_mapping_enabled = tone_mapping_enabled;
    post_process.exposure = exposure;
    post_process.contrast = contrast;
    post_process.saturation = saturation;
    post_process.depth_of_field_enabled = depth_of_field_enabled;
    post_process.focus_distance = focus_distance;
    post_process.focus_range = focus_range;
    post_process.depth_of_field_strength = depth_of_field_strength;
    post_process.sun_lens_flare_enabled = sun_lens_flare_enabled;
    post_process.sun_lens_flare_intensity = sun_lens_flare_intensity;
    post_process.sun_disc_size = sun_disc_size;
    post_process.sun_disc_softness = sun_disc_softness;
    context.rendering->SetPostProcessSettings(post_process);

    Renderer::SSAOSettings ssao;
    ssao.enabled = ssao_enabled;
    ssao.radius = ssao_radius;
    ssao.bias = ssao_bias;
    ssao.intensity = ssao_intensity;
    ssao.contrast = ssao_contrast;
    context.rendering->SetSSAOSettings(ssao);
}

/**
 * @brief TODO: Describe PostProcessEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void PostProcessEntity::Shutdown(Context &context)
{
    if (context.rendering == nullptr)
    {
        return;
    }
    context.rendering->SetPostProcessSettings(Renderer::PostProcessSettings{});
    context.rendering->SetSSAOSettings(Renderer::SSAOSettings{});
}

void PostProcessEntity::OnEnabledChanged(Context &context, bool enabled)
{
    if (enabled)
    {
        Update(context, 0.0f);
    }
    else
    {
        Shutdown(context);
    }
}

} // namespace CEngine::Entities
