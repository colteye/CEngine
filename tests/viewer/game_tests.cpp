//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/viewer/game_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "context.h"
#include "entity/entity_factory.h"
#include "entity/player_entity.h"
#include "entity/player_spawn_entity.h"
#include "game/game_coordinator.h"
#include "input/actions.h"
#include "input/input_system.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <cmath>
#include <iostream>

namespace
{

bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

const CEngine::Renderer::MeshInstance *FindMeshInstance(const CEngine::Renderer::RenderSystem &rendering,
                                                        std::string_view mesh_name)
{
    for (const CEngine::Renderer::MeshInstance &instance : rendering.GetMeshInstances())
    {
        if (instance.mesh != nullptr && instance.mesh->name == mesh_name)
        {
            return &instance;
        }
    }
    return nullptr;
}

bool RightHanded(const glm::mat4 &transform)
{
    return glm::dot(glm::cross(glm::vec3(transform[0]), glm::vec3(transform[1])), glm::vec3(transform[2])) > 0.0f;
}

Viewer::PlayerSpawnEntity &AddSpawn(CEngine::Scene::Scene &scene, std::string_view name, glm::vec3 position,
                                    Viewer::Generated::PlayerTeam team, std::uint32_t priority = 0)
{
    auto &spawn = scene.CreateEntity<Viewer::PlayerSpawnEntity>(name);
    spawn.GetTransform().position = position;
    spawn.GetTransform().UpdateWorldMatrix();
    spawn.team = team;
    spawn.priority = priority;
    spawn.clearance_radius = 1.0f;
    return spawn;
}

bool PlayerCrouchesAndFiresPhysicsBalls()
{
    CEngine::Input::InputSystem input;
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    CEngine::Renderer::RenderSystem rendering;
    PhysicsSystem physics;
    PhysicsSystemDesc physics_desc;
    if (!Expect(physics.Initialize(physics_desc), "viewer gameplay test should initialize physics"))
    {
        return false;
    }

    PhysicsShape ground_shape;
    ground_shape.type = PhysicsShapeType::Plane;
    ground_shape.half_extents = {20.0f, 20.0f, 0.0f};
    const PhysicsBodyHandle ground = physics.CreateBody(PhysicsBodyDesc{}, ground_shape);
    if (!Expect(static_cast<bool>(ground), "viewer gameplay test should create a ground plane"))
    {
        return false;
    }

    CEngine::Scene::Scene scene;
    auto &player = scene.CreateEntity<Viewer::PlayerEntity>("PhysicsPlayer", actions);
    CEngine::Scene::SceneSettings settings;
    settings.active_entity = player.GetHandle();
    scene.SetSettings(settings);

    CEngine::Context context;
    context.input = &input;
    context.physics = &physics;
    context.rendering = &rendering;
    scene.Activate(context);

    input.Set(actions.crouch, 1.0f);
    scene.Update(context, 0.2f);
    const float crouched_camera_height = rendering.ActiveCamera().position.z - player.GetTransform().position.z;
    const bool crouched = Expect(player.IsCrouched() && crouched_camera_height < player.eye_height,
                                 "Control should shrink the character and lower its camera");

    input.Set(actions.crouch, 0.0f);
    input.Set(actions.move_right, 1.0f);
    input.Set(actions.fire, 1.0f);
    scene.Update(context, 0.2f);
    const CEngine::Renderer::MeshInstance *launcher = FindMeshInstance(rendering, "PhysicsBallLauncher");
    const CEngine::Renderer::MeshInstance *ball = FindMeshInstance(rendering, "PhysicsBall");
    const bool fired =
        Expect(!player.IsCrouched() && player.ProjectileCount() == 1 && physics.BodyCount() == 2 &&
                   rendering.GetMeshInstances().size() == 4 && rendering.GetParticleEmitterCount() == 1 &&
                   rendering.GetParticleDraws().size() == 12,
               "primary fire should create one visible dynamic physics ball and a muzzle burst") &&
        Expect(launcher != nullptr && RightHanded(launcher->transform),
               "the first-person launcher transform should preserve outward face winding") &&
        Expect(ball != nullptr && Near(ball->transform[3].y - player.GetTransform().position.y, -0.19f),
               "a moving player should still launch the ball strictly along the camera forward axis") &&
        Expect(!rendering.GetParticleDraws().empty() && Near(rendering.GetParticleDraws().front().size, 0.24f),
               "the muzzle burst should use the enlarged evaluation size");

    input.Set(actions.fire, 0.0f);
    input.Set(actions.move_right, 0.0f);
    scene.Update(context, 1.0f / 60.0f);
    const bool retained =
        Expect(player.ProjectileCount() == 1 && physics.IsValid(ground) && !rendering.GetParticleDraws().empty(),
               "released fire should retain the rolling ball, ground, and live muzzle particles");
    scene.Stop();
    const bool cleaned_up = Expect(rendering.GetParticleEmitterCount() == 0 && rendering.GetParticleDraws().empty(),
                                   "player shutdown should remove its muzzle emitter");
    physics.DestroyBody(ground);
    physics.Shutdown();
    return crouched && fired && retained && cleaned_up;
}

} // namespace

int main()
{
    CEngine::Input::InputSystem input;
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    const Viewer::Actions second_actions = Viewer::RegisterActions(input, "second_player");
    CEngine::Entities::EntityFactory factory;
    Viewer::GameCoordinator game(actions);
    CEngine::Renderer::RenderSystem rendering;
    if (!Expect(game.RegisterEntityTypes(factory), "viewer game should register player and player_spawn entities"))
    {
        return 1;
    }

    CEngine::Scene::Scene scene;
    AddSpawn(scene, "RedA", {10.0f, 0.0f, 0.0f}, Viewer::Generated::PlayerTeam::Red, 10);
    AddSpawn(scene, "RedB", {20.0f, 0.0f, 0.0f}, Viewer::Generated::PlayerTeam::Red, 5);
    AddSpawn(scene, "BlueA", {-10.0f, 0.0f, 0.0f}, Viewer::Generated::PlayerTeam::Blue, 10);

    Viewer::PlayerRequest local;
    local.id = 1;
    local.name = "LocalRed";
    local.team = Viewer::Generated::PlayerTeam::Red;
    local.locally_controlled = true;
    local.primary_view = true;
    Viewer::PlayerRequest remote_red;
    remote_red.id = 2;
    remote_red.name = "RemoteRed";
    remote_red.team = Viewer::Generated::PlayerTeam::Red;
    remote_red.actions = second_actions;
    remote_red.locally_controlled = true;
    Viewer::PlayerRequest remote_blue;
    remote_blue.id = 3;
    remote_blue.name = "RemoteBlue";
    remote_blue.team = Viewer::Generated::PlayerTeam::Blue;

    if (!Expect(game.AddPlayer(scene, local), "local player should spawn") ||
        !Expect(game.AddPlayer(scene, remote_red), "second red player should use another unoccupied spawn") ||
        !Expect(game.AddPlayer(scene, remote_blue), "blue player should use the blue spawn") ||
        !Expect(game.Players().size() == 3, "coordinator should retain three persistent player records"))
    {
        return 1;
    }

    const Viewer::PlayerRecord *local_record = game.FindPlayer(1);
    const Viewer::PlayerRecord *remote_red_record = game.FindPlayer(2);
    const Viewer::PlayerRecord *remote_blue_record = game.FindPlayer(3);
    if (!Expect(local_record != nullptr && remote_red_record != nullptr && remote_blue_record != nullptr,
                "coordinator should find every retained player record"))
    {
        return 1;
    }
    auto *local_entity = dynamic_cast<Viewer::PlayerEntity *>(scene.GetEntity(local_record->entity));
    auto *remote_red_entity = dynamic_cast<Viewer::PlayerEntity *>(scene.GetEntity(remote_red_record->entity));
    auto *remote_blue_entity = dynamic_cast<Viewer::PlayerEntity *>(scene.GetEntity(remote_blue_record->entity));
    if (!Expect(local_entity != nullptr && remote_red_entity != nullptr && remote_blue_entity != nullptr,
                "spawned player records should resolve to live player entities") ||
        !Expect(local_entity->GetTransform().position.x == 10.0f &&
                    remote_red_entity->GetTransform().position.x == 20.0f &&
                    remote_blue_entity->GetTransform().position.x == -10.0f,
                "team and occupancy policy should select compatible spawn points") ||
        !Expect(local_entity->PlayerId() == 1 && local_entity->Team() == Viewer::Generated::PlayerTeam::Red &&
                    local_entity->LocallyControlled() && remote_red_entity->LocallyControlled(),
                "live player entities should receive coordinator runtime identity"))
    {
        return 1;
    }

    local_entity->vertical_fov_radians = 0.9f;
    local_entity->near_clip = 0.2f;
    local_entity->far_clip = 750.0f;
    const glm::vec3 remote_position = remote_red_entity->GetTransform().position;

    CEngine::Context context;
    context.rendering = &rendering;
    context.input = &input;
    scene.Activate(context);
    input.Set(actions.move_forward, 1.0f);
    scene.Update(context, 1.0f);

    const CEngine::Renderer::Camera &camera = rendering.ActiveCamera();
    if (!Expect(local_entity->GetTransform().position.x != 10.0f,
                "locally controlled player should consume its input actions") ||
        !Expect(remote_red_entity->GetTransform().position == remote_position,
                "second local player should not consume the first action set") ||
        !Expect(camera.position ==
                    local_entity->GetTransform().position + glm::vec3(0.0f, 0.0f, local_entity->eye_height),
                "the coordinator-selected player should publish an eye-height view") ||
        !Expect(camera.direction == glm::vec3(1.0f, 0.0f, 0.0f),
                "an identity player spawn should look horizontally forward") ||
        !Expect(Near(camera.vertical_fov_radians, local_entity->vertical_fov_radians) &&
                    Near(camera.near_clip, local_entity->near_clip) && Near(camera.far_clip, local_entity->far_clip),
                "active player should publish its camera lens"))
    {
        return 1;
    }
    input.Set(actions.move_forward, 0.0f);
    input.Set(second_actions.move_forward, 1.0f);
    scene.Update(context, 1.0f);
    if (!Expect(remote_red_entity->GetTransform().position != remote_position,
                "another local player should consume its own action set"))
    {
        return 1;
    }

    const CEngine::Scene::EntityHandle stale = game.FindPlayer(1)->entity;
    if (!Expect(game.RespawnPlayer(scene, 1), "coordinator should respawn an existing player") ||
        !Expect(!scene.IsAlive(stale) && scene.IsAlive(game.FindPlayer(1)->entity),
                "respawn should replace the live entity while preserving the record") ||
        !Expect(scene.Settings().active_entity == game.FindPlayer(1)->entity,
                "respawn should restore primary-view ownership") ||
        !Expect(game.RemovePlayer(scene, 2) && game.FindPlayer(2) == nullptr,
                "disconnect should destroy the live entity and remove its record"))
    {
        return 1;
    }
    scene.Stop();
    return PlayerCrouchesAndFiresPhysicsBalls() ? 0 : 1;
}
