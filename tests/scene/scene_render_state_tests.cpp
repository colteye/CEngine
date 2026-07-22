#include "renderer/render_system.h"
#include "camera.h"
#include "assets/asset_database.h"
#include "entity/light_entity.h"
#include "entity/prop_entity.h"
#include "physics/physics_backend.h"
#include "physics/physics_system.h"
#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
#include "physics/jolt/jolt_physics_backend.h"
#endif
#include "scene/scene.h"
#include "scene/scene_loader.h"
#include "scene/scene_physics_state.h"
#include "scene/scene_render_state.h"

#include <iostream>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>

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
        created.push_back(desc);
        return {static_cast<std::uint32_t>(created.size() + 6), 1};
    }
    void DestroyBody(PhysicsBodyHandle body) override { destroyed.push_back(body); }
    glm::mat4 GetBodyTransform(PhysicsBodyHandle) const override
    { return glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 5.0f, 6.0f)); }

    std::vector<PhysicsBodyDesc> created;
    std::vector<PhysicsBodyHandle> destroyed;
};

#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
bool JoltRejectsStaleBodyHandles()
{
    PhysicsSystem physics(std::make_unique<JoltPhysicsBackend>());
    PhysicsSystemDesc system_desc;
    system_desc.gravity = glm::vec3(0.0f);
    if (!Expect(physics.Initialize(system_desc), "Jolt should initialize for handle test")) return false;

    PhysicsBodyDesc first_desc;
    first_desc.motion_type = PhysicsMotionType::Static;
    first_desc.position = glm::vec3(2.0f, 0.0f, 0.0f);
    const PhysicsBodyHandle stale = physics.CreateBody(first_desc);
    physics.DestroyBody(stale);

    PhysicsBodyDesc second_desc = first_desc;
    second_desc.position = glm::vec3(5.0f, 0.0f, 0.0f);
    const PhysicsBodyHandle current = physics.CreateBody(second_desc);
    return Expect(stale.index == current.index, "Jolt should reuse the freed body slot") &&
        Expect(stale.generation != current.generation, "reused Jolt body slot should advance generation") &&
        Expect(physics.GetBodyTransform(stale) == glm::mat4(1.0f),
            "stale Jolt body handle should not resolve") &&
        Expect(physics.GetBodyTransform(current)[3].x == 5.0f,
            "current Jolt body handle should resolve its body");
}
#endif

bool ValidateCookedScene(const std::filesystem::path& scene_path,
    const std::filesystem::path& project_root)
{
    CEngine::Assets::AssetDatabase assets(project_root);
    std::string error;
    std::unique_ptr<CEngine::Scene::Scene> scene =
        CEngine::Scene::LoadScene(scene_path, assets, &error);
    if (!Expect(scene != nullptr, error.c_str())) return false;

    CEngine::Scene::SceneRenderState render_state;
    if (!Expect(render_state.Activate(*scene, assets, &error), error.c_str())) return false;
    std::size_t prop_count = 0;
    std::size_t lightmapped_prop_count = 0;
    std::size_t shadow_only_prop_count = 0;
    bool lightmapped_props_are_static = true;
    std::size_t expected_realtime_lights = 0;
    glm::vec3 expected_sun_direction(0.0f);
    bool found_realtime_sun = false;
    for (const auto& entity : scene->Entities())
    {
        if (entity == nullptr) continue;
        if (entity->Classname() == "prop")
        {
            ++prop_count;
            const auto& prop = static_cast<const CEngine::Entities::PropEntity&>(*entity);
            if (prop.shadow_only)
            {
                ++shadow_only_prop_count;
                lightmapped_props_are_static = lightmapped_props_are_static && !prop.lightmap;
            }
            if (prop.lightmap)
            {
                ++lightmapped_prop_count;
                lightmapped_props_are_static = lightmapped_props_are_static && !prop.dynamic &&
                    prop.lightmap_rgbm_range > 0.0f;
            }
        }
        if (entity->Classname() == "light")
        {
            const auto& light = static_cast<const CEngine::Entities::LightEntity&>(*entity);
            if (light.Enabled() && light.mode != CEngine::Entities::LightMode::Baked)
            {
                ++expected_realtime_lights;
                if (light.type == CEngine::Entities::LightType::Sun)
                {
                    expected_sun_direction = glm::normalize(glm::vec3(
                        light.GetTransform().world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                    found_realtime_sun = true;
                }
            }
        }
    }
    try
    {
        scene->Activate();
    }
    catch (const std::exception& exception)
    {
        return Expect(false, exception.what());
    }
    const auto& ambient = CEngine::Renderer::RenderSystem::GetAmbientLighting();
    const auto& lights = CEngine::Renderer::RenderSystem::GetDirectLights();
    std::size_t shadow_only_renderable_count = 0;
    bool shadow_only_flags_are_valid = true;
    for (const auto& renderable : CEngine::Renderer::RenderSystem::GetRenderables())
    {
        if ((renderable.flags & CEngine::Renderer::RenderableFlagShadowOnly) == 0) continue;
        ++shadow_only_renderable_count;
        shadow_only_flags_are_valid = shadow_only_flags_are_valid &&
            (renderable.flags & CEngine::Renderer::RenderableFlagCastsShadow) != 0 &&
            (renderable.flags & CEngine::Renderer::RenderableFlagReceivesShadow) == 0 &&
            renderable.lightmap == nullptr;
    }
    std::size_t directional_count = 0;
    std::size_t point_count = 0;
    const CEngine::Renderer::LightRecord* sun = nullptr;
    for (const auto& light : lights)
    {
        if (light.type == CEngine::Renderer::LightType::Directional)
        {
            ++directional_count;
            sun = &light;
        }
        else if (light.type == CEngine::Renderer::LightType::Point) ++point_count;
    }
    return Expect(scene->EntityCount() > 0, "cooked scene should contain entities") &&
        Expect(prop_count > 0, "cooked scene should contain prop entities") &&
        Expect(shadow_only_prop_count == 1 && shadow_only_renderable_count == 1 &&
                shadow_only_flags_are_valid,
            "Sponza should bind one cast-only occluder renderable") &&
        Expect(lightmapped_prop_count + shadow_only_prop_count == prop_count &&
                lightmapped_props_are_static,
            "every visible Sponza prop should have a lightmap and the occluder should not") &&
        Expect(scene->AssetReferenceCount() > 0, "cooked scene should reference target assets") &&
        Expect(render_state.Active(), "cooked scene render bindings should activate") &&
        Expect(ambient.enabled && ambient.sky_color == scene->Settings().ambient_color,
            "scene environment lighting should reach the renderer") &&
        Expect(lights.size() == expected_realtime_lights,
            "cooked scene should bind every realtime and mixed light") &&
        Expect(directional_count == 1 && point_count == 0,
            "Sponza should bind its single authored directional light") &&
        Expect(sun != nullptr && sun->casts_shadows,
            "Sponza Sun should bind as a shadow-casting directional light") &&
        Expect(sun != nullptr && std::abs(glm::length(sun->direction) - 1.0f) < 0.0001f,
            "light world transform should produce a normalized renderer direction") &&
        Expect(found_realtime_sun && sun != nullptr &&
                glm::length(sun->direction - expected_sun_direction) < 0.0001f,
            "renderer light direction should match the imported entity world transform");
}
}

int main(int argc, char** argv)
{
    if (argc == 4 && std::string_view(argv[1]) == "--validate-scene")
    {
        const bool result = ValidateCookedScene(argv[2], argv[3]);
        CEngine::Renderer::RenderSystem::Shutdown();
        return result ? 0 : 1;
    }
    CEngine::Renderer::Renderable renderable;
    const auto renderable_handle = CEngine::Renderer::RenderSystem::RegisterRenderable(renderable);
    CEngine::Renderer::RenderSystem::RemoveRenderable(renderable_handle);
    const auto current_renderable = CEngine::Renderer::RenderSystem::RegisterRenderable(renderable);

    CEngine::Renderer::LightRecord light;
    const auto light_handle = CEngine::Renderer::RenderSystem::RegisterLight(light);
    CEngine::Renderer::RenderSystem::RemoveLight(light_handle);
    const auto current_light = CEngine::Renderer::RenderSystem::RegisterLight(light);

    const bool result =
        Expect(renderable_handle.index == current_renderable.index,
            "renderer should reuse a freed renderable slot") &&
        Expect(renderable_handle.generation != current_renderable.generation,
            "reused renderable slot should advance generation") &&
        Expect(CEngine::Renderer::RenderSystem::ResolveRenderable(renderable_handle) == nullptr,
            "stale renderable handle should not resolve") &&
        Expect(CEngine::Renderer::RenderSystem::ResolveRenderable(current_renderable) != nullptr,
            "current renderable handle should resolve") &&
        Expect(light_handle.index == current_light.index,
            "renderer should reuse a freed light slot") &&
        Expect(light_handle.generation != current_light.generation,
            "reused light slot should advance generation") &&
        Expect(CEngine::Renderer::RenderSystem::ResolveLight(light_handle) == nullptr,
            "stale light handle should not resolve") &&
        Expect(CEngine::Renderer::RenderSystem::ResolveLight(current_light) != nullptr,
            "current light handle should resolve");

	Camera camera;
	camera.SetAspectRatio(16.0f / 9.0f);
	const auto& frame = CEngine::Renderer::RenderSystem::GetFrameConstants();
	const bool camera_result = Expect(std::abs(frame.proj[1][1] / frame.proj[0][0] - 16.0f / 9.0f) < 0.0001f,
		"camera projection should preserve the framebuffer aspect ratio");

    CEngine::Scene::Scene scene;
    auto& prop = static_cast<CEngine::Entities::PropEntity&>(
        scene.CreateEntity("prop", "MovingCrate"));
    prop.dynamic = true;
    prop.collision_enabled = true;
    prop.collision_half_extents = {1.0f, 2.0f, 3.0f};
    prop.mass = 8.0f;
    auto& static_prop = static_cast<CEngine::Entities::PropEntity&>(
        scene.CreateEntity("prop", "Floor"));
    static_prop.collision_enabled = true;
    static_prop.collision_half_extents = {10.0f, 0.5f, 10.0f};
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
        Expect(backend_ptr->created.size() == 2, "all collidable props should reach physics") &&
        Expect(backend_ptr->created[0].motion_type == PhysicsMotionType::Dynamic,
            "dynamic prop should create a dynamic body") &&
        Expect(backend_ptr->created[0].mass == 8.0f, "dynamic prop mass should reach physics") &&
        Expect(backend_ptr->created[1].motion_type == PhysicsMotionType::Static,
            "static prop should create a static body") &&
        Expect(prop.GetTransform().position == glm::vec3(4.0f, 5.0f, 6.0f),
            "dynamic body transform should return to its entity");
    physics_state.Stop();
    const bool cleanup_result = Expect(backend_ptr->destroyed ==
            std::vector<PhysicsBodyHandle>({{8, 1}, {7, 1}}),
        "scene physics shutdown should destroy all bodies in reverse order");
#if defined(CENGINE_ENABLE_JOLT_PHYSICS)
    const bool generation_result = JoltRejectsStaleBodyHandles();
#else
    const bool generation_result = true;
#endif
    CEngine::Renderer::RenderSystem::Shutdown();
    return result && camera_result && physics_result && cleanup_result && generation_result ? 0 : 1;
}
