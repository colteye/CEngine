#ifndef CENGINE_SAMPLES_VIEWER_GAME_PHYSICS_BALL_WEAPON_H
#define CENGINE_SAMPLES_VIEWER_GAME_PHYSICS_BALL_WEAPON_H

#include "physics/physics_types.h"
#include "renderer/mesh_instance.h"
#include "renderer/particle_emitter.h"

#include <cstddef>
#include <vector>

#include <glm/vec3.hpp>

namespace CEngine
{
struct Context;
}

namespace Viewer
{

struct PhysicsBallWeaponPose
{
    glm::vec3 eye_position = glm::vec3(0.0f);
    glm::vec3 aim_direction = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 inherited_velocity = glm::vec3(0.0f);
    bool first_person = true;
};

/**
 * Concrete sample weapon with Source-style fire cadence and Unreal-style
 * separation between the weapon, projectile collision, and projectile visual.
 */
class PhysicsBallWeapon
{
  public:
    void Initialize(CEngine::Context &context);
    void Update(CEngine::Context &context, float delta_seconds, bool wants_to_fire, const PhysicsBallWeaponPose &pose);
    void LateUpdate(CEngine::Context &context);
    void Shutdown(CEngine::Context &context);

    [[nodiscard]] std::size_t ProjectileCount() const
    {
        return projectiles_.size();
    }

  private:
    struct Projectile
    {
        PhysicsBodyHandle body;
        CEngine::Renderer::MeshInstanceHandle ball;
        CEngine::Renderer::MeshInstanceHandle markings;
        float age_seconds = 0.0f;
    };

    bool Fire(CEngine::Context &context, const PhysicsBallWeaponPose &pose);
    void DestroyProjectile(CEngine::Context &context, std::size_t index);
    void UpdateViewModel(CEngine::Context &context, const PhysicsBallWeaponPose &pose);

    std::vector<Projectile> projectiles_;
    CEngine::Renderer::MeshInstanceHandle launcher_;
    CEngine::Renderer::MeshInstanceHandle launcher_accent_;
    CEngine::Renderer::ParticleEmitterHandle muzzle_particles_;
    float next_primary_attack_seconds_ = 0.0f;
};

} // namespace Viewer

#endif
