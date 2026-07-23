#include "assets/asset_database.h"

#include "assets/asset_error.h"
#include "assets/asset_io.h"

#include <algorithm>

namespace CEngine::Assets {
namespace {
bool IsExternalPayload(AssetType type)
{
    return type == AssetType::Texture || type == AssetType::Audio;
}
} // namespace

bool NormalizeProjectAssetPath(std::string_view input, std::string& output)
{
    if (input.empty()) return false;
    output.assign(input);
    std::replace(output.begin(), output.end(), '\\', '/');
    const std::filesystem::path path(output);
    if (path.is_absolute()) return false;
    output = path.lexically_normal().generic_string();
    return !output.empty() && output != "." && output != ".." &&
        output.compare(0, 3, "../") != 0;
}

AssetDatabase::AssetDatabase(std::filesystem::path project_root)
    : project_root_(std::move(project_root))
{
}

AssetHandle AssetDatabase::Acquire(std::string_view project_relative_path,
    AssetType expected_type, const Guid& expected_guid)
{
    std::string normalized;
    if (!NormalizeProjectAssetPath(project_relative_path, normalized))
    {
        AssetError("asset path must be project-relative");
        return {};
    }
    if (expected_type == AssetType::Unknown || expected_type == AssetType::Scene)
    {
        AssetError("asset reference type is invalid");
        return {};
    }

    for (std::uint32_t index = 0; index < slots_.size(); ++index)
    {
        const Slot& slot = slots_[index];
        if (!slot.live || slot.path != normalized) continue;
        if (slot.type != expected_type || slot.guid != expected_guid)
        {
            AssetError("asset reference conflicts with an existing path");
            return {};
        }
        ++slots_[index].references;
        return {index, slot.generation};
    }

    if (!ValidateTarget(
            project_root_ / normalized, normalized, expected_type, expected_guid))
        return {};

    std::uint32_t index;
    if (free_indices_.empty()) { index = static_cast<std::uint32_t>(slots_.size()); slots_.emplace_back(); }
    else { index = free_indices_.back(); free_indices_.pop_back(); }
    Slot& slot = slots_[index];
    slot.path = std::move(normalized);
    slot.guid = expected_guid;
    slot.type = expected_type;
    slot.references = 1;
    slot.live = true;
    ++live_count_;
    return {index, slot.generation};
}

bool AssetDatabase::Release(AssetHandle handle)
{
    if (!IsValid(handle)) return false;
    Slot& slot = slots_[handle.index];
    if (--slot.references != 0) return true;
    slot.live = false;
    slot.path.clear();
    slot.guid = {};
    slot.type = AssetType::Unknown;
    slot.texture_cache = {};
    ++slot.generation;
    if (slot.generation == 0) ++slot.generation;
    free_indices_.push_back(handle.index);
    --live_count_;
    return true;
}

bool AssetDatabase::ValidateTarget(const std::filesystem::path& full_path,
    std::string_view relative_path, AssetType expected_type,
    const Guid& expected_guid) const
{
    if (!std::filesystem::is_regular_file(full_path))
        return AssetError(
            "referenced asset does not exist: " + std::string(relative_path));
    if (IsExternalPayload(expected_type)) return true;

    AssetFile file;
    if (!file.Load(full_path)) return false;
    if (file.Type() != expected_type)
        return AssetError(
            "referenced asset type does not match: " +
            std::string(relative_path));
    if (file.GetGuid() != expected_guid)
        return AssetError(
            "referenced asset GUID does not match: " +
            std::string(relative_path));
    return true;
}

bool AssetDatabase::IsValid(AssetHandle handle) const
{
    return handle.index < slots_.size() && slots_[handle.index].live &&
        slots_[handle.index].generation == handle.generation;
}

AssetType AssetDatabase::Type(AssetHandle handle) const
{ return IsValid(handle) ? slots_[handle.index].type : AssetType::Unknown; }

std::string_view AssetDatabase::Path(AssetHandle handle) const
{ return IsValid(handle) ? std::string_view(slots_[handle.index].path) : std::string_view{}; }

std::filesystem::path AssetDatabase::FullPath(AssetHandle handle) const
{ return IsValid(handle) ? project_root_ / slots_[handle.index].path : std::filesystem::path{}; }

void AssetDatabase::ReleaseAll()
{
    free_indices_.clear();
    free_indices_.reserve(slots_.size());
    for (std::uint32_t index = 0; index < slots_.size(); ++index)
    {
        Slot& slot = slots_[index];
        if (!slot.live) continue;
        slot.live = false;
        slot.path.clear();
        slot.guid = {};
        slot.type = AssetType::Unknown;
        slot.references = 0;
        slot.texture_cache = {};
        ++slot.generation;
        if (slot.generation == 0) ++slot.generation;
        free_indices_.push_back(index);
    }
    live_count_ = 0;
}

} // namespace CEngine::Assets
