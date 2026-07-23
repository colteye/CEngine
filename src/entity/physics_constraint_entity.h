//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/physics_constraint_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_PHYSICS_CONSTRAINT_ENTITY_H
#define CENGINE_ENTITY_PHYSICS_CONSTRAINT_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "physics/physics_types.h"

#include <string_view>

namespace CEngine::Entities
{

/**
 * @brief TODO: Describe PhysicsConstraintEntity.
 */
class PhysicsConstraintEntity final : public Scene::Entity,
                                      public Generated::EngineEntities::PhysicsConstraintProperties
{
  public:
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
    void Initialize(Context &context) override;
    /**
     * @brief TODO: Describe Shutdown.
     *
     * @param context TODO: Describe this parameter.
     */
    void Shutdown(Context &context) override;

  private:
    PhysicsConstraintHandle constraint_;
};

} // namespace CEngine::Entities

#endif
