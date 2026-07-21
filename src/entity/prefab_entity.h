#ifndef CENGINE_SCENE_ENTITIES_PREFAB_ENTITY_H
#define CENGINE_SCENE_ENTITIES_PREFAB_ENTITY_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <glm/vec2.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace CEngine::Entities {

struct PrefabLightmapBinding {
    std::uint32_t object_index = 0;
    Assets::AssetHandle lightmap;
    glm::vec2 scale = glm::vec2(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float rgbm_range = 8.0f;
};

class PrefabEntity final : public Scene::Entity {
public:
    Assets::AssetHandle prefab;
    std::vector<PrefabLightmapBinding> lightmaps;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
