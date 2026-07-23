#ifndef CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H
#define CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H

#include "entity/entity.h"
#include "input/actions.h"
#include "viewer/viewer_entities.generated.h"

#include <string_view>

namespace Viewer {

using PlayerViewMode = Generated::PlayerViewMode;

class PlayerEntity final : public CEngine::Scene::Entity,
    public Generated::PlayerProperties {
public:
    explicit PlayerEntity(Actions actions) : actions_(actions) {}
    float move_speed = 5.0f;
    std::string_view Classname() const override;
    void Initialize(CEngine::EngineContext& context) override;
    void Update(CEngine::EngineContext& context, float delta_seconds) override;

private:
    Actions actions_;
    glm::vec2 look_angles_ = glm::vec2(0.0f);
};

} // namespace Viewer

#endif
