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

namespace CEngine::Assets {
namespace Renderer = CEngine::Renderer;

namespace {

constexpr std::uint32_t StaticVertexStride = 40;
constexpr std::uint32_t SkinnedVertexStride = 56;
constexpr std::uint32_t MeshFlagSkinned = 1u << 0u;
constexpr std::uint32_t MeshFlagLightmapUv = 1u << 1u;
constexpr std::array<char, 4> MeshMetadataMagic = {'C', 'E', 'M', 'H'};
constexpr std::uint16_t MeshMetadataVersion = 2;

#pragma pack(push, 1)
struct DiskMeshMetadata {
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

bool ReadMetadata(ByteView bytes, DiskMeshMetadata& value)
{
    if (bytes.size < sizeof(value)) return false;
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    if (!ReadU16LE(bytes, offset, value.version) ||
        !ReadU16LE(bytes, offset, value.header_size) ||
        !ReadU32LE(bytes, offset, value.flags) ||
        !ReadU32LE(bytes, offset, value.vertex_count) ||
        !ReadU32LE(bytes, offset, value.index_count) ||
        !ReadU32LE(bytes, offset, value.vertex_stride) ||
        !ReadU32LE(bytes, offset, value.index_size))
        return false;
    for (float& component : value.bounds_min)
        if (!ReadF32LE(bytes, offset, component)) return false;
    for (float& component : value.bounds_max)
        if (!ReadF32LE(bytes, offset, component)) return false;
    return ReadU32LE(bytes, offset, value.material_slot_count) &&
        ReadU32LE(bytes, offset, value.geometry_offset) &&
        ReadU32LE(bytes, offset, value.reserved0) &&
        ReadU32LE(bytes, offset, value.reserved1) &&
        ReadU32LE(bytes, offset, value.reserved2);
}

glm::vec3 ReadVec3(ByteView bytes, std::size_t offset)
{
    glm::vec3 value;
    ReadF32LE(bytes, offset, value.x);
    ReadF32LE(bytes, offset, value.y);
    ReadF32LE(bytes, offset, value.z);
    return value;
}

glm::vec2 ReadVec2(ByteView bytes, std::size_t offset)
{
    glm::vec2 value;
    ReadF32LE(bytes, offset, value.x);
    ReadF32LE(bytes, offset, value.y);
    return value;
}

bool CheckedMul(std::uint32_t left, std::uint32_t right, std::size_t& result)
{
    const std::uint64_t wide = static_cast<std::uint64_t>(left) * static_cast<std::uint64_t>(right);
    if (wide > std::numeric_limits<std::size_t>::max())
    {
        return false;
    }
    result = static_cast<std::size_t>(wide);
    return true;
}

glm::vec3 TangentForNormal(const glm::vec3& raw_tangent, const glm::vec3& normal)
{
    constexpr float Epsilon = 1.0e-12f;
    const float normal_length_squared = glm::dot(normal, normal);
    const glm::vec3 unit_normal = normal_length_squared > Epsilon
        ? normal / std::sqrt(normal_length_squared)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = raw_tangent - unit_normal * glm::dot(unit_normal, raw_tangent);
    float tangent_length_squared = glm::dot(tangent, tangent);
    if (tangent_length_squared <= Epsilon)
    {
        const glm::vec3 helper = std::abs(unit_normal.z) < 0.999f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);
        tangent = glm::cross(helper, unit_normal);
        tangent_length_squared = glm::dot(tangent, tangent);
    }
    return tangent / std::sqrt(tangent_length_squared);
}

bool ReadMeshPayloads(const AssetFile& asset, DiskMeshMetadata& metadata,
    ByteView& vertex_bytes, ByteView& index_bytes)
{
    const ByteView payload = asset.Payload();
    if (payload.size < sizeof(DiskMeshMetadata))
    {
        return AssetError("mesh payload is invalid");
    }

    if (!ReadMetadata(payload, metadata))
        return AssetError("mesh payload is invalid");
    if (metadata.magic != MeshMetadataMagic ||
        metadata.version != MeshMetadataVersion ||
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
    if (vertex_size > geometry_size ||
        index_size > geometry_size - vertex_size)
    {
        return AssetError(
            "mesh payload is smaller than metadata declares");
    }

    vertex_bytes = {payload.data + metadata.geometry_offset, vertex_size};
    index_bytes = {payload.data + metadata.geometry_offset + vertex_size, index_size};
    return true;
}

} // namespace

bool LoadMeshAsset(const std::filesystem::path& path,
    const std::vector<Renderer::Material*>& material_slots,
    Renderer::Mesh& mesh)
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
        return AssetError(
            "mesh loader expects exactly one material slot in this phase");
    }
    const std::uint32_t expected_vertex_stride =
        (metadata.flags & MeshFlagSkinned) != 0 ? SkinnedVertexStride : StaticVertexStride;
    if (metadata.vertex_stride != expected_vertex_stride || metadata.index_size != sizeof(std::uint32_t))
    {
        return AssetError(
            "mesh loader does not support this vertex/index layout");
    }

    if (material_slots.size() != metadata.material_slot_count || material_slots[0] == nullptr)
    {
        return AssetError("mesh material slot 0 is not bound");
    }

    Renderer::MeshData mesh_data;
    mesh_data.vertices.reserve(metadata.index_count);
    mesh_data.normals.reserve(metadata.index_count);
    mesh_data.uvs.reserve(metadata.index_count);
    mesh_data.lightmap_uvs.reserve(metadata.index_count);
    mesh_data.tangents.reserve(metadata.index_count);
    mesh_data.has_lightmap_uv = (metadata.flags & MeshFlagLightmapUv) != 0;
    mesh_data.local_bounds.min = glm::vec3(
        metadata.bounds_min[0],
        metadata.bounds_min[1],
        metadata.bounds_min[2]);
    mesh_data.local_bounds.max = glm::vec3(
        metadata.bounds_max[0],
        metadata.bounds_max[1],
        metadata.bounds_max[2]);
    mesh_data.local_bounds.valid = metadata.vertex_count > 0;

    for (std::uint32_t index = 0; index < metadata.index_count; ++index)
    {
        std::size_t index_offset =
            static_cast<std::size_t>(index) * sizeof(std::uint32_t);
        std::uint32_t vertex_index = 0;
        if (!ReadU32LE(index_bytes, index_offset, vertex_index))
            return AssetError("mesh index buffer is truncated");
        if (vertex_index >= metadata.vertex_count)
        {
            return AssetError(
                "mesh index references a vertex outside the vertex buffer");
        }

        const std::size_t vertex =
            static_cast<std::size_t>(vertex_index) * metadata.vertex_stride;
        mesh_data.vertices.push_back(ReadVec3(vertex_bytes, vertex));
        mesh_data.normals.push_back(ReadVec3(vertex_bytes, vertex + 12u));
        mesh_data.uvs.push_back(ReadVec2(vertex_bytes, vertex + 24u));
        mesh_data.lightmap_uvs.push_back(
            ReadVec2(vertex_bytes, vertex + 32u));
        mesh_data.tangents.push_back(glm::vec3(0.0f));
    }

    for (std::size_t first = 0; first + 2 < mesh_data.vertices.size(); first += 3)
    {
        const glm::vec3 edge1 = mesh_data.vertices[first + 1] - mesh_data.vertices[first];
        const glm::vec3 edge2 = mesh_data.vertices[first + 2] - mesh_data.vertices[first];
        const glm::vec2 uv_edge1 = mesh_data.uvs[first + 1] - mesh_data.uvs[first];
        const glm::vec2 uv_edge2 = mesh_data.uvs[first + 2] - mesh_data.uvs[first];
        const float determinant = uv_edge1.x * uv_edge2.y - uv_edge1.y * uv_edge2.x;
        const glm::vec3 raw_tangent = std::abs(determinant) > 1.0e-12f
            ? (edge1 * uv_edge2.y - edge2 * uv_edge1.y) / determinant
            : edge1;
        for (std::size_t corner = 0; corner < 3; ++corner)
        {
            mesh_data.tangents[first + corner] =
                TangentForNormal(raw_tangent, mesh_data.normals[first + corner]);
        }
    }

    Renderer::Mesh loaded;
    loaded.mesh_name = path.stem().string();
    loaded.AddMaterialMeshData(material_slots[0], std::move(mesh_data));
    mesh = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
