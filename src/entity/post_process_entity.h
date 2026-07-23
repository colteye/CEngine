#ifndef CENGINE_ENTITY_POST_PROCESS_ENTITY_H
#define CENGINE_ENTITY_POST_PROCESS_ENTITY_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

#include <string_view>

namespace CEngine::Entities
{

class PostProcessEntity final : public Scene::Entity, public Generated::EngineEntities::PostProcessProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override;
    void Update(Context &context, float delta_seconds) override;
    void Shutdown(Context &context) override;
};

} // namespace CEngine::Entities

#endif
