//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/fog_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ENTITY_FOG_ENTITY_H
#define CENGINE_ENTITY_FOG_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities
{

/**
 * @brief TODO: Describe FogEntity.
 */
class FogEntity final : public Scene::Entity, public Generated::EngineEntities::FogProperties
{
  public:
    /**
     * @brief TODO: Describe Classname.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view Classname() const override;
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
    void OnEnabledChanged(Context &context, bool enabled) override;
};

} // namespace CEngine::Entities

#endif
