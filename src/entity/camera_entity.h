#ifndef CENGINE_SCENE_ENTITIES_CAMERA_ENTITY_H
#define CENGINE_SCENE_ENTITIES_CAMERA_ENTITY_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

enum class CameraProjection : std::uint32_t { Perspective, Orthographic };

class CameraEntity final : public Scene::Entity {
public:
    CameraProjection projection = CameraProjection::Perspective;
    float vertical_fov_radians = 1.0471976f;
    float orthographic_size = 10.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    bool enabled = true;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
