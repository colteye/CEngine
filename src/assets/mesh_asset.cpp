//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/mesh_asset.cpp
 * @brief Instantiates renderer mesh LODs from generated wire data.
 * @author Erik Coltey
 */

#include "assets/mesh_asset.h"

#include "engine/engine_entities.generated.h"
#include "log.h"

#include <cmath>
#include <algorithm>

#include <glm/geometric.hpp>

namespace CEngine::Assets
{
namespace
{
namespace Wire = Generated::EngineEntities::Wire;

glm::vec3 Tangent(const glm::vec3 &raw, const glm::vec3 &normal)
{
    constexpr float Epsilon = 1.0e-12f;
    const float normal_length = glm::dot(normal, normal);
    const glm::vec3 unit =
        normal_length > Epsilon ? normal / std::sqrt(normal_length) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = raw - unit * glm::dot(unit, raw);
    float length = glm::dot(tangent, tangent);
    if (length <= Epsilon)
    {
        const glm::vec3 helper =
            std::abs(unit.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        tangent = glm::cross(helper, unit);
        length = glm::dot(tangent, tangent);
    }
    return tangent / std::sqrt(length);
}

bool BuildLod(const Wire::MeshLod &source, bool skinned, Renderer::MeshLod &lod)
{
    if (source.indices.empty() || source.indices.size() % 3u != 0u)
    {
        Logging::Logger::Get().Error("assets", "mesh LOD indices are not triangles");
        return false;
    }
    lod.screen_size = source.screen_size;
    lod.vertices.resize(source.vertices.size());
    lod.indices = source.indices;
    for (std::size_t index = 0; index < source.vertices.size(); ++index)
    {
        const Wire::MeshVertex &input = source.vertices[index];
        Renderer::MeshVertex &output = lod.vertices[index];
        output.position = {input.position[0], input.position[1], input.position[2]};
        output.normal = {input.normal[0], input.normal[1], input.normal[2]};
        output.uv = {input.uv[0], input.uv[1]};
        output.lightmap_uv = {input.lightmap_uv[0], input.lightmap_uv[1]};
        output.tangent = glm::vec3(0.0f);
        output.joints = input.joints;
        output.weights = {input.weights[0], input.weights[1], input.weights[2], input.weights[3]};
        float weight_sum = 0.0f;
        for (std::size_t influence = 0; influence < input.weights.size(); ++influence)
        {
            if (input.weights[influence] == 0.0f && input.joints[influence] != 0u)
            {
                Logging::Logger::Get().Error("assets", "zero-weight mesh influence has a joint index");
                return false;
            }
            if (input.joints[influence] >= 1024u)
            {
                Logging::Logger::Get().Error("assets", "mesh joint index exceeds animation format capacity");
                return false;
            }
            weight_sum += input.weights[influence];
        }
        if ((skinned && std::abs(weight_sum - 1.0f) > 1.0e-3f) ||
            (!skinned && (weight_sum != 0.0f || input.joints != std::array<std::uint16_t, 4>{})))
        {
            Logging::Logger::Get().Error("assets", "mesh skin weights do not match its skinning flag");
            return false;
        }
    }
    for (std::uint32_t index : lod.indices)
    {
        if (index >= lod.vertices.size())
        {
            Logging::Logger::Get().Error("assets", "mesh LOD index is outside its vertex buffer");
            return false;
        }
    }
    for (std::size_t first = 0; first < lod.indices.size(); first += 3u)
    {
        Renderer::MeshVertex &a = lod.vertices[lod.indices[first]];
        Renderer::MeshVertex &b = lod.vertices[lod.indices[first + 1u]];
        Renderer::MeshVertex &c = lod.vertices[lod.indices[first + 2u]];
        const glm::vec3 edge1 = b.position - a.position;
        const glm::vec3 edge2 = c.position - a.position;
        const glm::vec2 uv1 = b.uv - a.uv;
        const glm::vec2 uv2 = c.uv - a.uv;
        const float determinant = uv1.x * uv2.y - uv1.y * uv2.x;
        const glm::vec3 raw =
            std::abs(determinant) > 1.0e-12f ? (edge1 * uv2.y - edge2 * uv1.y) / determinant : edge1;
        a.tangent += raw;
        b.tangent += raw;
        c.tangent += raw;
    }
    for (Renderer::MeshVertex &vertex : lod.vertices)
    {
        vertex.tangent = Tangent(vertex.tangent, vertex.normal);
    }
    return true;
}
} // namespace

bool LoadMeshAsset(const std::filesystem::path &path, Renderer::Mesh &mesh)
{
    Wire::Mesh decoded;
    if (!Decode(path, Type::Mesh, decoded))
    {
        Logging::Logger::Get().Error("assets", "mesh payload is invalid");
        return false;
    }

    Renderer::Mesh loaded;
    loaded.name = path.stem().string();
    loaded.skinned = (decoded.flags & 1u) != 0u;
    loaded.skeleton_guid = decoded.skeleton_guid;
    loaded.has_lightmap_uv = (decoded.flags & 2u) != 0u;
    const bool has_skeleton_guid =
        std::any_of(loaded.skeleton_guid.begin(), loaded.skeleton_guid.end(), [](std::uint8_t value) {
            return value != 0u;
        });
    if (loaded.skinned != has_skeleton_guid)
    {
        Logging::Logger::Get().Error("assets", "mesh skinning flag and skeleton GUID disagree");
        return false;
    }
    loaded.local_bounds.min = {decoded.bounds_min[0], decoded.bounds_min[1], decoded.bounds_min[2]};
    loaded.local_bounds.max = {decoded.bounds_max[0], decoded.bounds_max[1], decoded.bounds_max[2]};
    loaded.local_bounds.valid = !decoded.lods.front().vertices.empty();
    loaded.lods.resize(decoded.lods.size());
    float previous = 2.0f;
    for (std::size_t index = 0; index < decoded.lods.size(); ++index)
    {
        if (decoded.lods[index].screen_size >= previous ||
            !BuildLod(decoded.lods[index], loaded.skinned, loaded.lods[index]))
        {
            Logging::Logger::Get().Error("assets", "mesh LOD order is invalid");
            return false;
        }
        previous = decoded.lods[index].screen_size;
    }
    mesh = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
