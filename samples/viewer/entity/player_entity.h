//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/entity/player_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H
#define CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_ENTITY_H

#include "entity/entity.h"
#include "input/actions.h"
#include "physics/physics_types.h"
#include "viewer/viewer_entities.generated.h"

#include <string_view>

namespace Viewer
{

/**
 * @brief TODO: Describe PlayerEntity.
 */
class PlayerEntity final : public CEngine::Scene::Entity, public Generated::PlayerProperties
{
  public:
    /**
     * @brief TODO: Describe PlayerEntity.
     *
     * @param actions TODO: Describe this parameter.
     */
    explicit PlayerEntity(Actions actions) : actions_(actions)
    {
    }
    float move_speed = 5.0f;
    /**
     * @brief TODO: Describe Classname.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view Classname() const override;
    /**
     * @brief TODO: Describe Initialize.
     *
     * @param context TODO: Describe this parameter.
     */
    void Initialize(CEngine::Context &context) override;
    /**
     * @brief TODO: Describe Update.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(CEngine::Context &context, float delta_seconds) override;
    /**
     * @brief TODO: Describe Shutdown.
     *
     * @param context TODO: Describe this parameter.
     */
    void Shutdown(CEngine::Context &context) override;

  private:
    Actions actions_;
    glm::vec2 look_angles_ = glm::vec2(0.0f);
    PhysicsCharacterHandle character_;
};

} // namespace Viewer

#endif
