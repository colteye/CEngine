#ifndef CENGINE_ENTITY_SKYBOX_ENTITY_H
#define CENGINE_ENTITY_SKYBOX_ENTITY_H

#include "assets/asset_handle.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class SkyboxEntity final : public Scene::Entity {
public:
    Assets::AssetHandle panorama;
    float intensity = 1.0f;
    float rotation_radians = 0.0f;
    bool enabled = true;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
