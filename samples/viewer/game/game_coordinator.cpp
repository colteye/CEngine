// Copyright (c) CEngine contributors.

#include "game/game_coordinator.h"

#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "entity/player_spawn_entity.h"
#include "scene/scene.h"

#include <algorithm>
#include <string>

#include <glm/geometric.hpp>

namespace Viewer
{
namespace
{

std::string PlayerName(const PlayerRecord &player)
{
    return player.name.empty()
               ? "Player_" + std::to_string(player.id)
               : player.name;
}

bool TeamMatches(
    Generated::PlayerTeam spawn, Generated::PlayerTeam player)
{
    return spawn == Generated::PlayerTeam::Any ||
           player == Generated::PlayerTeam::Any || spawn == player;
}

bool GroupMatches(std::uint32_t spawn, std::uint32_t requested)
{
    return spawn == 0 || requested == 0 || spawn == requested;
}

bool HasActions(const Actions &actions)
{
    return static_cast<bool>(actions.move_forward);
}

} // namespace

GameCoordinator::GameCoordinator(Actions default_actions)
    : default_actions_(default_actions)
{
}

bool GameCoordinator::RegisterEntityTypes(
    CEngine::Entities::EntityFactory &factory) const
{
    const bool spawn_registered =
        factory.Register<PlayerSpawnEntity, Generated::PlayerSpawn>(
            "player_spawn");
    const bool player_registered =
        factory.Register<PlayerEntity, Generated::Player>(
            "player", default_actions_);
    return spawn_registered && player_registered;
}

const PlayerRecord *GameCoordinator::FindPlayer(PlayerId player) const
{
    const auto found = std::find_if(
        players_.begin(), players_.end(),
        [player](const PlayerRecord &candidate) {
            return candidate.id == player;
        });
    return found == players_.end() ? nullptr : &*found;
}

bool GameCoordinator::AddPlayer(
    CEngine::Scene::Scene &scene, PlayerRequest request)
{
    if (request.id == 0 || FindPlayer(request.id) != nullptr)
    {
        return false;
    }
    PlayerRecord record;
    record.id = request.id;
    record.name = std::move(request.name);
    record.team = request.team;
    record.spawn_group = request.spawn_group;
    record.actions = request.locally_controlled
                         ? (HasActions(request.actions)
                                ? request.actions
                                : default_actions_)
                         : Actions{};
    record.locally_controlled = request.locally_controlled;
    record.primary_view = request.primary_view;
    if (!SpawnPlayer(scene, record))
    {
        return false;
    }
    if (record.primary_view)
    {
        for (PlayerRecord &existing : players_)
        {
            existing.primary_view = false;
        }
    }
    players_.push_back(std::move(record));
    return true;
}

bool GameCoordinator::AdoptOrAddPlayer(
    CEngine::Scene::Scene &scene, PlayerRequest request)
{
    if (request.id == 0 || FindPlayer(request.id) != nullptr)
    {
        return false;
    }
    for (const auto &candidate : scene.Entities())
    {
        if (candidate == nullptr || candidate->Classname() != "player")
        {
            continue;
        }
        const bool already_owned = std::any_of(
            players_.begin(), players_.end(),
            [&candidate](const PlayerRecord &player) {
                return player.entity == candidate->GetHandle();
            });
        if (already_owned)
        {
            continue;
        }
        auto *entity = dynamic_cast<PlayerEntity *>(candidate.get());
        if (entity == nullptr)
        {
            continue;
        }
        const Actions runtime_actions =
            request.locally_controlled
                ? (HasActions(request.actions)
                       ? request.actions
                       : default_actions_)
                : Actions{};
        entity->ConfigureRuntime(
            request.id, request.team, request.locally_controlled,
            runtime_actions);
        PlayerRecord record;
        record.id = request.id;
        record.name =
            request.name.empty() ? entity->Name() : std::move(request.name);
        record.team = request.team;
        record.spawn_group = request.spawn_group;
        record.actions = runtime_actions;
        record.locally_controlled = request.locally_controlled;
        record.primary_view = request.primary_view;
        record.entity = entity->GetHandle();
        if (record.primary_view && !scene.SetActiveEntity(record.entity))
        {
            return false;
        }
        if (record.primary_view)
        {
            for (PlayerRecord &existing : players_)
            {
                existing.primary_view = false;
            }
        }
        players_.push_back(std::move(record));
        return true;
    }
    return AddPlayer(scene, std::move(request));
}

bool GameCoordinator::RespawnPlayer(
    CEngine::Scene::Scene &scene, PlayerId player)
{
    auto found = std::find_if(
        players_.begin(), players_.end(),
        [player](const PlayerRecord &candidate) {
            return candidate.id == player;
        });
    if (found == players_.end())
    {
        return false;
    }
    if (found->entity)
    {
        scene.DestroyEntity(found->entity);
        found->entity = {};
    }
    return SpawnPlayer(scene, *found);
}

bool GameCoordinator::RemovePlayer(
    CEngine::Scene::Scene &scene, PlayerId player)
{
    const auto found = std::find_if(
        players_.begin(), players_.end(),
        [player](const PlayerRecord &candidate) {
            return candidate.id == player;
        });
    if (found == players_.end())
    {
        return false;
    }
    if (found->entity)
    {
        scene.DestroyEntity(found->entity);
    }
    const bool removed_primary = found->primary_view;
    players_.erase(found);
    if (removed_primary)
    {
        const auto replacement = std::find_if(
            players_.begin(), players_.end(),
            [&scene](const PlayerRecord &candidate) {
                return candidate.locally_controlled &&
                       scene.IsAlive(candidate.entity);
            });
        if (replacement != players_.end())
        {
            replacement->primary_view = true;
            scene.SetActiveEntity(replacement->entity);
        }
    }
    return true;
}

PlayerEntity *GameCoordinator::PrimaryViewPlayer(
    CEngine::Scene::Scene &scene) const
{
    CEngine::Scene::Entity *entity =
        scene.GetEntity(scene.Settings().active_entity);
    return entity != nullptr && entity->Classname() == "player"
               ? dynamic_cast<PlayerEntity *>(entity)
               : nullptr;
}

const PlayerSpawnEntity *GameCoordinator::ChooseSpawn(
    const CEngine::Scene::Scene &scene, const PlayerRecord &player) const
{
    const PlayerSpawnEntity *best = nullptr;
    for (const auto &candidate : scene.Entities())
    {
        if (candidate == nullptr || !candidate->Enabled() ||
            candidate->Classname() != "player_spawn")
        {
            continue;
        }
        const auto *spawn =
            dynamic_cast<const PlayerSpawnEntity *>(candidate.get());
        if (spawn == nullptr || !TeamMatches(spawn->team, player.team) ||
            !GroupMatches(spawn->spawn_group, player.spawn_group) ||
            SpawnOccupied(scene, *spawn))
        {
            continue;
        }
        if (best == nullptr)
        {
            best = spawn;
            continue;
        }
        const bool exact_group =
            player.spawn_group != 0 &&
            spawn->spawn_group == player.spawn_group;
        const bool best_exact_group =
            player.spawn_group != 0 &&
            best->spawn_group == player.spawn_group;
        const bool exact_team =
            player.team != Generated::PlayerTeam::Any &&
            spawn->team == player.team;
        const bool best_exact_team =
            player.team != Generated::PlayerTeam::Any &&
            best->team == player.team;
        if (exact_group != best_exact_group)
        {
            if (exact_group)
            {
                best = spawn;
            }
            continue;
        }
        if (exact_team != best_exact_team)
        {
            if (exact_team)
            {
                best = spawn;
            }
            continue;
        }
        if (spawn->priority > best->priority ||
            (spawn->priority == best->priority &&
             spawn->GetHandle().Value() < best->GetHandle().Value()))
        {
            best = spawn;
        }
    }
    return best;
}

bool GameCoordinator::SpawnOccupied(
    const CEngine::Scene::Scene &scene,
    const PlayerSpawnEntity &spawn) const
{
    if (spawn.clearance_radius <= 0.0f)
    {
        return false;
    }
    const float radius_squared =
        spawn.clearance_radius * spawn.clearance_radius;
    for (const auto &candidate : scene.Entities())
    {
        if (candidate == nullptr || candidate->Classname() != "player")
        {
            continue;
        }
        const glm::vec3 offset =
            candidate->GetTransform().position -
            spawn.GetTransform().position;
        if (glm::dot(offset, offset) < radius_squared)
        {
            return true;
        }
    }
    return false;
}

bool GameCoordinator::SpawnPlayer(
    CEngine::Scene::Scene &scene, PlayerRecord &player)
{
    const PlayerSpawnEntity *spawn = ChooseSpawn(scene, player);
    if (spawn == nullptr)
    {
        return false;
    }
    PlayerRuntimeConfig config;
    config.id = player.id;
    config.team = player.team;
    config.locally_controlled = player.locally_controlled;
    config.transform = spawn->GetTransform();
    PlayerEntity &entity = scene.CreateEntity<PlayerEntity>(
        PlayerName(player), player.actions, config);
    player.entity = entity.GetHandle();
    if (player.primary_view && !scene.SetActiveEntity(player.entity))
    {
        scene.DestroyEntity(player.entity);
        player.entity = {};
        return false;
    }
    return true;
}

} // namespace Viewer
