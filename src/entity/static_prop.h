#ifndef CENGINE_SCENE_ENTITIES_STATIC_PROP_H
#define CENGINE_SCENE_ENTITIES_STATIC_PROP_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities {

class StaticProp final : public Scene::Entity {
public:
    StaticProp();
    Assets::AssetHandle mesh;
    std::uint32_t first_material = 0;
    std::uint32_t material_count = 0;
    Assets::AssetHandle lightmap;
    glm::vec2 lightmap_scale = glm::vec2(1.0f);
    glm::vec2 lightmap_offset = glm::vec2(0.0f);
    float lightmap_rgbm_range = 8.0f;
    bool visible = true;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
