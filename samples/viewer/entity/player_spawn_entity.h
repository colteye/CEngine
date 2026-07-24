//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/entity/player_spawn_entity.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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
