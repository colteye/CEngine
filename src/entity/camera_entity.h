// Copyright (c) CEngine contributors.

#ifndef CENGINE_ENTITY_CAMERA_ENTITY_H
#define CENGINE_ENTITY_CAMERA_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

namespace CEngine::Entities
{

class CameraEntity final : public Scene::Entity, public Generated::EngineEntities::CameraProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Update(Context &context, float delta_seconds) override;

  protected:
    void OnEnabledChanged(Context &context, bool enabled) override;
};

} // namespace CEngine::Entities

#endif
