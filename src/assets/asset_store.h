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
struct AssetReference
{
    std::string path;
    Guid guid = {};
    AssetType type = AssetType::Unknown;
};

class AssetStore
{
  public:
    explicit AssetStore(std::filesystem::path project_root);

    bool Resolve(std::string_view project_relative_path, AssetType expected_type, const Guid &expected_guid,
                 AssetReference &reference) const;
    std::shared_ptr<const Renderer::Material> LoadMaterial(const AssetReference &reference) const;
    std::shared_ptr<const Renderer::Mesh> LoadMesh(const AssetReference &reference) const;
    std::shared_ptr<const Renderer::Texture> LoadTexture(const AssetReference &reference,
                                                         bool flip_vertical = true) const;
    std::shared_ptr<const PhysicsShape> LoadPhysics(const AssetReference &reference) const;

  private:
    bool FullPath(const AssetReference &reference, AssetType expected_type, std::filesystem::path &path) const;

    std::filesystem::path project_root_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Material>>> materials_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Mesh>>> meshes_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Texture>>> textures_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const Renderer::Texture>>>
        flipped_textures_;
    mutable std::unordered_map<std::string, std::pair<Guid, std::shared_ptr<const PhysicsShape>>> physics_shapes_;
};

bool NormalizeProjectAssetPath(std::string_view input, std::string &output);

} // namespace CEngine::Assets

#endif
