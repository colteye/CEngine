#ifndef CENGINE_SCENE_ENTITIES_PREFAB_ENTITY_H
#define CENGINE_SCENE_ENTITIES_PREFAB_ENTITY_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class PrefabEntity final : public Scene::Entity {
public:
    Assets::AssetHandle prefab;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
