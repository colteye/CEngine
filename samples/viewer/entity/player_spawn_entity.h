// Copyright (c) CEngine contributors.

#ifndef CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_SPAWN_ENTITY_H
#define CENGINE_SAMPLES_VIEWER_ENTITY_PLAYER_SPAWN_ENTITY_H

#include "entity/entity.h"
#include "viewer/viewer_entities.generated.h"

namespace Viewer
{

class PlayerSpawnEntity final : public CEngine::Scene::Entity,
                                public Generated::PlayerSpawnProperties
{
  public:
    [[nodiscard]] std::string_view Classname() const override
    {
        return "player_spawn";
    }
};

} // namespace Viewer

#endif
