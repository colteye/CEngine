#ifndef CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H
#define CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"
#include "renderer/light.h"

#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities
{

// Realtime and Mixed lights both provide direct light and shadows at runtime.
// Baked lights are represented entirely by the static lightmap.
constexpr bool HasRuntimeDirectLighting(Generated::EngineEntities::LightMode mode)
{
    return mode == Generated::EngineEntities::LightMode::Realtime ||
           mode == Generated::EngineEntities::LightMode::Mixed;
}

class LightEntity final : public Scene::Entity, public Generated::EngineEntities::LightProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Initialize(Context &context) override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;

  private:
    [[nodiscard]] Renderer::Light RenderLight() const;
    Renderer::LightHandle renderer_light_;
};

} // namespace CEngine::Entities

#endif
