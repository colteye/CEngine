//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/store.h
 * @brief Shared immutable asset cache and reference resolver.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_STORE_H
#define CENGINE_ASSETS_STORE_H

#include "assets/animation_asset.h"
#include "assets/audio_asset.h"
#include "assets/casset_asset.h"
#include "assets/format.h"
#include "assets/particle_asset.h"
#include "assets/skeleton_asset.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace CEngine::Renderer
{
struct Material;
struct Mesh;
struct Texture;
} // namespace CEngine::Renderer
struct PhysicsShape;

namespace CEngine::Assets
{

class Store
{
  public:
    explicit Store(std::filesystem::path project_root);

    bool Resolve(std::string_view project_relative_path, Type expected_type, const Guid &expected_guid,
                 Reference &reference) const;
    std::shared_ptr<const Renderer::Material> LoadMaterial(const Reference &reference) const;
    std::shared_ptr<const Renderer::Mesh> LoadMesh(const Reference &reference) const;
    std::shared_ptr<const Renderer::Texture> LoadTexture(const Reference &reference,
                                                         bool flip_vertical = true) const;
    std::shared_ptr<const PhysicsShape> LoadPhysics(const Reference &reference) const;
    std::shared_ptr<const Skeleton> LoadSkeleton(const Reference &reference) const;
    std::shared_ptr<const Animation> LoadAnimation(const Reference &reference) const;
    std::shared_ptr<const CAsset> LoadCAsset(const Reference &reference) const;
    std::shared_ptr<const Particle> LoadParticle(const Reference &reference) const;
    std::shared_ptr<const Audio> LoadAudio(const Reference &reference) const;

  private:
    template <typename T> using Cache = std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const T>>>;
    template <typename T, typename Load>
    std::shared_ptr<const T> Get(const Reference &reference, Type type, Cache<T> &cache, Load load) const;

    bool FullPath(const Reference &reference, Type expected_type, std::filesystem::path &path) const;

    std::filesystem::path project_root_;
    mutable Cache<Renderer::Material> materials_;
    mutable Cache<Renderer::Mesh> meshes_;
    mutable Cache<Renderer::Texture> textures_;
    mutable Cache<Renderer::Texture> flipped_textures_;
    mutable Cache<PhysicsShape> physics_;
    mutable Cache<Skeleton> skeletons_;
    mutable Cache<Animation> animations_;
    mutable Cache<CAsset> assets_;
    mutable Cache<Particle> particles_;
    mutable Cache<Audio> audio_;
};

/**
 * @brief TODO: Describe NormalizeProjectAssetPath.
 *
 * @param input TODO: Describe this parameter.
 * @param output TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool NormalizeProjectAssetPath(std::string_view input, std::string &output);

} // namespace CEngine::Assets

#endif
