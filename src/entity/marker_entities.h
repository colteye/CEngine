// Copyright (c) CEngine contributors.

#ifndef CENGINE_ENTITY_MARKER_ENTITIES_H
#define CENGINE_ENTITY_MARKER_ENTITIES_H

#include "engine/engine_entities.generated.h"
#include "entity/entity.h"

namespace CEngine::Entities
{

class MarkerEntity final : public Scene::Entity, public Generated::EngineEntities::MarkerProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override
    {
        return "marker";
    }
};

} // namespace CEngine::Entities

#endif
