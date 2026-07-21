#ifndef CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H
#define CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H

#include "entity/entity.h"

#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities {

enum class LightType : std::uint32_t { Point, Sun, Spot, Area };
enum class LightMode : std::uint32_t { Realtime, Baked, Mixed };

class LightEntity final : public Scene::Entity {
public:
    LightType type = LightType::Point;
    LightMode mode = LightMode::Realtime;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
    float inner_angle_radians = 0.0f;
    float outer_angle_radians = 0.7853982f;
    glm::vec2 area_size = glm::vec2(1.0f);
    bool enabled = true;
    bool casts_shadows = true;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
