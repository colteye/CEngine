//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/asset_store.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_ASSET_STORE_H
#define CENGINE_ASSETS_ASSET_STORE_H

#include "assets/asset_format.h"

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

// A validated, project-relative reference from cooked content. It is a value,
// not a runtime resource handle: immutable assets are shared by std::shared_ptr
// and do not need a second generation/refcount lifetime system.
/**
 * @brief TODO: Describe AssetReference.
 */
struct AssetReference
{
    std::string path;
    Guid guid = {};
    AssetType type = AssetType::Unknown;
};

/**
 * @brief TODO: Describe AssetStore.
 */
class AssetStore
{
  public:
    /**
     * @brief TODO: Describe AssetStore.
     *
     * @param project_root TODO: Describe this parameter.
     */
    explicit AssetStore(std::filesystem::path project_root);

    /**
     * @brief TODO: Describe Resolve.
     *
     * @param project_relative_path TODO: Describe this parameter.
     * @param expected_type TODO: Describe this parameter.
     * @param expected_guid TODO: Describe this parameter.
     * @param reference TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Resolve(std::string_view project_relative_path, AssetType expected_type, const Guid &expected_guid,
                 AssetReference &reference) const;
    /**
     * @brief TODO: Describe LoadMaterial.
     *
     * @param reference TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::shared_ptr<const Renderer::Material> LoadMaterial(const AssetReference &reference) const;
    /**
     * @brief TODO: Describe LoadMesh.
     *
     * @param reference TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::shared_ptr<const Renderer::Mesh> LoadMesh(const AssetReference &reference) const;
    /**
     * @brief TODO: Describe LoadTexture.
     *
     * @param reference TODO: Describe this parameter.
     * @param flip_vertical TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::shared_ptr<const Renderer::Texture> LoadTexture(const AssetReference &reference,
                                                         bool flip_vertical = true) const;
    /**
     * @brief TODO: Describe LoadPhysics.
     *
     * @param reference TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    std::shared_ptr<const PhysicsShape> LoadPhysics(const AssetReference &reference) const;

  private:
    /**
     * @brief TODO: Describe FullPath.
     *
     * @param reference TODO: Describe this parameter.
     * @param expected_type TODO: Describe this parameter.
     * @param path TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool FullPath(const AssetReference &reference, AssetType expected_type, std::filesystem::path &path) const;

    std::filesystem::path project_root_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Material>>> materials_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Mesh>>> meshes_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Texture>>> textures_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Texture>>>
        flipped_textures_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const PhysicsShape>>> physics_shapes_;
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
