#ifndef CENGINE_ENTITY_PROP_ENTITY_H
#define CENGINE_ENTITY_PROP_ENTITY_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <cstdint>
#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities {

class PropEntity final : public Scene::Entity {
public:
    Assets::AssetHandle mesh;
    std::uint32_t first_material = 0;
    std::uint32_t material_count = 0;
    Assets::AssetHandle lightmap;
    glm::vec2 lightmap_scale = glm::vec2(1.0f);
    glm::vec2 lightmap_offset = glm::vec2(0.0f);
    float lightmap_rgbm_range = 8.0f;
    bool dynamic = false;
    bool visible = true;
    bool collision_enabled = false;
    glm::vec3 collision_half_extents = glm::vec3(0.5f);
    float mass = 1.0f;

    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
