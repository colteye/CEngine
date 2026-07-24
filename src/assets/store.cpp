//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/store.cpp
 * @brief Shared immutable asset cache and reference resolver.
 * @author Erik Coltey
 */

#include "assets/store.h"

#include "assets/animation_asset.h"
#include "assets/audio_asset.h"
#include "assets/casset_asset.h"
#include "assets/material_asset.h"
#include "assets/mesh_asset.h"
#include "assets/particle_asset.h"
#include "assets/physics_asset.h"
#include "assets/reader.h"
#include "assets/skeleton_asset.h"
#include "assets/texture_asset.h"
#include "log.h"

#include <algorithm>

namespace CEngine::Assets
{
namespace
{
/**
 * @brief TODO: Describe IsExternalPayload.
 *
 * @param type TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool IsExternal(Type type)
{
    return type == Type::Texture || type == Type::Audio;
}
} // namespace

/**
 * @brief TODO: Describe NormalizeProjectAssetPath.
 *
 * @param input TODO: Describe this parameter.
 * @param output TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief Construct a store rooted at one project.
 *
 * @param project_root TODO: Describe this parameter.
 */
Store::Store(std::filesystem::path project_root) : project_root_(std::move(project_root))
{
}

/**
 * @brief Validate and normalize a cooked reference.
 *
 * @param project_relative_path TODO: Describe this parameter.
 * @param expected_type TODO: Describe this parameter.
 * @param expected_guid TODO: Describe this parameter.
 * @param reference TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Store::Resolve(std::string_view project_relative_path, Type expected_type, const Guid &expected_guid,
                    Reference &reference) const
{
    reference = {};
    std::string normalized;
    if (!NormalizeProjectAssetPath(project_relative_path, normalized))
    {
        Logging::Logger::Get().Error("assets", "asset path must be project-relative");
        return false;
    }
    if (expected_type == Type::Unknown || expected_type == Type::Scene)
    {
        Logging::Logger::Get().Error("assets", "asset reference type is invalid");
        return false;
    }

    const std::filesystem::path full_path = project_root_ / normalized;
    if (!std::filesystem::is_regular_file(full_path))
    {
        Logging::Logger::Get().Error("assets", "referenced asset does not exist: " + normalized);
        return false;
    }
    if (!IsExternal(expected_type))
    {
        Header header;
        std::vector<std::byte> payload;
        if (!ReadAsset(full_path, header, payload))
        {
            return false;
        }
        if (header.type != expected_type)
        {
            Logging::Logger::Get().Error("assets", "referenced asset type does not match: " + normalized);
            return false;
        }
        if (header.guid != expected_guid)
        {
            Logging::Logger::Get().Error("assets", "referenced asset GUID does not match: " + normalized);
            return false;
        }
    }

    reference = {std::move(normalized), expected_guid, expected_type};
    return true;
}

/**
 * @brief Resolve a validated reference to a filesystem path.
 *
 * @param reference TODO: Describe this parameter.
 * @param expected_type TODO: Describe this parameter.
 * @param path TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Store::FullPath(const Reference &reference, Type expected_type, std::filesystem::path &path) const
{
    if (reference.type != expected_type)
    {
        Logging::Logger::Get().Error("assets", "asset reference type does not match loader");
        return false;
    }
    Reference validated;
    if (!Resolve(reference.path, expected_type, reference.guid, validated))
    {
        return false;
    }
    path = project_root_ / validated.path;
    return true;
}

template <typename T, typename Load>
std::shared_ptr<const T> Store::Get(const Reference &reference, Type type, Cache<T> &cache, Load load) const
{
    if (reference.type != type)
    {
        Logging::Logger::Get().Error("assets", "asset reference type does not match loader");
        return {};
    }
    const auto found = cache.find(reference.path);
    if (found != cache.end())
    {
        if (found->second.first != reference.guid)
        {
            Logging::Logger::Get().Error("assets", "asset path was requested with a different GUID");
            return {};
        }
        return found->second.second;
    }
    std::filesystem::path path;
    if (!FullPath(reference, type, path))
    {
        return {};
    }
    auto value = std::make_shared<T>();
    if (!load(path, *value))
    {
        return {};
    }
    return cache.emplace(reference.path, std::make_pair(reference.guid, std::move(value))).first->second.second;
}

std::shared_ptr<const Renderer::Material> Store::LoadMaterial(const Reference &reference) const
{
    return Get(reference, Type::Material, materials_, LoadMaterialAsset);
}

std::shared_ptr<const Renderer::Mesh> Store::LoadMesh(const Reference &reference) const
{
    return Get(reference, Type::Mesh, meshes_, LoadMeshAsset);
}

std::shared_ptr<const Renderer::Texture> Store::LoadTexture(const Reference &reference, bool flip_vertical) const
{
    return Get(reference, Type::Texture, flip_vertical ? flipped_textures_ : textures_,
               [flip_vertical](const auto &path, auto &texture) {
                   return LoadTextureAsset(path, texture, flip_vertical);
               });
}

std::shared_ptr<const PhysicsShape> Store::LoadPhysics(const Reference &reference) const
{
    return Get(reference, Type::Physics, physics_, LoadPhysicsAsset);
}

std::shared_ptr<const Skeleton> Store::LoadSkeleton(const Reference &reference) const
{
    return Get(reference, Type::Skeleton, skeletons_,
               [&reference](const auto &path, auto &skeleton) { return skeleton.Load(path, reference.guid); });
}

std::shared_ptr<const Animation> Store::LoadAnimation(const Reference &reference) const
{
    return Get(reference, Type::Animation, animations_, LoadAnimationAsset);
}

std::shared_ptr<const CAsset> Store::LoadCAsset(const Reference &reference) const
{
    return Get(reference, Type::Asset, assets_, [](const auto &path, auto &asset) { return asset.Load(path); });
}

std::shared_ptr<const Particle> Store::LoadParticle(const Reference &reference) const
{
    return Get(reference, Type::Particle, particles_, LoadParticleAsset);
}

std::shared_ptr<const Audio> Store::LoadAudio(const Reference &reference) const
{
    return Get(reference, Type::Audio, audio_, LoadAudioAsset);
}

} // namespace CEngine::Assets
