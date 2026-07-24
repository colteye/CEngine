//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#ifndef CENGINE_RENDERER_ENVIRONMENT_PROBE_H
#define CENGINE_RENDERER_ENVIRONMENT_PROBE_H

#include "handle.h"
#include "renderer/texture.h"

#include <memory>

#include <glm/mat4x4.hpp>

namespace CEngine::Renderer
{

struct EnvironmentProbeSlotTag;
using EnvironmentProbeHandle = Handle<EnvironmentProbeSlotTag>;

// A baked local environment used only by non-lightmapped meshes. The transform
// maps a unit box to the probe's oriented influence volume.
struct EnvironmentProbe
{
    std::shared_ptr<const Texture> panorama;
    glm::mat4 transform = glm::mat4(1.0f);
    float blend_distance = 1.0f;
    float intensity = 1.0f;
    bool enabled = true;
};

} // namespace CEngine::Renderer

#endif
