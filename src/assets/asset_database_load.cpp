#include "assets/asset_database.h"

#include "assets/asset_error.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "assets/texture_loader.h"

#include <memory>

namespace CEngine::Assets {

bool AssetDatabase::Load(
    AssetHandle handle, Renderer::Material& material) const
{
    if (Type(handle) != AssetType::Material)
        return AssetError("asset handle is not a material");
    return LoadMaterialAsset(FullPath(handle), material);
}

bool AssetDatabase::Load(AssetHandle handle,
    const std::vector<Renderer::Material*>& materials,
    Renderer::Mesh& mesh) const
{
    if (Type(handle) != AssetType::Mesh)
        return AssetError("asset handle is not a mesh");
    return LoadMeshAsset(FullPath(handle), materials, mesh);
}

bool AssetDatabase::Load(AssetHandle handle, Renderer::Texture& texture,
    bool flip_vertical) const
{
    if (Type(handle) != AssetType::Texture)
        return AssetError("asset handle is not a texture");
    return LoadTextureAsset(FullPath(handle), texture, flip_vertical);
}

std::shared_ptr<const Renderer::Texture> AssetDatabase::LoadTexture(
    AssetHandle handle, bool flip_vertical) const
{
    if (Type(handle) != AssetType::Texture)
    {
        AssetError("asset handle is not a texture");
        return {};
    }

    auto& cached = slots_[handle.index].texture_cache[flip_vertical ? 1u : 0u];
    if (cached) return cached;

    auto texture = std::make_shared<Renderer::Texture>();
    if (!LoadTextureAsset(FullPath(handle), *texture, flip_vertical)) return {};
    cached = std::move(texture);
    return cached;
}

} // namespace CEngine::Assets
