//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/mesh_loader.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/mesh_loader.h"

#include "assets/asset_error.h"
#include "assets/asset_io.h"
#include "assets/binary.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include <glm/geometric.hpp>

namespace CEngine::Assets
{
namespace Renderer = CEngine::Renderer;

namespace
{

constexpr std::uint32_t StaticVertexStride = 40;
constexpr std::uint32_t SkinnedVertexStride = 56;
constexpr std::uint32_t MeshFlagSkinned = 1u << 0u;
constexpr std::uint32_t MeshFlagLightmapUv = 1u << 1u;
constexpr std::array<char, 4> MeshMetadataMagic = {'C', 'E', 'M', 'H'};
constexpr std::uint16_t MeshMetadataVersion = 2;

#pragma pack(push, 1)
/**
 * @brief TODO: Describe DiskMeshMetadata.
 */
struct DiskMeshMetadata
{
    std::array<char, 4> magic = MeshMetadataMagic;
    std::uint16_t version = MeshMetadataVersion;
    std::uint16_t header_size = 0;
    std::uint32_t flags = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    std::uint32_t vertex_stride = 0;
    std::uint32_t index_size = 0;
    float bounds_min[3] = {};
    float bounds_max[3] = {};
    std::uint32_t material_slot_count = 0;
    std::uint32_t geometry_offset = 0;
    std::uint32_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
    std::uint32_t reserved2 = 0;
};
#pragma pack(pop)

static_assert(sizeof(DiskMeshMetadata) == 72, "DiskMeshMetadata must stay packed and stable.");

/**
 * @brief TODO: Describe ReadMetadata.
 *
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadMetadata(ByteView bytes, DiskMeshMetadata &value)
{
    if (bytes.size < sizeof(value))
    {
        return false;
    }
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    if (!ReadU16LE(bytes, offset, value.version) || !ReadU16LE(bytes, offset, value.header_size) ||
        !ReadU32LE(bytes, offset, value.flags) || !ReadU32LE(bytes, offset, value.vertex_count) ||
        !ReadU32LE(bytes, offset, value.index_count) || !ReadU32LE(bytes, offset, value.vertex_stride) ||
        !ReadU32LE(bytes, offset, value.index_size))
    {
        return false;
    }
    for (float &component : value.bounds_min)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    for (float &component : value.bounds_max)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    return ReadU32LE(bytes, offset, value.material_slot_count) && ReadU32LE(bytes, offset, value.geometry_offset) &&
           ReadU32LE(bytes, offset, value.reserved0) && ReadU32LE(bytes, offset, value.reserved1) &&
           ReadU32LE(bytes, offset, value.reserved2);
}

/**
 * @brief TODO: Describe ReadVec3.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
glm::vec3 ReadVec3(ByteView bytes, std::size_t offset)
{
    glm::vec3 value;
    ReadF32LE(bytes, offset, value.x);
    ReadF32LE(bytes, offset, value.y);
    ReadF32LE(bytes, offset, value.z);
    return value;
}

/**
 * @brief TODO: Describe ReadVec2.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
glm::vec2 ReadVec2(ByteView bytes, std::size_t offset)
{
    glm::vec2 value;
    ReadF32LE(bytes, offset, value.x);
    ReadF32LE(bytes, offset, value.y);
    return value;
}

/**
 * @brief TODO: Describe CheckedMul.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @param result TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool CheckedMul(std::uint32_t left, std::uint32_t right, std::size_t &result)
{
    const std::uint64_t wide = static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right);
    if (wide > std::numeric_limits<std::size_t>::max())
    {
        return false;
    }
    result = static_cast<std::size_t>(wide);
    return true;
}

/**
 * @brief TODO: Describe TangentForNormal.
 *
 * @param raw_tangent TODO: Describe this parameter.
 * @param normal TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
glm::vec3 TangentForNormal(const glm::vec3 &raw_tangent, const glm::vec3 &normal)
{
    constexpr float Epsilon = 1.0e-12f;
    const float normal_length_squared = glm::dot(normal, normal);
    const glm::vec3 unit_normal =
        normal_length_squared > Epsilon ? normal / std::sqrt(normal_length_squared) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = raw_tangent - unit_normal * glm::dot(unit_normal, raw_tangent);
    float tangent_length_squared = glm::dot(tangent, tangent);
    if (tangent_length_squared <= Epsilon)
    {
        const glm::vec3 helper =
            std::abs(unit_normal.z) < 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        tangent = glm::cross(helper, unit_normal);
        tangent_length_squared = glm::dot(tangent, tangent);
    }
    return tangent / std::sqrt(tangent_length_squared);
}

/**
 * @brief TODO: Describe ReadMeshPayloads.
 *
 * @param asset TODO: Describe this parameter.
 * @param metadata TODO: Describe this parameter.
 * @param vertex_bytes TODO: Describe this parameter.
 * @param index_bytes TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadMeshPayloads(const AssetFile &asset, DiskMeshMetadata &metadata, ByteView &vertex_bytes, ByteView &index_bytes)
{
    const ByteView payload = asset.Payload();
    if (payload.size < sizeof(DiskMeshMetadata))
    {
        return AssetError("mesh payload is invalid");
    }

    if (!ReadMetadata(payload, metadata))
    {
        return AssetError("mesh payload is invalid");
    }
    if (metadata.magic != MeshMetadataMagic || metadata.version != MeshMetadataVersion ||
        metadata.header_size != sizeof(DiskMeshMetadata))
    {
        return AssetError("mesh metadata header is not supported");
    }
    if (metadata.geometry_offset > payload.size)
    {
        return AssetError("mesh geometry offset is outside the payload");
    }

    std::size_t vertex_size = 0;
    std::size_t index_size = 0;
    if (!CheckedMul(metadata.vertex_count, metadata.vertex_stride, vertex_size) ||
        !CheckedMul(metadata.index_count, metadata.index_size, index_size))
    {
        return AssetError("mesh buffer sizes overflow");
    }
    const std::size_t geometry_size = payload.size - metadata.geometry_offset;
    if (vertex_size > geometry_size || index_size > geometry_size - vertex_size)
    {
        return AssetError("mesh payload is smaller than metadata declares");
    }

    vertex_bytes = {payload.data + metadata.geometry_offset, vertex_size};
    index_bytes = {payload.data + metadata.geometry_offset + vertex_size, index_size};
    return true;
}

} // namespace

/**
 * @brief TODO: Describe LoadMeshAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param mesh TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadMeshAsset(const std::filesystem::path &path, Renderer::Mesh &mesh)
{
    AssetFile asset;
    if (!asset.Load(path))
    {
        return false;
    }

    DiskMeshMetadata metadata;
    ByteView vertex_bytes;
    ByteView index_bytes;
    if (!ReadMeshPayloads(asset, metadata, vertex_bytes, index_bytes))
    {
        return false;
    }

    if (metadata.material_slot_count != 1)
    {
        return AssetError("mesh loader expects exactly one material slot in this phase");
    }
    const std::uint32_t expected_vertex_stride =
        (metadata.flags & MeshFlagSkinned) != 0 ? SkinnedVertexStride : StaticVertexStride;
    if (metadata.vertex_stride != expected_vertex_stride || metadata.index_size != sizeof(std::uint32_t))
    {
        return AssetError("mesh loader does not support this vertex/index layout");
    }

    Renderer::Mesh loaded;
    loaded.name = path.stem().string();
    Renderer::Mesh &mesh_data = loaded;
    mesh_data.vertices.resize(metadata.vertex_count);
    mesh_data.indices.resize(metadata.index_count);
    mesh_data.has_lightmap_uv = (metadata.flags & MeshFlagLightmapUv) != 0;
    mesh_data.local_bounds.min = glm::vec3(metadata.bounds_min[0], metadata.bounds_min[1], metadata.bounds_min[2]);
    mesh_data.local_bounds.max = glm::vec3(metadata.bounds_max[0], metadata.bounds_max[1], metadata.bounds_max[2]);
    mesh_data.local_bounds.valid = metadata.vertex_count > 0;

    for (std::uint32_t vertex_index = 0; vertex_index < metadata.vertex_count; ++vertex_index)
    {
        const std::size_t offset = static_cast<std::size_t>(vertex_index) * metadata.vertex_stride;
        Renderer::MeshVertex &vertex = mesh_data.vertices[vertex_index];
        vertex.position = ReadVec3(vertex_bytes, offset);
        vertex.normal = ReadVec3(vertex_bytes, offset + 12u);
        vertex.uv = ReadVec2(vertex_bytes, offset + 24u);
        vertex.lightmap_uv = ReadVec2(vertex_bytes, offset + 32u);
        vertex.tangent = glm::vec3(0.0f);
    }

    for (std::uint32_t index = 0; index < metadata.index_count; ++index)
    {
        std::size_t index_offset = static_cast<std::size_t>(index) * sizeof(std::uint32_t);
        std::uint32_t vertex_index = 0;
        if (!ReadU32LE(index_bytes, index_offset, vertex_index))
        {
            return AssetError("mesh index buffer is truncated");
        }
        if (vertex_index >= metadata.vertex_count)
        {
            return AssetError("mesh index references a vertex outside the vertex buffer");
        }
        mesh_data.indices[index] = vertex_index;
    }

    for (std::size_t first = 0; first + 2 < mesh_data.indices.size(); first += 3)
    {
        const std::uint32_t first_index = mesh_data.indices[first];
        const std::uint32_t second_index = mesh_data.indices[first + 1];
        const std::uint32_t third_index = mesh_data.indices[first + 2];
        Renderer::MeshVertex &first_vertex = mesh_data.vertices[first_index];
        Renderer::MeshVertex &second_vertex = mesh_data.vertices[second_index];
        Renderer::MeshVertex &third_vertex = mesh_data.vertices[third_index];
        const glm::vec3 edge1 = second_vertex.position - first_vertex.position;
        const glm::vec3 edge2 = third_vertex.position - first_vertex.position;
        const glm::vec2 uv_edge1 = second_vertex.uv - first_vertex.uv;
        const glm::vec2 uv_edge2 = third_vertex.uv - first_vertex.uv;
        const float determinant = (uv_edge1.x * uv_edge2.y) - (uv_edge1.y * uv_edge2.x);
        const glm::vec3 raw_tangent =
            std::abs(determinant) > 1.0e-12f ? (edge1 * uv_edge2.y - edge2 * uv_edge1.y) / determinant : edge1;
        first_vertex.tangent += raw_tangent;
        second_vertex.tangent += raw_tangent;
        third_vertex.tangent += raw_tangent;
    }
    for (Renderer::MeshVertex &vertex : mesh_data.vertices)
    {
        vertex.tangent = TangentForNormal(vertex.tangent, vertex.normal);
    }

    mesh = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
