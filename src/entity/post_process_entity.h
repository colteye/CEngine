#ifndef CENGINE_ENTITY_POST_PROCESS_ENTITY_H
#define CENGINE_ENTITY_POST_PROCESS_ENTITY_H

#include "entity/entity.h"
#include "engine/engine_entities.generated.h"

#include <string_view>

namespace CEngine::Entities {

class PostProcessEntity final : public Scene::Entity,
    public Generated::EngineEntities::PostProcessProperties {
public:
    std::string_view Classname() const override;
    void Update(EngineContext& context, float delta_seconds) override;
    void Shutdown(EngineContext& context) override;
};

} // namespace CEngine::Entities

#endif
