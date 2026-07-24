// Copyright (c) CEngine contributors.

#ifndef CENGINE_ENTITY_TRIGGER_ENTITY_H
#define CENGINE_ENTITY_TRIGGER_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "physics/physics_types.h"

#include <memory>
#include <vector>

namespace CEngine::Entities
{

class TriggerEntity final : public Scene::Entity, public Generated::EngineEntities::TriggerVolumeProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void OnPhysicsContact(Context &context, const Scene::EntityContact &contact) override;
    void Shutdown(Context &context) override;

  protected:
    void OnEnabledChanged(Context &context, bool enabled) override;

  private:
    struct Overlap
    {
        Scene::EntityHandle entity;
        std::uint32_t contacts = 0;
    };

    bool CreateBody(Context &context);
    void DestroyBody(Context &context);
    Overlap *FindOverlap(Scene::EntityHandle entity);

    std::shared_ptr<const PhysicsShape> shape_;
    PhysicsBodyHandle body_;
    std::vector<Overlap> overlaps_;
    bool fired_ = false;
};

} // namespace CEngine::Entities

#endif
