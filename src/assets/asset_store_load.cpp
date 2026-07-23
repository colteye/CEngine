//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/asset_store_load.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/asset_store.h"

#include "assets/asset_error.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "assets/physics_loader.h"
#include "assets/texture_loader.h"

#include <memory>

namespace CEngine::Assets
{

/**
 * @brief TODO: Describe AssetStore::LoadMaterial.
 *
 * @param reference TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::shared_ptr<const Renderer::Material> AssetStore::LoadMaterial(const AssetReference &reference) const
{
    if (reference.type != AssetType::Material)
    {
        AssetError("asset reference is not a material");
        return {};
    }
    const auto found = materials_.find(reference.path);
    if (found != materials_.end())
    {
        if (found->second.first != reference.guid)
        {
            AssetError("material path was requested with a different GUID");
            return {};
        }
        return found->second.second;
    }
    std::filesystem::path path;
    if (!FullPath(reference, AssetType::Material, path))
    {
        return {};
    }
    auto material = std::make_shared<Renderer::Material>();
    if (!LoadMaterialAsset(path, *material))
    {
        return {};
    }
    return materials_.emplace(reference.path, std::make_pair(reference.guid, std::move(material))).first->second.second;
}

/**
 * @brief TODO: Describe AssetStore::LoadMesh.
 *
 * @param reference TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::shared_ptr<const Renderer::Mesh> AssetStore::LoadMesh(const AssetReference &reference) const
{
    if (reference.type != AssetType::Mesh)
    {
        AssetError("asset reference is not a mesh");
        return {};
    }
    const auto found = meshes_.find(reference.path);
    if (found != meshes_.end())
    {
        if (found->second.first != reference.guid)
        {
            AssetError("mesh path was requested with a different GUID");
            return {};
        }
        return found->second.second;
    }
    std::filesystem::path path;
    if (!FullPath(reference, AssetType::Mesh, path))
    {
        return {};
    }
    auto mesh = std::make_shared<Renderer::Mesh>();
    if (!LoadMeshAsset(path, *mesh))
    {
        return {};
    }
    return meshes_.emplace(reference.path, std::make_pair(reference.guid, std::move(mesh))).first->second.second;
}

/**
 * @brief TODO: Describe AssetStore::LoadTexture.
 *
 * @param reference TODO: Describe this parameter.
 * @param flip_vertical TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::shared_ptr<const Renderer::Texture> AssetStore::LoadTexture(const AssetReference &reference,
                                                                 bool flip_vertical) const
{
    if (reference.type != AssetType::Texture)
    {
        AssetError("asset reference is not a texture");
        return {};
    }

    auto &cache = flip_vertical ? flipped_textures_ : textures_;
    const auto found = cache.find(reference.path);
    if (found != cache.end())
    {
        if (found->second.first != reference.guid)
        {
            AssetError("texture path was requested with a different GUID");
            return {};
        }
        return found->second.second;
    }
    std::filesystem::path path;
    if (!FullPath(reference, AssetType::Texture, path))
    {
        return {};
    }

    auto texture = std::make_shared<Renderer::Texture>();
    if (!LoadTextureAsset(path, *texture, flip_vertical))
    {
        return {};
    }
    return cache.emplace(reference.path, std::make_pair(reference.guid, std::move(texture))).first->second.second;
}

/**
 * @brief TODO: Describe AssetStore::LoadPhysics.
 *
 * @param reference TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::shared_ptr<const PhysicsShape> AssetStore::LoadPhysics(const AssetReference &reference) const
{
    if (reference.type != AssetType::Physics)
    {
        AssetError("asset reference is not physics");
        return {};
    }
    const auto found = physics_shapes_.find(reference.path);
    if (found != physics_shapes_.end())
    {
        if (found->second.first != reference.guid)
        {
            AssetError("physics path was requested with a different GUID");
            return {};
        }
        return found->second.second;
    }
    std::filesystem::path path;
    if (!FullPath(reference, AssetType::Physics, path))
    {
        return {};
    }
    auto shape = std::make_shared<PhysicsShape>();
    if (!LoadPhysicsAsset(path, *shape))
    {
        return {};
    }
    return physics_shapes_.emplace(reference.path, std::make_pair(reference.guid, std::move(shape)))
        .first->second.second;
}

} // namespace CEngine::Assets
