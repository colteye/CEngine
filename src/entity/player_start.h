#ifndef CENGINE_SCENE_ENTITIES_PLAYER_START_H
#define CENGINE_SCENE_ENTITIES_PLAYER_START_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class PlayerStart final : public Scene::Entity {
public:
    std::uint32_t team = 0;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
