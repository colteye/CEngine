#include "game/physics_ball_weapon.h"

#include "context.h"
#include "game/physics_ball_assets.h"
#include "physics/physics_system.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Viewer
{
namespace
{

constexpr float BallRadius = 0.18f;
constexpr float BallMass = 2.0f;
constexpr float LaunchSpeed = 18.0f;
constexpr float PrimaryAttackInterval = 0.18f;
constexpr float ProjectileLifetime = 20.0f;
constexpr std::size_t MaxProjectiles = 64;
constexpr std::uint32_t MuzzleParticleCount = 12;
constexpr float MuzzleOffset = 0.60f;

glm::mat4 BallTransform(const PhysicsBodyState &state)
{
    return glm::translate(glm::mat4(1.0f), state.position) * glm::mat4_cast(state.rotation) *
           glm::scale(glm::mat4(1.0f), glm::vec3(BallRadius));
}

glm::mat4 PoseTransform(const glm::vec3 &position, const glm::vec3 &forward, const glm::vec3 &right,
                        const glm::vec3 &up)
{
    glm::mat4 result(1.0f);
    result[0] = glm::vec4(forward, 0.0f);
    // Mesh local +Y is left. Negating the world-space right vector keeps the
    // view-model basis right-handed, so culling sees the authored winding.
    result[1] = glm::vec4(-right, 0.0f);
    result[2] = glm::vec4(up, 0.0f);
    result[3] = glm::vec4(position, 1.0f);
    return result;
}

glm::mat4 ViewModelTransform(const PhysicsBallWeaponPose &pose)
{
    const glm::vec3 position = pose.eye_position + pose.aim_direction * 0.36f + pose.right * 0.19f - pose.up * 0.18f;
    return PoseTransform(position, pose.aim_direction, pose.right, pose.up);
}

glm::mat4 MuzzleTransform(const PhysicsBallWeaponPose &pose)
{
    return ViewModelTransform(pose) * glm::translate(glm::mat4(1.0f), glm::vec3(MuzzleOffset, 0.0f, 0.0f));
}

} // namespace

void PhysicsBallWeapon::Initialize(CEngine::Context &context)
{
    projectiles_.reserve(MaxProjectiles);
    if (context.rendering == nullptr)
    {
        return;
    }

    const PhysicsBallAssets &assets = GetPhysicsBallAssets();
    CEngine::Renderer::MeshInstance instance;
    instance.mesh = assets.launcher;
    instance.material = assets.launcher_material;
    instance.flags =
        CEngine::Renderer::MeshInstanceFlagVisible | CEngine::Renderer::MeshInstanceFlagCastsShadow;
    launcher_ = context.rendering->RegisterMeshInstance(instance);

    instance.mesh = assets.launcher_accent;
    instance.material = assets.accent_material;
    launcher_accent_ = context.rendering->RegisterMeshInstance(instance);

    CEngine::Renderer::ParticleEmitter emitter;
    emitter.particle = assets.muzzle_particles;
    emitter.capacity = 48;
    emitter.emitting = false;
    muzzle_particles_ = context.rendering->RegisterParticleEmitter(emitter);
}

void PhysicsBallWeapon::Update(CEngine::Context &context, float delta_seconds, bool wants_to_fire,
                               const PhysicsBallWeaponPose &pose)
{
    next_primary_attack_seconds_ = std::max(next_primary_attack_seconds_ - std::max(delta_seconds, 0.0f), 0.0f);

    for (std::size_t index = projectiles_.size(); index > 0; --index)
    {
        Projectile &projectile = projectiles_[index - 1u];
        projectile.age_seconds += std::max(delta_seconds, 0.0f);
        if (projectile.age_seconds >= ProjectileLifetime || context.physics == nullptr ||
            !context.physics->IsValid(projectile.body))
        {
            DestroyProjectile(context, index - 1u);
        }
    }

    UpdateViewModel(context, pose);
    if (wants_to_fire && next_primary_attack_seconds_ <= 0.0f && Fire(context, pose))
    {
        next_primary_attack_seconds_ = PrimaryAttackInterval;
    }
}

void PhysicsBallWeapon::LateUpdate(CEngine::Context &context)
{
    if (context.physics == nullptr || context.rendering == nullptr)
    {
        return;
    }
    for (const Projectile &projectile : projectiles_)
    {
        PhysicsBodyState state;
        if (!context.physics->GetBodyState(projectile.body, state))
        {
            continue;
        }
        const glm::mat4 transform = BallTransform(state);
        context.rendering->UpdateMeshInstance(projectile.ball, transform,
                                              CEngine::Renderer::MeshInstanceFlagCastsShadow |
                                                  CEngine::Renderer::MeshInstanceFlagVisible);
        context.rendering->UpdateMeshInstance(projectile.markings, transform,
                                              CEngine::Renderer::MeshInstanceFlagCastsShadow |
                                                  CEngine::Renderer::MeshInstanceFlagVisible);
    }
}

void PhysicsBallWeapon::Shutdown(CEngine::Context &context)
{
    while (!projectiles_.empty())
    {
        DestroyProjectile(context, projectiles_.size() - 1u);
    }
    if (context.rendering != nullptr)
    {
        context.rendering->RemoveMeshInstance(launcher_);
        context.rendering->RemoveMeshInstance(launcher_accent_);
        context.rendering->RemoveParticleEmitter(muzzle_particles_);
    }
    launcher_ = {};
    launcher_accent_ = {};
    muzzle_particles_ = {};
    next_primary_attack_seconds_ = 0.0f;
}

bool PhysicsBallWeapon::Fire(CEngine::Context &context, const PhysicsBallWeaponPose &pose)
{
    if (context.physics == nullptr)
    {
        return false;
    }
    if (projectiles_.size() == MaxProjectiles)
    {
        DestroyProjectile(context, 0);
    }

    const glm::vec3 aim =
        glm::length(pose.aim_direction) > 0.00001f ? glm::normalize(pose.aim_direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 spawn_position = glm::vec3(MuzzleTransform(pose)[3]);

    PhysicsShape shape;
    shape.type = PhysicsShapeType::Sphere;
    shape.radius = BallRadius;

    PhysicsBodyDesc desc;
    desc.motion_type = PhysicsMotionType::Dynamic;
    desc.position = spawn_position;
    desc.mass = BallMass;
    desc.friction = 0.88f;
    desc.restitution = 0.28f;
    desc.linear_velocity = aim * LaunchSpeed;
    desc.linear_damping = 0.035f;
    desc.angular_damping = 0.025f;
    desc.continuous = true;
    const PhysicsBodyHandle body = context.physics->CreateBody(desc, shape);
    if (!body)
    {
        return false;
    }

    Projectile projectile;
    projectile.body = body;
    if (context.rendering != nullptr)
    {
        const PhysicsBallAssets &assets = GetPhysicsBallAssets();
        CEngine::Renderer::MeshInstance instance;
        instance.mesh = assets.ball;
        instance.material = assets.ball_material;
        instance.transform =
            glm::translate(glm::mat4(1.0f), spawn_position) * glm::scale(glm::mat4(1.0f), glm::vec3(BallRadius));
        projectile.ball = context.rendering->RegisterMeshInstance(instance);

        instance.mesh = assets.ball_markings;
        instance.material = assets.marking_material;
        projectile.markings = context.rendering->RegisterMeshInstance(instance);
        if (!projectile.ball || !projectile.markings)
        {
            context.rendering->RemoveMeshInstance(projectile.ball);
            context.rendering->RemoveMeshInstance(projectile.markings);
            context.physics->DestroyBody(body);
            return false;
        }
    }
    projectiles_.push_back(projectile);
    if (context.rendering != nullptr && pose.first_person)
    {
        context.rendering->EmitParticles(muzzle_particles_, MuzzleParticleCount);
    }
    return true;
}

void PhysicsBallWeapon::DestroyProjectile(CEngine::Context &context, std::size_t index)
{
    if (index >= projectiles_.size())
    {
        return;
    }
    const Projectile projectile = projectiles_[index];
    if (context.physics != nullptr)
    {
        context.physics->DestroyBody(projectile.body);
    }
    if (context.rendering != nullptr)
    {
        context.rendering->RemoveMeshInstance(projectile.ball);
        context.rendering->RemoveMeshInstance(projectile.markings);
    }
    projectiles_.erase(projectiles_.begin() + static_cast<std::ptrdiff_t>(index));
}

void PhysicsBallWeapon::UpdateViewModel(CEngine::Context &context, const PhysicsBallWeaponPose &pose)
{
    if (context.rendering == nullptr)
    {
        return;
    }
    const std::uint32_t flags =
        pose.first_person
            ? CEngine::Renderer::MeshInstanceFlagVisible | CEngine::Renderer::MeshInstanceFlagCastsShadow
            : 0u;
    const glm::mat4 transform = ViewModelTransform(pose);
    context.rendering->UpdateMeshInstance(launcher_, transform, flags);
    context.rendering->UpdateMeshInstance(launcher_accent_, transform, flags);
    context.rendering->UpdateParticleEmitter(muzzle_particles_, MuzzleTransform(pose), false);
}

} // namespace Viewer
