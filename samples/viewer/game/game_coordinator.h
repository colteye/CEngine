// Copyright (c) CEngine contributors.

#ifndef CENGINE_SAMPLES_VIEWER_GAME_GAME_COORDINATOR_H
#define CENGINE_SAMPLES_VIEWER_GAME_GAME_COORDINATOR_H

#include "entity/entity.h"
#include "input/actions.h"
#include "viewer/viewer_entities.generated.h"

#include <cstdint>
#include <string>
#include <vector>

namespace CEngine::Entities
{
class EntityFactory;
}

namespace CEngine::Scene
{
class Scene;
}

namespace Viewer
{

class PlayerEntity;
class PlayerSpawnEntity;

using PlayerId = std::uint64_t;

struct PlayerRequest
{
    PlayerId id = 0;
    std::string name;
    Generated::PlayerTeam team = Generated::PlayerTeam::Any;
    std::uint32_t spawn_group = 0;
    Actions actions;
    bool locally_controlled = false;
    bool primary_view = false;
};

struct PlayerRecord
{
    PlayerId id = 0;
    std::string name;
    Generated::PlayerTeam team = Generated::PlayerTeam::Any;
    std::uint32_t spawn_group = 0;
    Actions actions;
    bool locally_controlled = false;
    bool primary_view = false;
    CEngine::Scene::EntityHandle entity;
};

class GameCoordinator
{
  public:
    explicit GameCoordinator(Actions default_actions);

    bool RegisterEntityTypes(CEngine::Entities::EntityFactory &factory) const;
    bool AddPlayer(CEngine::Scene::Scene &scene, PlayerRequest request);
    bool AdoptOrAddPlayer(CEngine::Scene::Scene &scene, PlayerRequest request);
    bool RespawnPlayer(CEngine::Scene::Scene &scene, PlayerId player);
    bool RemovePlayer(CEngine::Scene::Scene &scene, PlayerId player);

    [[nodiscard]] const PlayerRecord *FindPlayer(PlayerId player) const;
    [[nodiscard]] PlayerEntity *PrimaryViewPlayer(CEngine::Scene::Scene &scene) const;
    [[nodiscard]] const std::vector<PlayerRecord> &Players() const
    {
        return players_;
    }

  private:
    [[nodiscard]] const PlayerSpawnEntity *ChooseSpawn(
        const CEngine::Scene::Scene &scene, const PlayerRecord &player) const;
    [[nodiscard]] bool SpawnPlayer(
        CEngine::Scene::Scene &scene, PlayerRecord &player);
    [[nodiscard]] bool SpawnOccupied(
        const CEngine::Scene::Scene &scene,
        const PlayerSpawnEntity &spawn) const;

    Actions default_actions_;
    std::vector<PlayerRecord> players_;
};

} // namespace Viewer

#endif
