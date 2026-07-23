#include "assets/asset_store.h"
#include "assets/scene_loader.h"
#include "engine_context.h"
#include "entity/entity_factory.h"
#include "entity/fog_entity.h"
#include "entity/light_entity.h"
#include "entity/post_process_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
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

class CustomEntity final : public Entity
{
  public:
    [[nodiscard]] std::string_view Classname() const override
    {
        return "game_custom";
    }
    void Update(CEngine::EngineContext & /*unused*/, float delta_seconds) override
    {
        elapsed += delta_seconds;
    }
    float elapsed = 0.0f;
};

class FakeInputBackend final : public CEngine::Input::IInputBackend
{
  public:
    void BeginFrame() override
    {
        ++frames;
    }
    [[nodiscard]] bool IsDown(CEngine::Input::Key key) const override
    {
        return key == CEngine::Input::Key::W || key == CEngine::Input::Key::LeftShift;
    }
    [[nodiscard]] glm::vec2 PointerDelta() const override
    {
        return {4.0f, -2.0f};
    }
    void SetPointerCaptured(bool value) override
    {
        pointer_captured = value;
    }

    int frames = 0;
    bool pointer_captured = false;
};

bool GenerationalSlotsRejectStaleHandles()
{
    Scene scene;
    auto &first = scene.CreateEntity<CEngine::Entities::PropEntity>("Crate");
    const EntityId old = first.Id();
    if (!Expect(scene.DestroyEntity(old), "live entity should be destroyed") ||
        !Expect(!scene.IsAlive(old), "destroyed handle should be stale"))
    {
        return false;
    }
    auto &second = scene.CreateEntity<CEngine::Entities::LightEntity>("Light");
    return Expect(second.Id().index == old.index, "free slot should be reused") &&
           Expect(second.Id().generation != old.generation, "reused slot should advance generation") &&
           Expect(scene.GetEntity(old) == nullptr, "stale handle should not resolve");
}

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
           Expect(fog.Classname() == "exponential_height_fog" && Near(fog.density, 0.04f) &&
                      Near(fog.GetTransform().position.z, 2.0f),
                  "fog should use actor height as base height") &&
           Expect(post_process.Classname() == "post_process" && Near(post_process.exposure, 1.25f) &&
                      Near(post_process.ssao_radius, 0.8f),
                  "post-process entity should own the scene image settings");
}

bool SlotLookupWorks()
{
    Scene scene;
    auto &prop = scene.CreateEntity<CEngine::Entities::PropEntity>("Crate");
    return Expect(scene.GetEntity(prop.Id()) == &prop, "runtime handle should find entity") &&
           Expect(scene.Entities()[prop.Id().index].get() == &prop, "single entity list should own entity");
}

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

bool LightRendererRecordFollowsEntityLifetime()
{
    CEngine::Renderer::RenderSystem rendering;
    CEngine::EngineContext context;
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
    return Expect(replacement.index == 0 && replacement.generation == 2,
                  "the renderer light should be released exactly once during shutdown");
}

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
    CEngine::EngineContext context;
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

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        CEngine::Assets::AssetStore assets(std::filesystem::path(argv[1]).parent_path());
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
        if (!Expect(physics.Initialize({}), "physics should initialize for loaded prop"))
        {
            return 1;
        }
        CEngine::EngineContext context;
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
    }
    return GenerationalSlotsRejectStaleHandles() && EntityClassesOwnTheirFields() &&
                   EnvironmentEntitiesOwnTheirFields() && SlotLookupWorks() &&
                   LightModesHaveExplicitRuntimeDirectSemantics() && LightRendererRecordFollowsEntityLifetime() &&
                   GameLayerCanAddEntitiesAndDriveLifecycle() && InputSystemOwnsPlatformBackend()
               ? 0
               : 1;
}
