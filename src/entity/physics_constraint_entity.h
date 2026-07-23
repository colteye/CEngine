#ifndef CENGINE_ENTITY_PHYSICS_CONSTRAINT_ENTITY_H
#define CENGINE_ENTITY_PHYSICS_CONSTRAINT_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "physics/physics_types.h"

#include <string_view>

namespace CEngine::Entities
{

class PhysicsConstraintEntity final : public Scene::Entity,
                                      public Generated::EngineEntities::PhysicsConstraintProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Shutdown(Context &context) override;

  private:
    PhysicsConstraintHandle constraint_;
};

} // namespace CEngine::Entities

#endif
