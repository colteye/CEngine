#include "assets/asset_store.h"

#include "assets/asset_error.h"
#include "assets/asset_io.h"

#include <algorithm>

namespace CEngine::Assets
{
namespace
{
bool IsExternalPayload(AssetType type)
{
    return type == AssetType::Texture || type == AssetType::Audio;
}
} // namespace

bool NormalizeProjectAssetPath(std::string_view input, std::string &output)
{
    if (input.empty())
    {
        return false;
    }
    output.assign(input);
    std::replace(output.begin(), output.end(), '\\', '/');
    const std::filesystem::path path(output);
    if (path.is_absolute())
    {
        return false;
    }
    output = path.lexically_normal().generic_string();
    return !output.empty() && output != "." && output != ".." && output.compare(0, 3, "../") != 0;
}

AssetStore::AssetStore(std::filesystem::path project_root) : project_root_(std::move(project_root))
{
}

bool AssetStore::Resolve(std::string_view project_relative_path, AssetType expected_type, const Guid &expected_guid,
                         AssetReference &reference) const
{
    reference = {};
    std::string normalized;
    if (!NormalizeProjectAssetPath(project_relative_path, normalized))
    {
        return AssetError("asset path must be project-relative");
    }
    if (expected_type == AssetType::Unknown || expected_type == AssetType::Scene)
    {
        return AssetError("asset reference type is invalid");
    }

    const std::filesystem::path full_path = project_root_ / normalized;
    if (!std::filesystem::is_regular_file(full_path))
    {
        return AssetError("referenced asset does not exist: " + normalized);
    }
    if (!IsExternalPayload(expected_type))
    {
        AssetFile file;
        if (!file.Load(full_path))
        {
            return false;
        }
        if (file.Type() != expected_type)
        {
            return AssetError("referenced asset type does not match: " + normalized);
        }
        if (file.GetGuid() != expected_guid)
        {
            return AssetError("referenced asset GUID does not match: " + normalized);
        }
    }

    reference = {std::move(normalized), expected_guid, expected_type};
    return true;
}

bool AssetStore::FullPath(const AssetReference &reference, AssetType expected_type, std::filesystem::path &path) const
{
    if (reference.type != expected_type)
    {
        return AssetError("asset reference type does not match loader");
    }
    AssetReference validated;
    if (!Resolve(reference.path, expected_type, reference.guid, validated))
    {
        return false;
    }
    path = project_root_ / validated.path;
    return true;
}

} // namespace CEngine::Assets
