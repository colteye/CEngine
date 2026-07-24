//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/scene/entity_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/store.h"
#include "assets/scene_asset.h"
#include "context.h"
#include "entity/entity_factory.h"
#include "entity/audio_entities.h"
#include "entity/camera_entity.h"
#include "entity/collider_entity.h"
#include "entity/fog_entity.h"
#include "entity/light_entity.h"
#include "entity/logic_entities.h"
#include "entity/marker_entities.h"
#include "entity/post_process_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
#include "entity/trigger_entity.h"
#include "input/input_system.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <cmath>
#include <filesystem>
#include <iostream>

namespace
{
using namespace CEngine::Scene;
/**
 * @brief TODO: Describe Expect.
 *
 * @param value TODO: Describe this parameter.
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}
/**
 * @brief TODO: Describe Near.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

/**
 * @brief TODO: Describe CustomEntity.
 */
class CustomEntity final : public Entity
{
  public:
    /**
     * @brief TODO: Describe Classname.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::string_view Classname() const override
    {
        return "game_custom";
    }
    /**
     * @brief TODO: Describe Update.
     *
     * @param delta_seconds TODO: Describe this parameter.
     */
    void Update(CEngine::Context & /*unused*/, float delta_seconds) override
    {
        elapsed += delta_seconds;
    }
    float elapsed = 0.0f;
};

class InputSinkEntity final : public Entity
{
  public:
    [[nodiscard]] std::string_view Classname() const override
    {
        return "test_input_sink";
    }
    [[nodiscard]] bool AcceptsInput(std::string_view input) const override
    {
        return input == "Record" || Entity::AcceptsInput(input);
    }
    bool HandleInput(
        CEngine::Context &context, const EntityInput &input) override
    {
        if (input.name != "Record")
        {
            return Entity::HandleInput(context, input);
        }
        ++received;
        last_parameter = input.parameter;
        last_source = input.source;
        last_activator = input.activator;
        return true;
    }

    std::uint32_t received = 0;
    std::string last_parameter;
    EntityHandle last_source;
    EntityHandle last_activator;
};

/**
 * @brief TODO: Describe FakeInputBackend.
 */
class FakeInputBackend final : public CEngine::Input::IInputBackend
{
  public:
    /**
     * @brief TODO: Describe BeginFrame.
     */
    void BeginFrame() override
    {
        ++frames;
    }
    /**
     * @brief TODO: Describe IsDown.
     *
     * @param key TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsDown(CEngine::Input::Key key) const override
    {
        return key == CEngine::Input::Key::W || key == CEngine::Input::Key::LeftShift;
    }
    /**
     * @brief TODO: Describe PointerDelta.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] glm::vec2 PointerDelta() const override
    {
        return {4.0f, -2.0f};
    }
    /**
     * @brief TODO: Describe SetPointerCaptured.
     *
     * @param value TODO: Describe this parameter.
     */
    void SetPointerCaptured(bool value) override
    {
        pointer_captured = value;
    }

    int frames = 0;
    bool pointer_captured = false;
};

/**
 * @brief TODO: Describe GenerationalSlotsRejectStaleHandles.
 *
 * @return TODO: Describe the return value.
 */
bool GenerationalSlotsRejectStaleHandles()
{
    Scene scene;
    auto &first = scene.CreateEntity<CEngine::Entities::PropEntity>("Crate");
    const EntityHandle old = first.GetHandle();
    if (!Expect(scene.DestroyEntity(old), "live entity should be destroyed") ||
        !Expect(!scene.IsAlive(old), "destroyed handle should be stale"))
    {
        return false;
    }
    auto &second = scene.CreateEntity<CEngine::Entities::LightEntity>("Light");
    return Expect(second.GetHandle().Index() == old.Index(), "free slot should be reused") &&
           Expect(second.GetHandle().Generation() != old.Generation(), "reused slot should advance generation") &&
           Expect(scene.GetEntity(old) == nullptr, "stale handle should not resolve");
}

/**
 * @brief TODO: Describe EntityClassesOwnTheirFields.
 *
 * @return TODO: Describe the return value.
 */
bool EntityClassesOwnTheirFields()
{
    Scene scene;
    auto &prop = scene.CreateEntity<CEngine::Entities::PropEntity>("HallwayCrate");
    prop.visible = false;
    prop.shadow_only = true;
    prop.GetTransform().position = {2.0f, 3.0f, 4.0f};
    prop.GetTransform().UpdateWorldMatrix();
    auto &light = scene.CreateEntity<CEngine::Entities::LightEntity>("KeyLight");
    light.intensity = 5.0f;
    return Expect(prop.Classname() == "prop", "prop classname should be fixed") &&
           Expect(prop.Name() == "HallwayCrate", "entity should own its name") &&
           Expect(!prop.visible, "prop should own visibility field") &&
           Expect(prop.shadow_only, "prop should own shadow-only presentation state") &&
           Expect(Near(prop.GetTransform().world_matrix[3].x, 2.0f), "entity transform should update") &&
           Expect(light.Classname() == "light" && Near(light.intensity, 5.0f), "light should own light fields") &&
           Expect(scene.EntityCount() == 2, "scene should own both entities");
}

/**
 * @brief TODO: Describe EnvironmentEntitiesOwnTheirFields.
 *
 * @return TODO: Describe the return value.
 */
bool EnvironmentEntitiesOwnTheirFields()
{
    Scene scene;
    auto &sky = scene.CreateEntity<CEngine::Entities::SkyboxEntity>("Environment");
    sky.intensity = 1.5f;
    auto &fog = scene.CreateEntity<CEngine::Entities::FogEntity>("Atmosphere");
    fog.density = 0.04f;
    fog.GetTransform().position.z = 2.0f;
    auto &post_process = scene.CreateEntity<CEngine::Entities::PostProcessEntity>("SceneLook");
    post_process.exposure = 1.25f;
    post_process.ssao_radius = 0.8f;
    return Expect(sky.Classname() == "skybox" && Near(sky.intensity, 1.5f),
                  "skybox should own its environment fields") &&
           Expect(fog.Classname() == "fog" && Near(fog.density, 0.04f) &&
                      Near(fog.GetTransform().position.z, 2.0f),
                  "fog should use actor height as base height") &&
           Expect(post_process.Classname() == "post_process" && Near(post_process.exposure, 1.25f) &&
                      Near(post_process.ssao_radius, 0.8f),
                  "post-process entity should own the scene image settings");
}

bool BaseEntitySetOwnsEssentialRuntimeFields()
{
    Scene scene;
    auto &camera =
        scene.CreateEntity<CEngine::Entities::CameraEntity>("GameplayCamera");
    camera.vertical_fov_radians = 1.1f;
    auto &audio = scene.CreateEntity<CEngine::Entities::AudioSourceEntity>(
        "AmbientLoop");
    audio.looping = true;
    auto &environment =
        scene.CreateEntity<CEngine::Entities::AudioEnvironmentEntity>(
            "RoomAudio");
    environment.wet = 0.4f;
    auto &collider =
        scene.CreateEntity<CEngine::Entities::ColliderEntity>("WorldCollider");
    collider.friction = 0.8f;
    auto &trigger =
        scene.CreateEntity<CEngine::Entities::TriggerEntity>("DoorTrigger");
    trigger.fire_once = true;
    auto &relay =
        scene.CreateEntity<CEngine::Entities::LogicRelayEntity>("DoorRelay");
    relay.fire_once = true;
    auto &timer =
        scene.CreateEntity<CEngine::Entities::LogicTimerEntity>("Pulse");
    timer.interval_seconds = 2.0f;
    auto &automatic =
        scene.CreateEntity<CEngine::Entities::LogicAutoEntity>("Startup");
    auto &marker =
        scene.CreateEntity<CEngine::Entities::MarkerEntity>("PatrolPoint");
    return Expect(camera.Classname() == "camera" &&
                      Near(camera.vertical_fov_radians, 1.1f),
                  "camera should own projection fields") &&
           Expect(audio.Classname() == "audio_source" && audio.looping,
                  "audio source should own playback fields") &&
           Expect(environment.Classname() == "audio_environment" &&
                      Near(environment.wet, 0.4f),
                  "audio environment should own room fields") &&
           Expect(collider.Classname() == "collider" &&
                      Near(collider.friction, 0.8f),
                  "collider should own body fields") &&
           Expect(trigger.Classname() == "trigger_volume" &&
                      trigger.fire_once,
                  "trigger should own overlap behavior") &&
           Expect(relay.Classname() == "logic_relay" && relay.fire_once,
                  "relay should own fire-once behavior") &&
           Expect(timer.Classname() == "logic_timer" &&
                      Near(timer.interval_seconds, 2.0f),
                  "timer should own interval behavior") &&
           Expect(automatic.Classname() == "logic_auto" &&
                      marker.Classname() == "marker",
                  "startup and spatial marker entities should be concrete") &&
           Expect(scene.EntityCount() == 9,
                  "the essential base entity set should be constructible");
}

bool EntityConnectionsDispatchDelayParametersAndFireLimits()
{
    Scene scene;
    auto &relay =
        scene.CreateEntity<CEngine::Entities::LogicRelayEntity>("Relay");
    auto &activator =
        scene.CreateEntity<CEngine::Entities::MarkerEntity>("Activator");
    auto &sink = scene.CreateEntity<InputSinkEntity>("Sink");
    scene.AddConnection({
        relay.GetHandle(),
        sink.GetHandle(),
        "OnTrigger",
        "Record",
        "fixed parameter",
        0.1f,
        2,
    });
    CEngine::Context context;
    scene.Activate(context);
    scene.DispatchInput(
        relay.GetHandle(), "Trigger", activator.GetHandle(),
        activator.GetHandle(), "ignored parameter");
    scene.Update(context, 0.05f);
    if (!Expect(sink.received == 0,
                "delayed input should not dispatch early"))
    {
        return false;
    }
    scene.Update(context, 0.05f);
    if (!Expect(sink.received == 1 &&
                    sink.last_parameter == "fixed parameter" &&
                    sink.last_source == relay.GetHandle() &&
                    sink.last_activator == activator.GetHandle(),
                "connection should preserve source, activator, and parameter"))
    {
        return false;
    }
    scene.DispatchInput(
        relay.GetHandle(), "Trigger", {}, activator.GetHandle());
    scene.Update(context, 0.1f);
    scene.DispatchInput(
        relay.GetHandle(), "Trigger", {}, activator.GetHandle());
    scene.Update(context, 0.1f);
    if (!Expect(sink.received == 2,
                "connection fire limit should suppress later emissions"))
    {
        return false;
    }
    scene.Stop();
    scene.Activate(context);
    scene.DispatchInput(
        relay.GetHandle(), "Trigger", {}, activator.GetHandle());
    scene.Update(context, 0.1f);
    return Expect(
        sink.received == 3,
        "scene reactivation should reset relay and connection fire state");
}

bool CameraEntityOwnsSceneCameraSelection()
{
    Scene scene;
    auto &first =
        scene.CreateEntity<CEngine::Entities::CameraEntity>("FirstCamera");
    auto &second =
        scene.CreateEntity<CEngine::Entities::CameraEntity>("SecondCamera");
    first.GetTransform().position = {1.0f, 2.0f, 3.0f};
    first.GetTransform().UpdateWorldMatrix();
    SceneSettings settings;
    settings.active_entity = first.GetHandle();
    scene.SetSettings(settings);
    CEngine::Renderer::RenderSystem rendering;
    CEngine::Context context;
    context.rendering = &rendering;
    scene.Activate(context);
    if (!Expect(rendering.ActiveCamera().position ==
                    first.GetTransform().position,
                "active camera entity should publish to the renderer"))
    {
        return false;
    }
    scene.DispatchInput(
        second.GetHandle(), "Activate", first.GetHandle(),
        first.GetHandle());
    return Expect(scene.Settings().active_entity == second.GetHandle(),
                  "Activate input should change the scene camera");
}

/**
 * @brief TODO: Describe SlotLookupWorks.
 *
 * @return TODO: Describe the return value.
 */
bool SlotLookupWorks()
{
    Scene scene;
    auto &prop = scene.CreateEntity<CEngine::Entities::PropEntity>("Crate");
    return Expect(scene.GetEntity(prop.GetHandle()) == &prop, "runtime handle should find entity") &&
           Expect(scene.Entities()[prop.GetHandle().Index()].get() == &prop, "single entity list should own entity");
}

/**
 * @brief TODO: Describe LightModesHaveExplicitRuntimeDirectSemantics.
 *
 * @return TODO: Describe the return value.
 */
bool LightModesHaveExplicitRuntimeDirectSemantics()
{
    using CEngine::Entities::HasRuntimeDirectLighting;
    return Expect(HasRuntimeDirectLighting(CEngine::Generated::EngineEntities::LightMode::Realtime),
                  "realtime lights should provide runtime direct lighting") &&
           Expect(HasRuntimeDirectLighting(CEngine::Generated::EngineEntities::LightMode::Mixed),
                  "mixed lights should provide runtime direct lighting and shadows") &&
           Expect(!HasRuntimeDirectLighting(CEngine::Generated::EngineEntities::LightMode::Baked),
                  "baked lights should not be submitted to runtime direct lighting");
}

/**
 * @brief TODO: Describe LightRendererRecordFollowsEntityLifetime.
 *
 * @return TODO: Describe the return value.
 */
bool LightRendererRecordFollowsEntityLifetime()
{
    CEngine::Renderer::RenderSystem rendering;
    CEngine::Context context;
    context.rendering = &rendering;
    CEngine::Entities::LightEntity light;
    light.mode = CEngine::Generated::EngineEntities::LightMode::Baked;
    light.Initialize(context);
    if (!Expect(rendering.GetDirectLights().size() == 1, "light initialization should register one renderer light") ||
        !Expect(!rendering.GetDirectLights()[0].enabled, "a baked light should keep a disabled renderer record"))
    {
        return false;
    }

    light.mode = CEngine::Generated::EngineEntities::LightMode::Realtime;
    light.Update(context, 0.0f);
    if (!Expect(rendering.GetDirectLights().size() == 1 && rendering.GetDirectLights()[0].enabled,
                "changing light mode should update the existing renderer light"))
    {
        return false;
    }

    light.mode = CEngine::Generated::EngineEntities::LightMode::Baked;
    light.Update(context, 0.0f);
    if (!Expect(rendering.GetDirectLights().size() == 1 && !rendering.GetDirectLights()[0].enabled,
                "disabling runtime lighting should not remove its renderer light"))
    {
        return false;
    }

    light.Shutdown(context);
    const auto replacement = rendering.RegisterLight(CEngine::Renderer::Light{});
    return Expect(replacement.Index() == 0 && replacement.Generation() == 2,
                  "the renderer light should be released exactly once during shutdown");
}

/**
 * @brief TODO: Describe GameLayerCanAddEntitiesAndDriveLifecycle.
 *
 * @return TODO: Describe the return value.
 */
bool GameLayerCanAddEntitiesAndDriveLifecycle()
{
    Scene scene;
    auto &custom = scene.CreateEntity<CustomEntity>("");
    auto &fog = scene.CreateEntity<CEngine::Entities::FogEntity>("");
    fog.density = 0.125f;
    auto &post_process = scene.CreateEntity<CEngine::Entities::PostProcessEntity>("");
    post_process.exposure = 1.5f;
    post_process.ssao_radius = 0.9f;
    CEngine::Renderer::RenderSystem rendering;
    CEngine::Input::InputSystem input;
    CEngine::Context context;
    context.rendering = &rendering;
    context.input = &input;
    scene.Activate(context);
    scene.Update(context, 0.25f);
    return Expect(Near(custom.elapsed, 0.25f), "custom entity should receive the engine lifecycle") &&
           Expect(Near(rendering.GetExponentialHeightFog().density, 0.125f),
                  "fog entity should update the renderer API") &&
           Expect(Near(rendering.GetPostProcessSettings().exposure, 1.5f) &&
                      Near(rendering.GetSSAOSettings().radius, 0.9f),
                  "post-process entity should update the renderer APIs");
}

/**
 * @brief TODO: Describe InputSystemOwnsPlatformBackend.
 *
 * @return TODO: Describe the return value.
 */
bool InputSystemOwnsPlatformBackend()
{
    auto backend = std::make_unique<FakeInputBackend>();
    FakeInputBackend *backend_ptr = backend.get();
    CEngine::Input::InputSystem input(std::move(backend));
    const auto move = input.RegisterAction("move");
    const auto look = input.RegisterAction("look");
    input.BindKey(move, CEngine::Input::Key::W);
    input.BindPointerAxis(look, CEngine::Input::PointerAxis::X, 0.5f);
    input.Set(move, -1.0f);
    input.BeginFrame();
    return Expect(backend_ptr->frames == 1, "input system should begin its platform backend frame") &&
           Expect(Near(input.Value(move), 1.0f), "registered key binding should produce its logical action") &&
           Expect(Near(input.Value(look), 2.0f), "registered pointer binding should scale its logical action") &&
           Expect(input.IsDown(CEngine::Input::Key::W) && input.PointerDelta() == glm::vec2(4.0f, -2.0f),
                  "input system should expose backend-neutral device state");
}
} // namespace

/**
 * @brief TODO: Describe main.
 *
 * @param argc TODO: Describe this parameter.
 * @param argv TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
int main(int argc, char **argv)
{
    if (argc == 2)
    {
        CEngine::Assets::Store assets(std::filesystem::path(argv[1]).parent_path());
        auto scene = CEngine::Assets::LoadScene(argv[1], assets);
        const bool loaded =
            Expect(scene != nullptr, "scene should load") &&
            Expect(scene->EntityCount() == 6, "loaded scene should preserve its authored entities") &&
            Expect(scene->Entities()[0]->Classname() == "light", "loaded entity should preserve its light class") &&
            Expect(scene->Connections().size() == 1 && scene->Connections()[0].action == "Enable",
                   "loaded scene should resolve its entity connection") &&
            Expect(Near(scene->Entities()[0]->GetTransform().position.x, 1.0f), "loaded transform should match") &&
            Expect(dynamic_cast<CEngine::Entities::PropEntity *>(scene->Entities()[2].get())->motion ==
                       CEngine::Generated::EngineEntities::PhysicsMotion::Dynamic,
                   "loaded prop should preserve its physics motion") &&
            Expect(
                static_cast<bool>(dynamic_cast<CEngine::Entities::PropEntity *>(scene->Entities()[2].get())->collision),
                "loaded prop should preserve its collision asset") &&
            Expect(scene->Entities()[4]->Classname() == "physics_constraint",
                   "loaded scene should preserve its authored physics constraint") &&
            Expect(
                Near(dynamic_cast<CEngine::Entities::PostProcessEntity *>(scene->Entities()[5].get())->exposure, 1.25f),
                "loaded post-process entity should preserve its scene settings");
        if (!loaded)
        {
            return 1;
        }
        PhysicsSystem physics;
#ifdef CENGINE_ENABLE_JOLT_PHYSICS
        if (!Expect(physics.Initialize({}), "physics should initialize for loaded prop"))
        {
            return 1;
        }
        CEngine::Context context;
        context.assets = &assets;
        context.physics = &physics;
        scene->Activate(context);
        const bool activated = Expect(physics.BodyCount() == 2 && physics.ConstraintCount() == 1,
                                      "loaded collision assets should create two Jolt bodies and their constraint");
        scene->Update(context, 1.0f / 60.0f);
        scene->Stop();
        return activated && Expect(physics.BodyCount() == 0 && physics.ConstraintCount() == 0,
                                   "scene shutdown should release Jolt bodies and constraints")
                   ? 0
                   : 1;
#else
        return Expect(!physics.Initialize({}), "physics should report that no backend is compiled") ? 0 : 1;
#endif
    }
    return GenerationalSlotsRejectStaleHandles() && EntityClassesOwnTheirFields() &&
                   EnvironmentEntitiesOwnTheirFields() &&
                   BaseEntitySetOwnsEssentialRuntimeFields() &&
                   EntityConnectionsDispatchDelayParametersAndFireLimits() &&
                   CameraEntityOwnsSceneCameraSelection() && SlotLookupWorks() &&
                   LightModesHaveExplicitRuntimeDirectSemantics() && LightRendererRecordFollowsEntityLifetime() &&
                   GameLayerCanAddEntitiesAndDriveLifecycle() && InputSystemOwnsPlatformBackend()
               ? 0
               : 1;
}
