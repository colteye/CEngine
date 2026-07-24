//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/prop_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/prop_entity.h"

#include "animation/animation_system.h"
#include "assets/store.h"
#include "context.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace CEngine::Entities
{

/**
 * @brief TODO: Describe PropEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view PropEntity::Classname() const
{
    return "prop";
}

bool PropEntity::AcceptsInput(std::string_view input) const
{
    return input == "Show" || input == "Hide" || Entity::AcceptsInput(input);
}

bool PropEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name == "Show" || input.name == "Hide")
    {
        visible = input.name == "Show";
        UpdatePresentation(context);
        return true;
    }
    return Entity::HandleInput(context, input);
}

namespace
{
/**
 * @brief TODO: Describe RuntimeMotion.
 *
 * @param motion TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
PhysicsMotionType RuntimeMotion(Generated::EngineEntities::PhysicsMotion motion)
{
    switch (motion)
    {
    case Generated::EngineEntities::PhysicsMotion::Static:
        return PhysicsMotionType::Static;
    case Generated::EngineEntities::PhysicsMotion::Dynamic:
        return PhysicsMotionType::Dynamic;
    case Generated::EngineEntities::PhysicsMotion::Kinematic:
        return PhysicsMotionType::Kinematic;
    case Generated::EngineEntities::PhysicsMotion::None:
        break;
    }
    return PhysicsMotionType::Static;
}

/**
 * @brief TODO: Describe Vector.
 *
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
glm::vec3 Vector(const Generated::EngineEntities::Vec3 &value)
{
    return {value.x, value.y, value.z};
}
} // namespace

/**
 * @brief TODO: Describe PropEntity::Initialize.
 *
 * @param context TODO: Describe this parameter.
 */
void PropEntity::Initialize(Context &context)
{
    try
    {
        if (context.rendering != nullptr)
        {
            if (context.assets == nullptr || context.scene == nullptr)
            {
                throw std::runtime_error("prop rendering requires scene assets");
            }
            const Assets::Reference *mesh_asset = context.scene->AssetReference(mesh.index);
            if (mesh_asset == nullptr || mesh_asset->type != Assets::Type::Mesh)
            {
                throw std::runtime_error("prop mesh reference is invalid");
            }

            if (materials.count != 1)
            {
                throw std::runtime_error("prop requires exactly one material in this phase");
            }
            const Assets::Reference *material_asset = context.scene->AuxiliaryAsset(materials.first);
            if (material_asset == nullptr || material_asset->type != Assets::Type::Material)
            {
                throw std::runtime_error("prop material reference is invalid");
            }

            const Assets::Reference *lightmap_asset = nullptr;
            if (lightmap)
            {
                lightmap_asset = context.scene->AssetReference(lightmap.index);
                if (lightmap_asset == nullptr || lightmap_asset->type != Assets::Type::Texture)
                {
                    throw std::runtime_error("prop lightmap reference is invalid");
                }
            }

            std::shared_ptr<const Renderer::Material> renderer_material = context.assets->LoadMaterial(*material_asset);
            if (!renderer_material)
            {
                throw std::runtime_error("could not load prop material");
            }

            std::shared_ptr<const Renderer::Texture> renderer_lightmap;
            if (lightmap_asset != nullptr)
            {
                renderer_lightmap = context.assets->LoadTexture(*lightmap_asset, false);
                if (!renderer_lightmap)
                {
                    throw std::runtime_error("could not load prop lightmap");
                }
            }

            std::shared_ptr<const Renderer::Mesh> renderer_mesh = context.assets->LoadMesh(*mesh_asset);
            if (!renderer_mesh)
            {
                throw std::runtime_error("could not load prop mesh");
            }

            const bool has_skeleton = static_cast<bool>(skeleton);
            const bool has_animation = static_cast<bool>(animation);
            if (renderer_mesh->skinned != has_skeleton || has_animation != has_skeleton)
            {
                throw std::runtime_error(
                    "skinned prop requires one skeleton and one animation; static prop requires neither");
            }
            if (has_skeleton)
            {
                if (context.animations == nullptr)
                {
                    throw std::runtime_error("skinned prop requires the animation system");
                }
                const Assets::Reference *skeleton_reference = context.scene->AssetReference(skeleton.index);
                const Assets::Reference *animation_reference = context.scene->AssetReference(animation.index);
                if (skeleton_reference == nullptr || skeleton_reference->type != Assets::Type::Skeleton)
                {
                    throw std::runtime_error("prop skeleton reference is invalid");
                }
                if (animation_reference == nullptr || animation_reference->type != Assets::Type::Animation)
                {
                    throw std::runtime_error("prop animation reference is invalid");
                }
                skeleton_asset_ = context.assets->LoadSkeleton(*skeleton_reference);
                animation_asset_ = context.assets->LoadAnimation(*animation_reference);
                if (!skeleton_asset_ || !animation_asset_)
                {
                    throw std::runtime_error("could not load prop animation assets");
                }
                if (renderer_mesh->skeleton_guid != skeleton_asset_->Identity() ||
                    animation_asset_->SkeletonReference().guid != skeleton_asset_->Identity())
                {
                    throw std::runtime_error("prop mesh, skeleton, and animation identities do not match");
                }
                animation_instance_ = context.animations->CreateInstance({skeleton_asset_});
                Animations::AnimationPlayback playback;
                playback.rate = animation_playback_rate;
                playback.looping = animation_looping;
                if (!animation_instance_ ||
                    !context.animations->Play(animation_instance_, animation_asset_, playback))
                {
                    throw std::runtime_error("animation system rejected prop animation");
                }
            }
            RegisterMeshInstance(context, std::move(renderer_mesh), std::move(renderer_material),
                                 std::move(renderer_lightmap));
        }

        if (context.physics != nullptr && motion != Generated::EngineEntities::PhysicsMotion::None)
        {
            if (context.assets == nullptr || context.scene == nullptr)
            {
                throw std::runtime_error("prop physics requires scene assets");
            }
            const Assets::Reference *collision_asset = context.scene->AssetReference(collision.index);
            if (collision_asset == nullptr || collision_asset->type != Assets::Type::Physics)
            {
                throw std::runtime_error("prop collision reference is invalid");
            }
            const std::shared_ptr<const PhysicsShape> shape = context.assets->LoadPhysics(*collision_asset);
            if (!shape)
            {
                throw std::runtime_error("could not load prop collision geometry");
            }

            PhysicsBodyDesc desc;
            desc.motion_type = RuntimeMotion(motion);
            desc.position = GetTransform().position;
            desc.rotation = GetTransform().rotation;
            desc.mass = mass;
            desc.friction = friction;
            desc.restitution = restitution;
            desc.linear_velocity = Vector(initial_linear_velocity);
            desc.angular_velocity = Vector(initial_angular_velocity);
            desc.linear_damping = linear_damping;
            desc.angular_damping = angular_damping;
            desc.gravity_factor = gravity_factor;
            desc.collision_layer = static_cast<std::uint8_t>(collision_layer);
            desc.sensor = sensor;
            desc.continuous = continuous;
            desc.allow_sleeping = allow_sleeping;
            desc.locked_axes =
                (lock_translation_x ? PhysicsLockTranslationX : 0u) |
                (lock_translation_y ? PhysicsLockTranslationY : 0u) |
                (lock_translation_z ? PhysicsLockTranslationZ : 0u) | (lock_rotation_x ? PhysicsLockRotationX : 0u) |
                (lock_rotation_y ? PhysicsLockRotationY : 0u) | (lock_rotation_z ? PhysicsLockRotationZ : 0u);
            physics_body_ = context.physics->CreateBody(desc, *shape);
            if (!physics_body_)
            {
                throw std::runtime_error("physics rejected prop body");
            }
            context.scene->RegisterPhysicsBody(physics_body_, GetHandle());
        }
    }
    catch (...)
    {
        Shutdown(context);
        throw;
    }
}

/**
 * @brief TODO: Describe PropEntity::BuildMeshInstanceFlags.
 *
 * @return TODO: Describe the return value.
 */
std::uint32_t PropEntity::BuildMeshInstanceFlags() const
{
    std::uint32_t flags = Renderer::MeshInstanceFlagCastsShadow;
    if (Enabled() && visible)
    {
        flags |= Renderer::MeshInstanceFlagVisible;
    }
    if (shadow_only)
    {
        flags |= Renderer::MeshInstanceFlagShadowOnly;
    }
    return flags;
}

/**
 * @brief TODO: Describe PropEntity::RegisterMeshInstance.
 *
 * @param context TODO: Describe this parameter.
 * @param mesh TODO: Describe this parameter.
 * @param material TODO: Describe this parameter.
 * @param lightmap_texture TODO: Describe this parameter.
 */
void PropEntity::RegisterMeshInstance(Context &context, std::shared_ptr<const Renderer::Mesh> mesh,
                                      std::shared_ptr<const Renderer::Material> material,
                                      std::shared_ptr<const Renderer::Texture> lightmap_texture)
{
    Renderer::MeshInstance mesh_instance;
    mesh_instance.mesh = std::move(mesh);
    mesh_instance.material = std::move(material);
    mesh_instance.transform = GetTransform().world_matrix;
    mesh_instance.flags = BuildMeshInstanceFlags();
    if (lightmap_texture && !shadow_only)
    {
        if (!mesh_instance.mesh->has_lightmap_uv)
        {
            throw std::runtime_error("lightmapped mesh has no UV1");
        }
        mesh_instance.lightmap = std::move(lightmap_texture);
        mesh_instance.lightmap_scale = {lightmap_scale.x, lightmap_scale.y};
        mesh_instance.lightmap_offset = {lightmap_offset.x, lightmap_offset.y};
        mesh_instance.lightmap_rgbm_range = lightmap_rgbm_range;
    }
    renderer_mesh_instance_ = context.rendering->RegisterMeshInstance(mesh_instance);
    if (!renderer_mesh_instance_)
    {
        throw std::runtime_error("renderer rejected prop entity");
    }
}

/**
 * @brief TODO: Describe PropEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 * @param delta_seconds TODO: Describe this parameter.
 */
void PropEntity::Update(Context &context, float delta_seconds)
{
    if (context.physics == nullptr || !physics_body_ || motion != Generated::EngineEntities::PhysicsMotion::Kinematic ||
        delta_seconds <= 0.0f)
    {
        return;
    }
    context.physics->MoveKinematic(physics_body_, GetTransform().position, GetTransform().rotation, delta_seconds);
}

/**
 * @brief TODO: Describe PropEntity::LateUpdate.
 *
 * @param context TODO: Describe this parameter.
 */
void PropEntity::LateUpdate(Context &context, float /*unused*/)
{
    if (context.physics != nullptr && motion == Generated::EngineEntities::PhysicsMotion::Dynamic && physics_body_)
    {
        PhysicsBodyState state;
        if (context.physics->GetBodyState(physics_body_, state))
        {
            GetTransform().position = state.position;
            GetTransform().rotation = state.rotation;
            const glm::mat4 world = glm::translate(glm::mat4(1.0f), state.position) * glm::mat4_cast(state.rotation);
            GetTransform().world_matrix = world * glm::scale(glm::mat4(1.0f), GetTransform().scale);
            GetTransform().dirty = false;
        }
    }

    UpdatePresentation(context);
}

void PropEntity::UpdatePresentation(Context &context)
{
    if (context.rendering != nullptr && renderer_mesh_instance_)
    {
        context.rendering->UpdateMeshInstance(renderer_mesh_instance_, GetTransform().world_matrix,
                                              BuildMeshInstanceFlags());
        if (animation_instance_ && context.animations != nullptr &&
            !context.rendering->UpdateMeshSkinning(
                renderer_mesh_instance_, context.animations->SkinningPalette(animation_instance_)))
        {
            throw std::runtime_error("renderer rejected prop skinning palette");
        }
    }
}

/**
 * @brief TODO: Describe PropEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void PropEntity::Shutdown(Context &context)
{
    if (context.animations != nullptr && animation_instance_)
    {
        context.animations->DestroyInstance(animation_instance_);
    }
    animation_instance_ = {};
    animation_asset_.reset();
    skeleton_asset_.reset();
    if (context.physics != nullptr && physics_body_)
    {
        if (context.scene != nullptr)
        {
            context.scene->UnregisterPhysicsBody(physics_body_);
        }
        context.physics->DestroyBody(physics_body_);
    }
    physics_body_ = {};
    if (context.rendering != nullptr)
    {
        if (renderer_mesh_instance_)
        {
            context.rendering->RemoveMeshInstance(renderer_mesh_instance_);
        }
    }
    renderer_mesh_instance_ = {};
}

} // namespace CEngine::Entities
