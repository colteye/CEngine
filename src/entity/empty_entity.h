#ifndef CENGINE_SCENE_ENTITIES_EMPTY_ENTITY_H
#define CENGINE_SCENE_ENTITIES_EMPTY_ENTITY_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class EmptyEntity final : public Scene::Entity {
public:
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
