#ifndef CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H
#define CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H

#include "entity/entity.h"
#include "input/actions.h"
#include "physics/physics_types.h"
#include "viewer/viewer_entities.generated.h"

#include <string_view>

namespace Viewer
{

class PlayerEntity final : public CEngine::Scene::Entity, public Generated::PlayerProperties
{
  public:
    explicit PlayerEntity(Actions actions) : actions_(actions)
    {
    }
    float move_speed = 5.0f;
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(CEngine::Context &context) override;
    void Update(CEngine::Context &context, float delta_seconds) override;
    void Shutdown(CEngine::Context &context) override;

  private:
    Actions actions_;
    glm::vec2 look_angles_ = glm::vec2(0.0f);
    PhysicsCharacterHandle character_;
};

} // namespace Viewer

#endif
