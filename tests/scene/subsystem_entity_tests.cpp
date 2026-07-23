#include "assets/asset_store.h"
#include "assets/scene_loader.h"
#include "engine_context.h"
#include "entity/entity_factory.h"
#include "entity/fog_entity.h"
#include "entity/light_entity.h"
#include "entity/player_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
#include "input/actions.h"
#include "input/input_system.h"
#include "physics/physics_system.h"
#include "renderer/camera.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

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

#ifdef CENGINE_ENABLE_JOLT_PHYSICS
bool JoltRejectsStaleBodyHandles()
{
    PhysicsSystem physics;
    PhysicsSystemDesc system_desc;
    system_desc.gravity = glm::vec3(0.0f);
    if (!Expect(physics.Initialize(system_desc), "Jolt should initialize for handle test"))
    {
        return false;
    }

    PhysicsBodyDesc first_desc;
    first_desc.motion_type = PhysicsMotionType::Static;
    first_desc.position = glm::vec3(2.0f, 0.0f, 0.0f);
    PhysicsShape shape;
    const PhysicsBodyHandle stale = physics.CreateBody(first_desc, shape);
    physics.DestroyBody(stale);

    PhysicsBodyDesc second_desc = first_desc;
    second_desc.position = glm::vec3(5.0f, 0.0f, 0.0f);
    const PhysicsBodyHandle current = physics.CreateBody(second_desc, shape);
    return Expect(stale.index == current.index, "Jolt should reuse the freed body slot") &&
           Expect(stale.generation != current.generation, "reused Jolt body slot should advance generation") && [&] {
               PhysicsBodyState state;
               return Expect(!physics.GetBodyState(stale, state), "stale Jolt body handle should not resolve") &&
                      Expect(physics.GetBodyState(current, state) && state.position.x == 5.0f,
                             "current Jolt body handle should resolve its body");
           }();
}

bool JoltQueriesAndContactsWork()
{
    PhysicsSystem physics;
    PhysicsSystemDesc system_desc;
    system_desc.gravity = glm::vec3(0.0f);
    if (!Expect(physics.Initialize(system_desc), "Jolt should initialize for query test"))
    {
        return false;
    }

    PhysicsShape box;
    PhysicsBodyDesc target_desc;
    target_desc.position = {5.0f, 0.0f, 0.0f};
    target_desc.collision_layer = 3;
    const PhysicsBodyHandle target = physics.CreateBody(target_desc, box);

    PhysicsQueryHit ray_hit;
    const bool ray_result = Expect(physics.RayCast({-5.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, ray_hit, 1u << 3u) &&
                                       ray_hit.body == target && ray_hit.fraction > 0.0f && ray_hit.fraction < 1.0f,
                                   "filtered ray cast should return the target body") &&
                            Expect(!physics.RayCast({-5.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, ray_hit, 1u << 4u),
                                   "ray mask should reject other collision layers");

    PhysicsShape sphere;
    sphere.type = PhysicsShapeType::Sphere;
    sphere.radius = 0.25f;
    PhysicsQueryHit cast_hit;
    const bool shape_cast_result =
        Expect(physics.ShapeCast(sphere, {0.0f, 0.0f, 0.0f}, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), {10.0f, 0.0f, 0.0f},
                                 cast_hit, 1u << 3u) &&
                   cast_hit.body == target,
               "convex shape cast should return the target body");
    PhysicsBodyHandle overlaps[2];
    const bool overlap_result = Expect(
        physics.Overlap(box, {5.0f, 0.0f, 0.0f}, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), overlaps, 2, 1u << 3u) == 1 &&
            overlaps[0] == target,
        "overlap should return each matching body once");

    PhysicsShape plane;
    plane.type = PhysicsShapeType::Plane;
    plane.half_extents = {10.0f, 10.0f, 0.0f};
    PhysicsBodyDesc plane_desc;
    plane_desc.position = {20.0f, 0.0f, 0.0f};
    const PhysicsBodyHandle plane_body = physics.CreateBody(plane_desc, plane);
    PhysicsQueryHit plane_hit;
    const bool plane_result =
        Expect(plane_body && physics.RayCast({20.0f, 0.0f, 2.0f}, {0.0f, 0.0f, -4.0f}, plane_hit) &&
                   plane_hit.body == plane_body,
               "static plane should create and answer queries");

    PhysicsBodyDesc moving_desc;
    moving_desc.motion_type = PhysicsMotionType::Dynamic;
    moving_desc.collision_layer = 1;
    const PhysicsBodyHandle moving = physics.CreateBody(moving_desc, box);
    PhysicsBodyDesc sensor_desc;
    sensor_desc.sensor = true;
    sensor_desc.collision_layer = 2;
    const PhysicsBodyHandle sensor = physics.CreateBody(sensor_desc, box);
    physics.Step(1.0f / 60.0f);
    PhysicsContactEvent events[8];
    const std::size_t event_count = physics.DrainContactEvents(events, 8);
    bool found_sensor_begin = false;
    for (std::size_t index = 0; index < event_count; ++index)
    {
        const PhysicsContactEvent &event = events[index];
        found_sensor_begin =
            found_sensor_begin ||
            (event.type == PhysicsContactType::Begin && event.sensor &&
             ((event.first == moving && event.second == sensor) || (event.first == sensor && event.second == moving)));
    }
    const bool contact_result =
        Expect(found_sensor_begin && physics.PendingContactEventCount() == 0 && physics.DroppedContactEventCount() == 0,
               "sensor overlap should publish one bounded normalized begin event");

    std::vector<PhysicsConstraintHandle> constraints;
    for (PhysicsConstraintType type :
         {PhysicsConstraintType::Fixed, PhysicsConstraintType::Point, PhysicsConstraintType::Hinge,
          PhysicsConstraintType::Slider, PhysicsConstraintType::Distance, PhysicsConstraintType::Cone})
    {
        PhysicsConstraintDesc constraint_desc;
        constraint_desc.type = type;
        constraint_desc.first = moving;
        constraint_desc.second = target;
        constraint_desc.first_anchor = {0.0f, 0.0f, 0.0f};
        constraint_desc.second_anchor = {5.0f, 0.0f, 0.0f};
        constraint_desc.minimum = type == PhysicsConstraintType::Distance ? 1.0f : -1.0f;
        constraint_desc.maximum = type == PhysicsConstraintType::Distance ? 10.0f : 1.0f;
        constraints.push_back(physics.CreateConstraint(constraint_desc));
    }
    const bool constraints_created =
        std::all_of(constraints.begin(), constraints.end(),
                    [&physics](PhysicsConstraintHandle constraint) { return physics.IsValid(constraint); }) &&
        physics.ConstraintCount() == constraints.size() &&
        physics.SetConstraintMotor(constraints[2], PhysicsMotorMode::Velocity, 1.0f, 10.0f) &&
        physics.SetConstraintMotor(constraints[3], PhysicsMotorMode::Position, 0.5f, 10.0f) &&
        !physics.SetConstraintMotor(constraints[0], PhysicsMotorMode::Velocity, 1.0f, 10.0f);
    const PhysicsConstraintHandle stale_constraint = constraints.front();
    for (PhysicsConstraintHandle constraint : constraints)
    {
        physics.DestroyConstraint(constraint);
    }
    const bool constraint_result =
        Expect(constraints_created && physics.ConstraintCount() == 0 && !physics.IsValid(stale_constraint),
               "standard constraints should create, destroy, and reject stale handles");

    physics.DestroyBody(sensor);
    physics.DestroyBody(moving);

    PhysicsShape character_floor_shape;
    character_floor_shape.half_extents = {10.0f, 10.0f, 0.5f};
    PhysicsBodyDesc character_floor_desc;
    character_floor_desc.position = {0.0f, 0.0f, -0.5f};
    const PhysicsBodyHandle character_floor = physics.CreateBody(character_floor_desc, character_floor_shape);
    PhysicsCharacterDesc character_desc;
    character_desc.position = {0.0f, 0.0f, 0.05f};
    const PhysicsCharacterHandle character = physics.CreateCharacter(character_desc);
    PhysicsCharacterState character_state;
    const bool character_result = Expect(
        character && physics.SetCharacterVelocity(character, {1.0f, 0.0f, 0.0f}) &&
            physics.UpdateCharacter(character, 1.0f / 60.0f) && physics.GetCharacterState(character, character_state) &&
            character_state.position.x > 0.0f && physics.SetCharacterHeight(character, 1.2f),
        "virtual character should move, report state, and crouch");
    physics.DestroyCharacter(character);
    const bool character_cleanup = Expect(physics.CharacterCount() == 0 && !physics.IsValid(character),
                                          "destroyed character handles should become stale");

    physics.DestroyBody(character_floor);
    physics.DestroyBody(target);
    return ray_result && shape_cast_result && overlap_result && plane_result && contact_result && constraint_result &&
           character_result && character_cleanup;
}
#endif

bool ValidateCookedScene(const std::filesystem::path &scene_path, const std::filesystem::path &project_root)
{
    CEngine::Assets::AssetStore assets(project_root);
    CEngine::Input::InputSystem input;
    const Viewer::Actions actions = Viewer::RegisterActions(input);
    CEngine::Entities::EntityFactory factory;
    if (!Expect(factory.Register<Viewer::PlayerEntity, Viewer::Generated::Player>("player", actions),
                "viewer player type should register"))
    {
        return false;
    }
    CEngine::Renderer::RenderSystem renderer;
    std::unique_ptr<CEngine::Scene::Scene> scene = CEngine::Assets::LoadScene(scene_path, assets, factory);
    if (!Expect(scene != nullptr, "scene should load"))
    {
        return false;
    }

    CEngine::Renderer::AmbientLighting initial_ambient;
    initial_ambient.sky_color = scene->Settings().ambient_color;
    initial_ambient.ground_color = scene->Settings().ambient_color * 0.35f;
    initial_ambient.intensity = scene->Settings().exposure;
    initial_ambient.enabled = glm::any(glm::greaterThan(scene->Settings().ambient_color, glm::vec3(0.0f)));
    renderer.SetAmbientLighting(initial_ambient);
    std::size_t prop_count = 0;
    std::size_t visible_prop_count = 0;
    std::size_t lightmapped_prop_count = 0;
    std::size_t shadow_only_prop_count = 0;
    bool lightmapped_props_are_static = true;
    std::size_t expected_realtime_lights = 0;
    glm::vec3 expected_sun_direction(0.0f);
    bool found_realtime_sun = false;
    bool found_skybox = false;
    bool found_fog = false;
    for (const auto &entity : scene->Entities())
    {
        if (entity == nullptr)
        {
            continue;
        }
        if (entity->Classname() == "prop")
        {
            ++prop_count;
            const auto &prop = dynamic_cast<const CEngine::Entities::PropEntity &>(*entity);
            if (prop.Enabled() && prop.visible)
            {
                ++visible_prop_count;
            }
            if (prop.shadow_only)
            {
                ++shadow_only_prop_count;
                lightmapped_props_are_static = lightmapped_props_are_static && !prop.lightmap;
            }
            if (prop.lightmap)
            {
                ++lightmapped_prop_count;
                lightmapped_props_are_static =
                    lightmapped_props_are_static &&
                    prop.motion != CEngine::Generated::EngineEntities::PhysicsMotion::Dynamic &&
                    prop.motion != CEngine::Generated::EngineEntities::PhysicsMotion::Kinematic &&
                    prop.lightmap_rgbm_range > 0.0f;
            }
        }
        if (entity->Classname() == "light")
        {
            const auto &light = dynamic_cast<const CEngine::Entities::LightEntity &>(*entity);
            if (light.Enabled() && light.mode != CEngine::Generated::EngineEntities::LightMode::Baked)
            {
                ++expected_realtime_lights;
                if (light.type == CEngine::Generated::EngineEntities::LightType::Sun)
                {
                    expected_sun_direction = glm::normalize(
                        glm::vec3(light.GetTransform().world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
                    found_realtime_sun = true;
                }
            }
        }
        if (entity->Classname() == "skybox")
        {
            found_skybox = true;
        }
        if (entity->Classname() == "exponential_height_fog")
        {
            found_fog = true;
        }
    }
    try
    {
        CEngine::EngineContext context;
        context.assets = &assets;
        context.rendering = &renderer;
        context.input = &input;
        scene->Activate(context);
    }
    catch (const std::exception &exception)
    {
        return Expect(false, exception.what());
    }
    const auto &ambient = renderer.GetAmbientLighting();
    const auto &ibl = renderer.GetImageBasedLighting();
    const auto &fog = renderer.GetExponentialHeightFog();
    const auto &lights = renderer.GetDirectLights();
    std::size_t shadow_only_mesh_instance_count = 0;
    std::size_t active_mesh_instance_count = 0;
    bool shadow_only_flags_are_valid = true;
    for (const auto &mesh_instance : renderer.GetMeshInstances())
    {
        if (mesh_instance.mesh != nullptr)
        {
            ++active_mesh_instance_count;
        }
        if ((mesh_instance.flags & CEngine::Renderer::MeshInstanceFlagShadowOnly) == 0)
        {
            continue;
        }
        ++shadow_only_mesh_instance_count;
        shadow_only_flags_are_valid = shadow_only_flags_are_valid &&
                                      (mesh_instance.flags & CEngine::Renderer::MeshInstanceFlagCastsShadow) != 0 &&
                                      mesh_instance.lightmap == nullptr;
    }
    std::size_t directional_count = 0;
    std::size_t point_count = 0;
    const CEngine::Renderer::Light *sun = nullptr;
    for (const auto &light : lights)
    {
        if (light.type == CEngine::Renderer::LightType::Directional)
        {
            ++directional_count;
            sun = &light;
        }
        else if (light.type == CEngine::Renderer::LightType::Point)
        {
            ++point_count;
        }
    }
    return Expect(scene->EntityCount() > 0, "cooked scene should contain entities") &&
           Expect(prop_count > 0, "cooked scene should contain prop entities") &&
           Expect(active_mesh_instance_count == visible_prop_count,
                  "each visible prop should own exactly one renderer record") &&
           Expect(shadow_only_prop_count == 1 && shadow_only_mesh_instance_count == 1 && shadow_only_flags_are_valid,
                  "Sponza should bind one cast-only occluder mesh instance") &&
           Expect(lightmapped_prop_count + shadow_only_prop_count == prop_count && lightmapped_props_are_static,
                  "every visible Sponza prop should have a lightmap and the occluder should not") &&
           Expect(scene->AssetReferenceCount() > 0, "cooked scene should reference target assets") &&
           Expect(found_skybox && ibl.enabled && !ambient.enabled && ibl.panorama != nullptr && !ibl.panorama->Empty(),
                  "authored skybox should replace fallback ambient lighting with IBL") &&
           Expect(found_fog && fog.enabled && fog.density > 0.0f,
                  "authored exponential height fog should reach the renderer") &&
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
} // namespace

int main(int argc, char **argv)
{
    if (argc == 4 && std::string_view(argv[1]) == "--validate-scene")
    {
        const bool result = ValidateCookedScene(argv[2], argv[3]);
        return result ? 0 : 1;
    }
    CEngine::Renderer::RenderSystem renderer;
    auto mesh = std::make_shared<CEngine::Renderer::Mesh>();
    mesh->vertices.resize(3);
    mesh->indices = {0, 1, 2};
    mesh->local_bounds = {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}, true};
    mesh->name = "HandleTest";
    auto material = std::make_shared<CEngine::Renderer::Material>();
    CEngine::Renderer::MeshInstance mesh_instance;
    mesh_instance.mesh = mesh;
    mesh_instance.material = material;
    const auto invalid_mesh_instance = renderer.RegisterMeshInstance(CEngine::Renderer::MeshInstance{});
    const auto mesh_instance_handle = renderer.RegisterMeshInstance(mesh_instance);
    renderer.RemoveMeshInstance(mesh_instance_handle);
    const auto current_mesh_instance = renderer.RegisterMeshInstance(mesh_instance);
    const std::uint64_t stable_mesh_instance_revision = renderer.GetMeshInstanceRevision();
    renderer.UpdateMeshInstance(current_mesh_instance, glm::mat4(1.0f), mesh_instance.flags);
    const bool unchanged_mesh_instance_is_free = renderer.GetMeshInstanceRevision() == stable_mesh_instance_revision;
    renderer.UpdateMeshInstance(current_mesh_instance, glm::translate(glm::mat4(1.0f), glm::vec3(1.0f)),
                                mesh_instance.flags);
    const bool changed_mesh_instance_is_published =
        renderer.GetMeshInstanceRevision() == stable_mesh_instance_revision + 1;
    const std::weak_ptr<const CEngine::Renderer::Mesh> retained_mesh = mesh;
    const std::weak_ptr<const CEngine::Renderer::Material> retained_material = material;
    mesh_instance.mesh.reset();
    mesh_instance.material.reset();
    mesh.reset();
    material.reset();

    CEngine::Renderer::Light light;
    const auto light_handle = renderer.RegisterLight(light);
    renderer.RemoveLight(light_handle);
    const auto current_light = renderer.RegisterLight(light);
    const std::uint64_t stable_light_revision = renderer.GetLightStateRevision();
    renderer.UpdateLight(current_light, light);
    const bool unchanged_light_is_free = renderer.GetLightStateRevision() == stable_light_revision;
    light.intensity = 2.0f;
    renderer.UpdateLight(current_light, light);
    const bool changed_light_is_published = renderer.GetLightStateRevision() == stable_light_revision + 1;
    CEngine::Renderer::RenderSystem other_renderer;

    const bool result =
        Expect(!invalid_mesh_instance, "renderer should reject a mesh instance without resources") &&
        Expect(mesh_instance_handle.index == current_mesh_instance.index,
               "renderer should reuse a freed mesh-instance slot") &&
        Expect(mesh_instance_handle.generation != current_mesh_instance.generation,
               "reused mesh-instance slot should advance generation") &&
        Expect(renderer.ResolveMeshInstance(mesh_instance_handle) == nullptr,
               "stale mesh-instance handle should not resolve") &&
        Expect(renderer.ResolveMeshInstance(current_mesh_instance) != nullptr,
               "current mesh-instance handle should resolve") &&
        Expect(!retained_mesh.expired() && !retained_material.expired() &&
                   renderer.ResolveMeshInstance(current_mesh_instance)->mesh != nullptr &&
                   renderer.ResolveMeshInstance(current_mesh_instance)->mesh->local_bounds.valid,
               "retained mesh instance should own its immutable resources") &&
        Expect(unchanged_mesh_instance_is_free, "an unchanged mesh instance update should not dirty renderer state") &&
        Expect(changed_mesh_instance_is_published, "a changed mesh instance should advance renderer state once") &&
        Expect(light_handle.index == current_light.index, "renderer should reuse a freed light slot") &&
        Expect(light_handle.generation != current_light.generation, "reused light slot should advance generation") &&
        Expect(renderer.ResolveLight(light_handle) == nullptr, "stale light handle should not resolve") &&
        Expect(renderer.ResolveLight(current_light) != nullptr, "current light handle should resolve") &&
        Expect(unchanged_light_is_free, "an unchanged light update should not dirty renderer state") &&
        Expect(changed_light_is_published, "a changed light should advance renderer state once") &&
        Expect(other_renderer.GetMeshInstances().empty() && other_renderer.GetDirectLights().empty(),
               "renderer instances should own independent state");

    CEngine::Renderer::Camera camera;
    renderer.UpdateCamera(camera);
    renderer.SetCameraAspectRatio(16.0f / 9.0f);
    const auto &frame = renderer.GetFrameConstants();
    const bool camera_result = Expect(std::abs((frame.proj[1][1] / frame.proj[0][0]) - (16.0f / 9.0f)) < 0.0001f,
                                      "camera projection should preserve the framebuffer aspect ratio");

    PhysicsSystem physics;
    PhysicsSystemDesc physics_desc;
    physics_desc.gravity = glm::vec3(0.0f);
    physics.Initialize(physics_desc);
    PhysicsShape dynamic_shape;
    dynamic_shape.half_extents = {1.0f, 2.0f, 3.0f};
    PhysicsBodyDesc dynamic_body;
    dynamic_body.motion_type = PhysicsMotionType::Dynamic;
    dynamic_body.mass = 8.0f;
    const PhysicsBodyHandle dynamic_handle = physics.CreateBody(dynamic_body, dynamic_shape);
    PhysicsShape floor_shape;
    floor_shape.half_extents = {10.0f, 10.0f, 0.5f};
    PhysicsBodyDesc floor_body;
    floor_body.position = {0.0f, 0.0f, -10.0f};
    const PhysicsBodyHandle floor_handle = physics.CreateBody(floor_body, floor_shape);
    physics.Step(1.0f / 60.0f);
    PhysicsBodyState dynamic_state;
    const bool physics_result =
        Expect(physics.BodyCount() == 2, "standard body descriptions should reach Jolt") &&
        Expect(physics.GetBodyState(dynamic_handle, dynamic_state) && glm::length(dynamic_state.position) < 0.0001f,
               "zero-gravity dynamic bodies should remain stable");
    physics.DestroyBody(dynamic_handle);
    physics.DestroyBody(floor_handle);
    const bool cleanup_result = Expect(physics.BodyCount() == 0, "body destruction should release every Jolt body");
#ifdef CENGINE_ENABLE_JOLT_PHYSICS
    const bool generation_result = JoltRejectsStaleBodyHandles();
    const bool query_result = JoltQueriesAndContactsWork();
#else
    const bool generation_result = true;
    const bool query_result = true;
#endif
    return result && camera_result && physics_result && cleanup_result && generation_result && query_result ? 0 : 1;
}
