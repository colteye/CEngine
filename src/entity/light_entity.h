#ifndef CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H
#define CENGINE_SCENE_ENTITIES_LIGHT_ENTITY_H

#include "entity/entity.h"
#include "engine/engine_entities.generated.h"
#include "renderer/light.h"

#include <string_view>

#include <glm/vec2.hpp>

namespace CEngine::Entities {

using LightType = Generated::EngineEntities::LightType;
using LightMode = Generated::EngineEntities::LightMode;

// Realtime and Mixed lights both provide direct light and shadows at runtime.
// Baked lights are represented entirely by the static lightmap.
constexpr bool HasRuntimeDirectLighting(LightMode mode)
{
    return mode == LightMode::Realtime || mode == LightMode::Mixed;
}

class LightEntity final : public Scene::Entity,
    public Generated::EngineEntities::LightProperties {
public:
    std::string_view Classname() const override;
    void Initialize(EngineContext& context) override;
    void Update(EngineContext& context, float delta_seconds) override;
    void Shutdown(EngineContext& context) override;

private:
    Renderer::Light RenderLight() const;
    Renderer::LightHandle renderer_light_;
};

} // namespace CEngine::Entities

#endif
