#ifndef CENGINE_ENTITY_FOG_ENTITY_H
#define CENGINE_ENTITY_FOG_ENTITY_H

#include "entity/entity.h"
#include "engine/engine_entities.generated.h"

#include <string_view>

namespace CEngine::Entities {

class FogEntity final : public Scene::Entity,
    public Generated::EngineEntities::ExponentialHeightFogProperties {
public:
    std::string_view Classname() const override;
    void Update(EngineContext& context, float delta_seconds) override;
    void Shutdown(EngineContext& context) override;
};

} // namespace CEngine::Entities

#endif
