#ifndef CENGINE_ASSETS_ASSET_DATABASE_H
#define CENGINE_ASSETS_ASSET_DATABASE_H

#include "assets/asset_format.h"
#include "assets/asset_handle.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Assets {

class AssetDatabase {
public:
    explicit AssetDatabase(std::filesystem::path project_root);

    AssetHandle Acquire(std::string_view project_relative_path, AssetType expected_type,
        const Guid& expected_guid, std::string* error = nullptr);
    bool Release(AssetHandle handle);
    bool IsValid(AssetHandle handle) const;
    AssetType Type(AssetHandle handle) const;
    std::string_view Path(AssetHandle handle) const;
    std::filesystem::path FullPath(AssetHandle handle) const;
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
    };

    bool ValidateTarget(const std::filesystem::path& full_path, std::string_view relative_path,
        AssetType expected_type, const Guid& expected_guid, std::string* error) const;

    std::filesystem::path project_root_;
    std::vector<Slot> slots_;
    std::vector<std::uint32_t> free_indices_;
    std::size_t live_count_ = 0;
};

bool NormalizeProjectAssetPath(std::string_view input, std::string& output);

} // namespace CEngine::Assets

#endif
