#ifndef CENGINE_SCENE_ENTITIES_TRIGGER_ENTITY_H
#define CENGINE_SCENE_ENTITIES_TRIGGER_ENTITY_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class TriggerEntity final : public Scene::Entity {
public:
    glm::vec3 half_extents = glm::vec3(0.5f);
    bool enabled = true;
    bool trigger_once = false;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
