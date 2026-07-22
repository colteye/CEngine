#ifndef CENGINE_ENTITY_EXPONENTIAL_HEIGHT_FOG_ENTITY_H
#define CENGINE_ENTITY_EXPONENTIAL_HEIGHT_FOG_ENTITY_H

#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities {

class ExponentialHeightFogEntity final : public Scene::Entity {
public:
    glm::vec3 inscattering_color = glm::vec3(0.5f, 0.6f, 0.7f);
    float density = 0.02f;
    float height_falloff = 0.2f;
    float start_distance = 0.0f;
    float max_opacity = 1.0f;
    float cutoff_distance = 0.0f;
    bool enabled = true;
    std::string_view Classname() const override;
};

} // namespace CEngine::Entities

#endif
