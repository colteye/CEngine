#include "assets/mesh_loader.h"

#include "assets/asset_io.h"

#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace CEngine::Assets {
namespace Renderer = CEngine::Renderer;

namespace {

constexpr std::uint32_t StaticVertexStride = 32;
constexpr std::uint32_t LightmapVertexStride = 40;
constexpr std::uint32_t MeshFlagLightmapUv = 1u << 1u;
constexpr std::array<char, 4> MeshMetadataMagic = {'C', 'E', 'M', 'H'};
constexpr std::uint16_t MeshMetadataVersion = 1;

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

void SetError(std::string* error, const std::string& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

std::uint32_t ReadU32(const std::uint8_t* data)
{
    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

float ReadF32(const std::uint8_t* data)
{
    float value = 0.0f;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

glm::vec3 ReadVec3(const std::uint8_t* data)
{
    return glm::vec3(ReadF32(data), ReadF32(data + 4), ReadF32(data + 8));
}

glm::vec2 ReadVec2(const std::uint8_t* data)
{
    return glm::vec2(ReadF32(data), ReadF32(data + 4));
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

bool ReadMeshPayloads(const AssetFile& asset, DiskMeshMetadata& metadata,
    ByteView& vertex_bytes, ByteView& index_bytes, std::string* error)
{
    const ByteView payload = asset.Payload();
    if (payload.size < sizeof(DiskMeshMetadata))
    {
        SetError(error, "mesh payload is invalid");
        return false;
    }

    std::memcpy(&metadata, payload.data, sizeof(metadata));
    if (metadata.magic != MeshMetadataMagic ||
        metadata.version != MeshMetadataVersion ||
        metadata.header_size != sizeof(DiskMeshMetadata))
    {
        SetError(error, "mesh metadata header is not supported");
        return false;
    }
    if (metadata.geometry_offset > payload.size)
    {
        SetError(error, "mesh geometry offset is outside the payload");
        return false;
    }

    std::size_t vertex_size = 0;
    std::size_t index_size = 0;
    if (!CheckedMul(metadata.vertex_count, metadata.vertex_stride, vertex_size) ||
        !CheckedMul(metadata.index_count, metadata.index_size, index_size))
    {
        SetError(error, "mesh buffer sizes overflow");
        return false;
    }
    const std::size_t geometry_size = payload.size - metadata.geometry_offset;
    if (vertex_size > geometry_size ||
        index_size > geometry_size - vertex_size)
    {
        SetError(error, "mesh payload is smaller than metadata declares");
        return false;
    }

    vertex_bytes = {payload.data + metadata.geometry_offset, vertex_size};
    index_bytes = {payload.data + metadata.geometry_offset + vertex_size, index_size};
    return true;
}

} // namespace

bool LoadMeshAsset(const std::filesystem::path& path,
    const std::vector<Renderer::Material*>& material_slots,
    Renderer::Mesh& mesh,
    std::string* error)
{
    AssetFile asset;
    if (!asset.Load(path, error))
    {
        return false;
    }

    DiskMeshMetadata metadata;
    ByteView vertex_bytes;
    ByteView index_bytes;
    if (!ReadMeshPayloads(asset, metadata, vertex_bytes, index_bytes, error))
    {
        return false;
    }

    if (metadata.material_slot_count != 1)
    {
        SetError(error, "mesh loader expects exactly one material slot in this phase");
        return false;
    }
    if (metadata.vertex_stride < StaticVertexStride || metadata.index_size != sizeof(std::uint32_t))
    {
        SetError(error, "mesh loader does not support this vertex/index layout");
        return false;
    }

    if (material_slots.size() != metadata.material_slot_count || material_slots[0] == nullptr)
    {
        SetError(error, "mesh material slot 0 is not bound");
        return false;
    }

    Renderer::MeshData mesh_data;
    mesh_data.vertices.reserve(metadata.index_count);
    mesh_data.normals.reserve(metadata.index_count);
    mesh_data.uvs.reserve(metadata.index_count);
    mesh_data.lightmap_uvs.reserve(metadata.index_count);
    mesh_data.tangents.reserve(metadata.index_count);
	mesh_data.has_lightmap_uv = (metadata.flags & MeshFlagLightmapUv) != 0;
	if (mesh_data.has_lightmap_uv && metadata.vertex_stride < LightmapVertexStride)
	{
		SetError(error, "mesh declares lightmap UVs but its vertex stride is too small");
		return false;
	}
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
        const std::uint32_t vertex_index = ReadU32(index_bytes.data + index * sizeof(std::uint32_t));
        if (vertex_index >= metadata.vertex_count)
        {
            SetError(error, "mesh index references a vertex outside the vertex buffer");
            return false;
        }

        const std::uint8_t* vertex = vertex_bytes.data + vertex_index * metadata.vertex_stride;
        mesh_data.vertices.push_back(ReadVec3(vertex));
        mesh_data.normals.push_back(ReadVec3(vertex + 12));
        mesh_data.uvs.push_back(ReadVec2(vertex + 24));
        mesh_data.lightmap_uvs.push_back(mesh_data.has_lightmap_uv ?
            ReadVec2(vertex + 32) : glm::vec2(0.0f));
        mesh_data.tangents.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
    }

    Renderer::Mesh loaded;
    loaded.mesh_name = path.stem().string();
    loaded.AddMaterialMeshData(material_slots[0], std::move(mesh_data));
    mesh = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
