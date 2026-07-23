#include "entity/prop_entity.h"

#include "assets/asset_database.h"
#include "engine_context.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace CEngine::Entities {

std::string_view PropEntity::Classname() const { return "prop"; }

void PropEntity::Initialize(EngineContext& context)
{
    try
    {
        if (context.rendering != nullptr)
        {
            if (context.assets == nullptr || context.scene == nullptr)
                throw std::runtime_error("prop rendering requires scene assets");
            const Assets::AssetHandle mesh_asset =
                context.scene->AssetReference(mesh.index);
            if (!mesh_asset ||
                context.assets->Type(mesh_asset) != Assets::AssetType::Mesh)
                throw std::runtime_error("prop mesh reference is invalid");

            if (materials.count != 1)
                throw std::runtime_error(
                    "prop requires exactly one material in this phase");
            const Assets::AssetHandle material_asset =
                context.scene->AuxiliaryAsset(materials.first);
            if (!material_asset ||
                context.assets->Type(material_asset) !=
                    Assets::AssetType::Material)
                throw std::runtime_error(
                    "prop material reference is invalid");

            Assets::AssetHandle lightmap_asset;
            if (lightmap)
            {
                lightmap_asset =
                    context.scene->AssetReference(lightmap.index);
                if (!lightmap_asset ||
                    context.assets->Type(lightmap_asset) !=
                        Assets::AssetType::Texture)
                    throw std::runtime_error(
                        "prop lightmap reference is invalid");
            }

            if (!context.assets->Load(material_asset, renderer_material_))
                throw std::runtime_error("could not load prop material");
            if (!context.rendering->RegisterMaterial(&renderer_material_))
                throw std::runtime_error("renderer rejected prop material");
            material_registered_ = true;

            if (lightmap_asset)
            {
                renderer_lightmap_ =
                    context.assets->LoadTexture(lightmap_asset, false);
                if (!renderer_lightmap_)
                    throw std::runtime_error("could not load prop lightmap");
                if (!context.rendering->RegisterLightmap(renderer_lightmap_.get()))
                    throw std::runtime_error("renderer rejected prop lightmap");
                lightmap_registered_ = true;
            }

            if (!context.assets->Load(mesh_asset,
                    {&renderer_material_}, renderer_mesh_))
                throw std::runtime_error("could not load prop mesh");
            RegisterRenderable(context);
        }

        if (context.physics != nullptr && collision_enabled)
        {
            PhysicsBodyDesc desc;
            desc.motion_type = dynamic ? PhysicsMotionType::Dynamic : PhysicsMotionType::Static;
            desc.shape_type = PhysicsShapeType::Box;
            desc.position = GetTransform().position;
            desc.rotation = GetTransform().rotation;
            desc.box_half_extents = glm::abs(GetTransform().scale) *
                glm::vec3(collision_half_extents.x, collision_half_extents.y,
                    collision_half_extents.z);
            desc.mass = mass;
            physics_body_ = context.physics->CreateBody(desc);
            if (physics_body_ == kInvalidPhysicsBodyHandle)
                throw std::runtime_error("physics rejected prop body");
        }
    }
    catch (...)
    {
        Shutdown(context);
        throw;
    }
}

std::uint32_t PropEntity::RenderableFlags() const
{
    std::uint32_t flags =
        Renderer::RenderableFlagCastsShadow |
        Renderer::RenderableFlagReceivesShadow |
        (dynamic ? Renderer::RenderableFlagDynamic :
            Renderer::RenderableFlagStatic);
    if (Enabled() && visible) flags |= Renderer::RenderableFlagVisible;
    if (shadow_only)
    {
        flags &= ~Renderer::RenderableFlagReceivesShadow;
        flags |= Renderer::RenderableFlagShadowOnly;
    }
    return flags;
}

void PropEntity::RegisterRenderable(EngineContext& context)
{
    Renderer::Renderable renderable;
    renderable.mesh = &renderer_mesh_;
    renderable.material = &renderer_material_;
    renderable.transform = GetTransform().world_matrix;
    renderable.local_bounds = renderer_mesh_.GetLocalBounds();
    renderable.flags = RenderableFlags();
    if (renderer_lightmap_ && !shadow_only)
    {
        for (const auto& batch : renderer_mesh_.GetMaterialMeshData())
            if (!batch.second.has_lightmap_uv)
                throw std::runtime_error("lightmapped mesh has no UV1");
        renderable.lightmap = renderer_lightmap_.get();
        renderable.lightmap_scale =
            {lightmap_scale.x, lightmap_scale.y};
        renderable.lightmap_offset =
            {lightmap_offset.x, lightmap_offset.y};
        renderable.lightmap_rgbm_range = lightmap_rgbm_range;
    }
    renderer_renderable_ = context.rendering->RegisterRenderable(renderable);
    if (!renderer_renderable_)
        throw std::runtime_error("renderer rejected prop entity");
}

void PropEntity::LateUpdate(EngineContext& context, float)
{
    if (context.physics != nullptr && dynamic && physics_body_ != kInvalidPhysicsBodyHandle)
    {
        const glm::mat4 world = context.physics->GetBodyTransform(physics_body_);
        GetTransform().position = glm::vec3(world[3]);
        GetTransform().rotation = glm::quat_cast(world);
        GetTransform().world_matrix =
            world * glm::scale(glm::mat4(1.0f), GetTransform().scale);
        GetTransform().dirty = false;
    }

    if (context.rendering == nullptr || !renderer_renderable_) return;
    context.rendering->UpdateRenderable(
        renderer_renderable_, GetTransform().world_matrix,
        RenderableFlags());
}

void PropEntity::Shutdown(EngineContext& context)
{
    if (context.physics != nullptr && physics_body_ != kInvalidPhysicsBodyHandle)
        context.physics->DestroyBody(physics_body_);
    physics_body_ = kInvalidPhysicsBodyHandle;
    if (context.rendering != nullptr)
    {
        if (renderer_renderable_)
            context.rendering->RemoveRenderable(renderer_renderable_);
        if (lightmap_registered_)
            context.rendering->RemoveLightmap(renderer_lightmap_.get());
        if (material_registered_)
            context.rendering->RemoveMaterial(&renderer_material_);
    }
    renderer_renderable_ = {};
    renderer_lightmap_.reset();
    lightmap_registered_ = false;
    material_registered_ = false;
}

} // namespace CEngine::Entities
