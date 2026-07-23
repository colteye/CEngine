#ifndef CENGINE_SCENE_ENTITIES_PLAYER_ENTITY_H
#define CENGINE_SCENE_ENTITIES_PLAYER_ENTITY_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

enum class PlayerViewMode : std::uint32_t { FirstPerson, ThirdPerson };

class PlayerEntity final : public Scene::Entity {
public:
    PlayerViewMode view_mode = PlayerViewMode::FirstPerson;
    float vertical_fov_radians = 1.0471976f;
    float third_person_distance = 4.0f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    bool enabled = true;
    float move_speed = 5.0f;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
