#include "renderer/render_system.h"
#include "entity/dynamic_prop.h"
#include "physics/physics_backend.h"
#include "physics/physics_system.h"
#include "scene/scene.h"
#include "scene/scene_physics_state.h"

#include <iostream>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

namespace {
bool Expect(bool value, const char* message)
{
    if (!value) std::cerr << message << '\n';
    return value;
}

class FakePhysicsBackend final : public IPhysicsBackend {
public:
    bool Initialize(const PhysicsSystemDesc&) override { return true; }
    void Shutdown() override {}
    void Step(float) override {}
    PhysicsBodyHandle CreateBody(const PhysicsBodyDesc& desc) override
    {
        created = desc;
        return 7;
    }
    void DestroyBody(PhysicsBodyHandle body) override { destroyed = body; }
    glm::mat4 GetBodyTransform(PhysicsBodyHandle) const override
    { return glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 5.0f, 6.0f)); }

    PhysicsBodyDesc created;
    PhysicsBodyHandle destroyed = kInvalidPhysicsBodyHandle;
};
}

int main()
{
    CEngine::Renderer::Renderable renderable;
    const auto renderable_handle = CEngine::Renderer::RenderSystem::RegisterRenderable(renderable);
    CEngine::Renderer::RenderSystem::RemoveRenderable(renderable_handle);

    CEngine::Renderer::LightRecord light;
    const auto light_handle = CEngine::Renderer::RenderSystem::RegisterLight(light);
    CEngine::Renderer::RenderSystem::RemoveLight(light_handle);

    const bool result =
        Expect(CEngine::Renderer::RenderSystem::GetRenderables()[renderable_handle].mesh == nullptr,
            "removed renderable should no longer reference scene resources") &&
        Expect(!CEngine::Renderer::RenderSystem::GetDirectLights()[light_handle].enabled,
            "removed scene light should be disabled");

    CEngine::Scene::Scene scene;
    auto& prop = static_cast<CEngine::Entities::DynamicProp&>(
        scene.CreateEntity("prop_dynamic", "MovingCrate"));
    prop.physics_enabled = true;
    prop.collision_half_extents = {1.0f, 2.0f, 3.0f};
    prop.mass = 8.0f;
    auto backend = std::make_unique<FakePhysicsBackend>();
    FakePhysicsBackend* backend_ptr = backend.get();
    PhysicsSystem physics(std::move(backend));
    physics.Initialize({});
    CEngine::Scene::ScenePhysicsState physics_state;
    std::string error;
    const bool activated = physics_state.Activate(scene, physics, &error);
    physics_state.Step(1.0f / 60.0f);
    const bool physics_result =
        Expect(activated, error.c_str()) &&
        Expect(backend_ptr->created.mass == 8.0f, "dynamic prop mass should reach physics") &&
        Expect(prop.GetTransform().position == glm::vec3(4.0f, 5.0f, 6.0f),
            "dynamic body transform should return to its entity");
    physics_state.Stop();
    const bool cleanup_result = Expect(backend_ptr->destroyed == 7,
        "scene physics shutdown should destroy its body");
    CEngine::Renderer::RenderSystem::Shutdown();
    return result && physics_result && cleanup_result ? 0 : 1;
}
