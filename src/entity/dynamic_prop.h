#ifndef CENGINE_SCENE_ENTITIES_DYNAMIC_PROP_H
#define CENGINE_SCENE_ENTITIES_DYNAMIC_PROP_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class DynamicProp final : public Scene::Entity {
public:
    Assets::AssetHandle mesh;
    std::uint32_t first_material = 0;
    std::uint32_t material_count = 0;
    bool visible = true;
    bool physics_enabled = false;
    glm::vec3 collision_half_extents = glm::vec3(0.5f);
    float mass = 1.0f;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
