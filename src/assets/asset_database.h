#ifndef CENGINE_ASSETS_ASSET_DATABASE_H
#define CENGINE_ASSETS_ASSET_DATABASE_H

#include "assets/asset_format.h"
#include "assets/asset_handle.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Renderer {
struct Material;
class Mesh;
struct Texture;
}

namespace CEngine::Assets {

class AssetDatabase {
public:
    explicit AssetDatabase(std::filesystem::path project_root);

    AssetHandle Acquire(std::string_view project_relative_path, AssetType expected_type,
        const Guid& expected_guid);
    bool Release(AssetHandle handle);
    bool IsValid(AssetHandle handle) const;
    AssetType Type(AssetHandle handle) const;
    std::string_view Path(AssetHandle handle) const;
    std::filesystem::path FullPath(AssetHandle handle) const;
    bool Load(AssetHandle handle, Renderer::Material& material) const;
    bool Load(AssetHandle handle,
        const std::vector<Renderer::Material*>& materials,
        Renderer::Mesh& mesh) const;
    bool Load(AssetHandle handle, Renderer::Texture& texture,
        bool flip_vertical = true) const;
    std::shared_ptr<const Renderer::Texture> LoadTexture(
        AssetHandle handle, bool flip_vertical = true) const;
    void ReleaseAll();
    std::size_t Size() const { return live_count_; }

private:
    struct Slot {
        std::string path;
        Guid guid = {};
        AssetType type = AssetType::Unknown;
        std::uint32_t generation = 1;
        std::uint32_t references = 0;
        bool live = false;
        mutable std::array<std::shared_ptr<const Renderer::Texture>, 2>
            texture_cache;
    };

    bool ValidateTarget(const std::filesystem::path& full_path, std::string_view relative_path,
        AssetType expected_type, const Guid& expected_guid) const;

    std::filesystem::path project_root_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_indices_;
    std::size_t live_count_ = 0;
};

bool NormalizeProjectAssetPath(std::string_view input, std::string& output);

} // namespace CEngine::Assets

#endif
