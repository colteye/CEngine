// Copyright (c) CEngine contributors.

#ifndef CENGINE_ENTITY_LOGIC_ENTITIES_H
#define CENGINE_ENTITY_LOGIC_ENTITIES_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

namespace CEngine::Entities
{

class LogicRelayEntity final : public Scene::Entity, public Generated::EngineEntities::LogicRelayProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Initialize(Context &context) override;

  private:
    bool fired_ = false;
};

class LogicTimerEntity final : public Scene::Entity, public Generated::EngineEntities::LogicTimerProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;

  private:
    void Fire(Context &context, Scene::EntityHandle activator);

    float remaining_seconds_ = 0.0f;
    bool initialized_ = false;
};

class LogicAutoEntity final : public Scene::Entity, public Generated::EngineEntities::LogicAutoProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override;
    [[nodiscard]] bool HasOutput(std::string_view output) const override;
    bool HandleInput(Context &context, const Scene::EntityInput &input) override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;

  private:
    bool armed_ = false;
};

} // namespace CEngine::Entities

#endif
