#ifndef CENGINE_SAMPLES_VIEWER_GAME_PHYSICS_BALL_ASSETS_H
#define CENGINE_SAMPLES_VIEWER_GAME_PHYSICS_BALL_ASSETS_H

#include "assets/particle_asset.h"

#include <memory>

namespace CEngine::Renderer
{
struct Material;
struct Mesh;
} // namespace CEngine::Renderer

namespace Viewer
{

/**
 * Immutable, sample-local visual assets shared by every launcher and ball.
 *
 * These meshes are generated once because the viewer has no source-art
 * dependency for simple primitives. Runtime instances retain them through the
 * renderer's normal shared ownership boundary.
 */
struct PhysicsBallAssets
{
    std::shared_ptr<const CEngine::Renderer::Mesh> ball;
    std::shared_ptr<const CEngine::Renderer::Mesh> ball_markings;
    std::shared_ptr<const CEngine::Renderer::Mesh> launcher;
    std::shared_ptr<const CEngine::Renderer::Mesh> launcher_accent;
    std::shared_ptr<const CEngine::Renderer::Material> ball_material;
    std::shared_ptr<const CEngine::Renderer::Material> marking_material;
    std::shared_ptr<const CEngine::Renderer::Material> launcher_material;
    std::shared_ptr<const CEngine::Renderer::Material> accent_material;
    std::shared_ptr<const CEngine::Assets::Particle> muzzle_particles;
};

[[nodiscard]] const PhysicsBallAssets &GetPhysicsBallAssets();

} // namespace Viewer

#endif
