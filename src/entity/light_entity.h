//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/light_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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
/**
 * @brief TODO: Describe HasRuntimeDirectLighting.
 *
 * @param mode TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
constexpr bool HasRuntimeDirectLighting(Generated::EngineEntities::LightMode mode)
{
    return mode == Generated::EngineEntities::LightMode::Realtime ||
           mode == Generated::EngineEntities::LightMode::Mixed;
}

/**
 * @brief TODO: Describe LightEntity.
 */
class LightEntity final : public Scene::Entity, public Generated::EngineEntities::LightProperties
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
     * @brief TODO: Describe Update.
     *
     * @param context TODO: Describe this parameter.
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(Context &context, float delta_seconds) override;
    /**
     * @brief TODO: Describe Shutdown.
     *
     * @param context TODO: Describe this parameter.
     */
    void Shutdown(Context &context) override;

  private:
    /**
     * @brief TODO: Describe RenderLight.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] Renderer::Light RenderLight() const;
    void OnEnabledChanged(Context &context, bool enabled) override;
    Renderer::LightHandle renderer_light_;
};

} // namespace CEngine::Entities

#endif
