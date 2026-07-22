#include "scene/scene_render_state.h"

#include "assets/asset_database.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "entity/light_entity.h"
#include "entity/exponential_height_fog_entity.h"
#include "entity/prop_entity.h"
#include "entity/skybox_entity.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <cmath>

namespace CEngine::Scene {
namespace {
bool Fail(std::string* error, std::string message)
{
    if (error != nullptr) *error = std::move(message);
    return false;
}

Renderer::LightRecord MakeLight(const Entities::LightEntity& entity)
{
    Renderer::LightRecord light;
    if (entity.type == Entities::LightType::Sun) light.type = Renderer::LightType::Directional;
    else if (entity.type == Entities::LightType::Spot) light.type = Renderer::LightType::Spot;
    else light.type = Renderer::LightType::Point;
    const Transform& transform = entity.GetTransform();
    light.position = transform.position;
    light.direction = glm::normalize(glm::vec3(transform.world_matrix * glm::vec4(0, 0, -1, 0)));
    light.color = entity.color;
    light.intensity = entity.intensity;
    light.range = entity.range;
    light.spot_inner_cos = std::cos(entity.inner_angle_radians);
    light.spot_outer_cos = std::cos(entity.outer_angle_radians);
    light.enabled = entity.enabled;
    light.casts_shadows = entity.casts_shadows;
    return light;
}
} // namespace

SceneRenderState::~SceneRenderState()
{
    Stop();
}

Renderer::Material* SceneRenderState::FindMaterial(Assets::AssetHandle asset)
{
    for (auto& loaded : materials_)
        if (loaded.asset == asset) return &loaded.material;
    return nullptr;
}

Renderer::Mesh* SceneRenderState::FindMesh(Assets::AssetHandle asset, Assets::AssetHandle material)
{
    for (auto& loaded : meshes_)
        if (loaded.asset == asset && loaded.material_asset == material) return &loaded.mesh;
    return nullptr;
}

Renderer::Lightmap* SceneRenderState::FindLightmap(Assets::AssetHandle asset)
{
    for (auto& loaded : lightmaps_)
        if (loaded.asset == asset) return &loaded.lightmap;
    return nullptr;
}

bool SceneRenderState::Activate(const Scene& scene, Assets::AssetDatabase& assets, std::string* error)
{
    Stop();
    Renderer::AmbientLighting ambient;
    ambient.sky_color = scene.Settings().ambient_color;
    ambient.ground_color = scene.Settings().ambient_color * 0.35f;
    ambient.intensity = scene.Settings().exposure;
    ambient.enabled = glm::any(glm::greaterThan(scene.Settings().ambient_color, glm::vec3(0.0f)));
    Renderer::RenderSystem::SetAmbientLighting(ambient);

    Renderer::ImageBasedLighting ibl;
    Renderer::ExponentialHeightFog fog_state;
    for (const auto& entity : scene.Entities())
    {
        if (entity == nullptr || !entity->Enabled()) continue;
        if (entity->Classname() == "skybox")
        {
            const auto& skybox = static_cast<const Entities::SkyboxEntity&>(*entity);
            if (!skybox.enabled) continue;
            ibl.panorama_path = assets.FullPath(skybox.panorama);
            ibl.intensity = skybox.intensity;
            ibl.rotation_radians = skybox.rotation_radians;
            ibl.enabled = true;
            ambient.enabled = false;
        }
        else if (entity->Classname() == "exponential_height_fog")
        {
            const auto& fog = static_cast<const Entities::ExponentialHeightFogEntity&>(*entity);
            if (!fog.enabled) continue;
            fog_state.inscattering_color = fog.inscattering_color;
            fog_state.density = fog.density;
            fog_state.height_falloff = fog.height_falloff;
            fog_state.base_height = fog.GetTransform().position.z;
            fog_state.start_distance = fog.start_distance;
            fog_state.max_opacity = fog.max_opacity;
            fog_state.cutoff_distance = fog.cutoff_distance;
            fog_state.enabled = true;
        }
    }
    Renderer::RenderSystem::SetAmbientLighting(ambient);
    Renderer::RenderSystem::SetImageBasedLighting(ibl);
    Renderer::RenderSystem::SetExponentialHeightFog(fog_state);

    materials_.reserve(scene.AssetReferenceCount());
    for (std::uint32_t index = 0; index < scene.AssetReferenceCount(); ++index)
    {
        if (scene.ReferencedAssetType(index) != Assets::AssetType::Material) continue;
        LoadedMaterial loaded;
        loaded.asset = scene.AssetReference(index);
        if (!Assets::LoadMaterialAsset(assets.FullPath(loaded.asset), loaded.material, error))
        { Stop(); return false; }
        materials_.push_back(std::move(loaded));
    }
    for (auto& loaded : materials_)
    {
        if (!Renderer::RenderSystem::RegisterMaterial(&loaded.material))
        {
            Stop();
            return Fail(error, "could not upload material textures: " + loaded.material.material_name);
        }
    }

    lightmaps_.reserve(scene.EntityCount());
    for (const auto& entity : scene.Entities())
    {
        if (entity == nullptr || entity->Classname() != "prop") continue;
        const Assets::AssetHandle asset = static_cast<const Entities::PropEntity&>(*entity).lightmap;
        if (!asset || FindLightmap(asset) != nullptr) continue;
        LoadedLightmap loaded;
        loaded.asset = asset;
        loaded.lightmap.path = assets.FullPath(loaded.asset);
        lightmaps_.push_back(std::move(loaded));
    }
    for (auto& loaded : lightmaps_)
    {
        if (!Renderer::RenderSystem::RegisterLightmap(&loaded.lightmap))
        {
            Stop();
            return Fail(error, "could not load lightmap DDS: " + loaded.lightmap.path.string());
        }
    }

    meshes_.reserve(scene.EntityCount());
    renderables_.reserve(scene.EntityCount());
    lights_.reserve(scene.EntityCount());
    for (const auto& entity_ptr : scene.Entities())
    {
        if (entity_ptr == nullptr || !entity_ptr->Enabled()) continue;
        Assets::AssetHandle mesh_asset;
        Assets::AssetHandle material_asset;
        bool visible = false;
        const Entities::PropEntity* prop = nullptr;
        std::uint32_t flags = Renderer::RenderableFlagCastsShadow | Renderer::RenderableFlagReceivesShadow;
        if (entity_ptr->Classname() == "prop")
        {
            prop = &static_cast<const Entities::PropEntity&>(*entity_ptr);
            mesh_asset = prop->mesh;
            material_asset = prop->material_count != 0 ? scene.Material(prop->first_material) : Assets::AssetHandle{};
            visible = prop->visible;
            flags |= prop->dynamic ? Renderer::RenderableFlagDynamic : Renderer::RenderableFlagStatic;
            if (prop->shadow_only)
            {
                flags &= ~Renderer::RenderableFlagReceivesShadow;
                flags |= Renderer::RenderableFlagShadowOnly;
            }
        }
        else if (entity_ptr->Classname() == "light")
        {
            const auto& light = static_cast<const Entities::LightEntity&>(*entity_ptr);
            if (Entities::HasRuntimeDirectLighting(light.mode))
                lights_.push_back(Renderer::RenderSystem::RegisterLight(MakeLight(light)));
            continue;
        }
        else continue;

        if (!visible) continue;
        Renderer::Material* material = FindMaterial(material_asset);
        if (material == nullptr)
        { Stop(); return Fail(error, "visible prop has no loadable material: " + entity_ptr->Name()); }
        Renderer::Mesh* mesh = FindMesh(mesh_asset, material_asset);
        if (mesh == nullptr)
        {
            LoadedMesh loaded;
            loaded.asset = mesh_asset;
            loaded.material_asset = material_asset;
            if (!Assets::LoadMeshAsset(assets.FullPath(mesh_asset), {material}, loaded.mesh, error))
            { Stop(); return false; }
            meshes_.push_back(std::move(loaded));
            mesh = &meshes_.back().mesh;
        }
        Renderer::Renderable renderable;
        renderable.mesh = mesh;
        renderable.material = material;
        renderable.transform = entity_ptr->GetTransform().world_matrix;
        renderable.local_bounds = mesh->GetLocalBounds();
        renderable.flags = flags;
        if (prop != nullptr && prop->lightmap && !prop->shadow_only)
        {
            renderable.lightmap = FindLightmap(prop->lightmap);
            if (renderable.lightmap == nullptr)
            { Stop(); return Fail(error, "static prop lightmap was not loaded: " + entity_ptr->Name()); }
            for (const auto& batch : mesh->GetMaterialMeshData())
            {
                if (!batch.second.has_lightmap_uv)
                { Stop(); return Fail(error, "lightmapped mesh has no UV1: " + entity_ptr->Name()); }
            }
            renderable.lightmap_scale = prop->lightmap_scale;
            renderable.lightmap_offset = prop->lightmap_offset;
            renderable.lightmap_rgbm_range = prop->lightmap_rgbm_range;
        }
        const Renderer::RenderableHandle handle = Renderer::RenderSystem::RegisterRenderable(renderable);
        if (!handle)
        { Stop(); return Fail(error, "renderer rejected prop: " + entity_ptr->Name()); }
        renderables_.push_back(handle);
        if ((flags & Renderer::RenderableFlagDynamic) != 0)
            dynamic_renderables_.push_back({entity_ptr->Id(), handle});
    }
    active_ = true;
    return true;
}

void SceneRenderState::UpdateDynamic(const Scene& scene)
{
    for (const DynamicRenderable& dynamic : dynamic_renderables_)
    {
        const Entity* entity = scene.GetEntity(dynamic.entity);
        if (entity != nullptr)
            Renderer::RenderSystem::UpdateRenderableTransform(dynamic.renderable,
                entity->GetTransform().world_matrix);
    }
}

void SceneRenderState::Stop()
{
    for (auto light = lights_.rbegin(); light != lights_.rend(); ++light)
        Renderer::RenderSystem::RemoveLight(*light);
    for (auto renderable = renderables_.rbegin(); renderable != renderables_.rend(); ++renderable)
        Renderer::RenderSystem::RemoveRenderable(*renderable);
    for (auto material = materials_.rbegin(); material != materials_.rend(); ++material)
        Renderer::RenderSystem::RemoveMaterial(&material->material);
    for (auto lightmap = lightmaps_.rbegin(); lightmap != lightmaps_.rend(); ++lightmap)
        Renderer::RenderSystem::RemoveLightmap(&lightmap->lightmap);
    lights_.clear();
    renderables_.clear();
    dynamic_renderables_.clear();
    meshes_.clear();
    lightmaps_.clear();
    materials_.clear();
    Renderer::RenderSystem::SetAmbientLighting(Renderer::AmbientLighting{});
    Renderer::RenderSystem::SetImageBasedLighting(Renderer::ImageBasedLighting{});
    Renderer::RenderSystem::SetExponentialHeightFog(Renderer::ExponentialHeightFog{});
    active_ = false;
}

} // namespace CEngine::Scene
