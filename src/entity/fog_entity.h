#ifndef CENGINE_ENTITY_FOG_ENTITY_H
#define CENGINE_ENTITY_FOG_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities
{

class FogEntity final : public Scene::Entity, public Generated::EngineEntities::ExponentialHeightFogProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;
};

} // namespace CEngine::Entities

#endif
