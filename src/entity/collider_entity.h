//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/collider_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_COLLIDER_ENTITY_H
#define CENGINE_ENTITY_COLLIDER_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "physics/physics_types.h"

#include <memory>

namespace CEngine::Entities
{

class ColliderEntity final : public Scene::Entity, public Generated::EngineEntities::ColliderProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void LateUpdate(Context &context, float delta_seconds) override;
    void OnPhysicsContact(Context &context, const Scene::EntityContact &contact) override;
    void Shutdown(Context &context) override;
    [[nodiscard]] PhysicsBodyHandle PhysicsBody() const
    {
        return body_;
    }

  private:
    bool CreateBody(Context &context);
    void DestroyBody(Context &context);
    void OnEnabledChanged(Context &context, bool enabled) override;

    std::shared_ptr<const PhysicsShape> shape_;
    PhysicsBodyHandle body_;
};

} // namespace CEngine::Entities

#endif
