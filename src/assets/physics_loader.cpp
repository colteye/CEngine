#include "assets/physics_loader.h"

#include "assets/asset_error.h"
#include "assets/asset_io.h"
#include "assets/binary.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <vector>

namespace CEngine::Assets
{
namespace
{

constexpr std::array<char, 4> PhysicsMagic = {'C', 'E', 'P', 'H'};
constexpr std::uint16_t PhysicsVersion = 2;
constexpr std::uint32_t InvalidShapeIndex = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t MaxShapes = 1024;
constexpr std::uint32_t MaxVertices = 4u * 1024u * 1024u;
constexpr std::uint32_t MaxIndices = 12u * 1024u * 1024u;
constexpr std::uint32_t MaxHeights = 4u * 1024u * 1024u;

#pragma pack(push, 1)
struct DiskPhysicsHeader
{
    std::array<char, 4> magic = PhysicsMagic;
    std::uint16_t version = PhysicsVersion;
    std::uint16_t header_size = 0;
    std::uint32_t shape_count = 0;
    std::uint32_t shape_stride = 0;
    std::uint64_t shape_offset = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t index_count = 0;
    std::uint32_t height_count = 0;
    std::uint32_t reserved = 0;
    std::uint64_t vertex_offset = 0;
    std::uint64_t index_offset = 0;
    std::uint64_t height_offset = 0;
};

struct DiskPhysicsShape
{
    std::uint32_t type = 0;
    std::uint32_t parent = InvalidShapeIndex;
    std::uint32_t flags = 0;
    std::uint32_t material = 0;
    std::uint32_t first_vertex = 0;
    std::uint32_t vertex_count = 0;
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    std::uint32_t first_height = 0;
    std::uint32_t height_count = 0;
    std::uint32_t samples_per_side = 0;
    float local_position[3] = {};
    float local_rotation[4] = {};
    float half_extents[3] = {};
    float radius = 0.0f;
    float half_height = 0.0f;
    float height_field_offset[3] = {};
    float height_field_scale[3] = {};
};
#pragma pack(pop)

static_assert(sizeof(DiskPhysicsHeader) == 64);
static_assert(sizeof(DiskPhysicsShape) == 116);

bool RangeFits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::size_t size)
{
    if (count != 0 && stride > std::numeric_limits<std::uint64_t>::max() / count)
    {
        return false;
    }
    const std::uint64_t bytes = count * stride;
    return offset <= size && bytes <= static_cast<std::uint64_t>(size) - offset;
}

bool ReadHeader(ByteView bytes, DiskPhysicsHeader &value)
{
    if (bytes.size < sizeof(value))
    {
        return false;
    }
    std::size_t offset = 0;
    std::memcpy(value.magic.data(), bytes.data, value.magic.size());
    offset += value.magic.size();
    return ReadU16LE(bytes, offset, value.version) && ReadU16LE(bytes, offset, value.header_size) &&
           ReadU32LE(bytes, offset, value.shape_count) && ReadU32LE(bytes, offset, value.shape_stride) &&
           ReadU64LE(bytes, offset, value.shape_offset) && ReadU32LE(bytes, offset, value.vertex_count) &&
           ReadU32LE(bytes, offset, value.index_count) && ReadU32LE(bytes, offset, value.height_count) &&
           ReadU32LE(bytes, offset, value.reserved) && ReadU64LE(bytes, offset, value.vertex_offset) &&
           ReadU64LE(bytes, offset, value.index_offset) && ReadU64LE(bytes, offset, value.height_offset);
}

bool ReadShape(ByteView bytes, std::size_t offset, DiskPhysicsShape &value)
{
    if (!ReadU32LE(bytes, offset, value.type) || !ReadU32LE(bytes, offset, value.parent) ||
        !ReadU32LE(bytes, offset, value.flags) || !ReadU32LE(bytes, offset, value.material) ||
        !ReadU32LE(bytes, offset, value.first_vertex) || !ReadU32LE(bytes, offset, value.vertex_count) ||
        !ReadU32LE(bytes, offset, value.first_index) || !ReadU32LE(bytes, offset, value.index_count) ||
        !ReadU32LE(bytes, offset, value.first_height) || !ReadU32LE(bytes, offset, value.height_count) ||
        !ReadU32LE(bytes, offset, value.samples_per_side))
    {
        return false;
    }
    for (float &component : value.local_position)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    for (float &component : value.local_rotation)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    for (float &component : value.half_extents)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    if (!ReadF32LE(bytes, offset, value.radius) || !ReadF32LE(bytes, offset, value.half_height))
    {
        return false;
    }
    for (float &component : value.height_field_offset)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    for (float &component : value.height_field_scale)
    {
        if (!ReadF32LE(bytes, offset, component))
        {
            return false;
        }
    }
    return true;
}

bool SpanFits(std::uint32_t first, std::uint32_t count, std::uint32_t total)
{
    return first <= total && count <= total - first;
}

bool Finite(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool DecodeShapeType(std::uint32_t value, PhysicsShapeType &type)
{
    if (value > static_cast<std::uint32_t>(PhysicsShapeType::Plane))
    {
        return false;
    }
    type = static_cast<PhysicsShapeType>(value);
    return true;
}

bool ValidShape(const PhysicsShape &shape, std::uint32_t depth = 0)
{
    if (depth > 8 || !Finite(shape.local_position) || !std::isfinite(shape.local_rotation.x) ||
        !std::isfinite(shape.local_rotation.y) || !std::isfinite(shape.local_rotation.z) ||
        !std::isfinite(shape.local_rotation.w) || glm::dot(shape.local_rotation, shape.local_rotation) <= 1.0e-12f)
    {
        return false;
    }
    const bool no_geometry =
        shape.vertices.empty() && shape.indices.empty() && shape.heights.empty() && shape.samples_per_side == 0;
    switch (shape.type)
    {
    case PhysicsShapeType::Box:
        return Finite(shape.half_extents) && shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               shape.half_extents.z > 0.0f && no_geometry && shape.children.empty();
    case PhysicsShapeType::Sphere:
        return std::isfinite(shape.radius) && shape.radius > 0.0f && no_geometry && shape.children.empty();
    case PhysicsShapeType::Capsule:
    case PhysicsShapeType::Cylinder:
        return std::isfinite(shape.radius) && shape.radius > 0.0f && std::isfinite(shape.half_height) &&
               shape.half_height > 0.0f && no_geometry && shape.children.empty();
    case PhysicsShapeType::Plane:
        return Finite(shape.half_extents) && shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               no_geometry && shape.children.empty();
    case PhysicsShapeType::ConvexHull:
        return shape.vertices.size() >= 4 && std::all_of(shape.vertices.begin(), shape.vertices.end(), Finite) &&
               shape.indices.empty() && shape.heights.empty() && shape.samples_per_side == 0 && shape.children.empty();
    case PhysicsShapeType::TriangleMesh:
        return shape.vertices.size() >= 3 && !shape.indices.empty() && shape.indices.size() % 3 == 0 &&
               std::all_of(shape.vertices.begin(), shape.vertices.end(), Finite) && shape.heights.empty() &&
               shape.samples_per_side == 0 && shape.children.empty();
    case PhysicsShapeType::HeightField: {
        const std::size_t side = shape.samples_per_side;
        return side >= 2 && side <= std::numeric_limits<std::size_t>::max() / side &&
               shape.heights.size() == side * side && Finite(shape.height_field_offset) &&
               Finite(shape.height_field_scale) && shape.height_field_scale.x > 0.0f &&
               shape.height_field_scale.y > 0.0f && shape.height_field_scale.z > 0.0f && shape.vertices.empty() &&
               shape.indices.empty() && shape.children.empty();
    }
    case PhysicsShapeType::Compound:
        return no_geometry && !shape.children.empty() &&
               std::all_of(shape.children.begin(), shape.children.end(),
                           [depth](const PhysicsShape &child) { return ValidShape(child, depth + 1); });
    }
    return false;
}

} // namespace

bool LoadPhysicsAsset(const std::filesystem::path &path, PhysicsShape &shape)
{
    shape = {};
    AssetFile asset;
    if (!asset.Load(path))
    {
        return false;
    }
    if (asset.Type() != AssetType::Physics)
    {
        return AssetError("asset is not a .cphys");
    }

    const ByteView payload = asset.Payload();
    DiskPhysicsHeader header;
    if (!ReadHeader(payload, header) || header.magic != PhysicsMagic || header.version != PhysicsVersion ||
        header.header_size != sizeof(DiskPhysicsHeader) || header.shape_count == 0 || header.shape_count > MaxShapes ||
        header.shape_stride != sizeof(DiskPhysicsShape) || header.vertex_count > MaxVertices ||
        header.index_count > MaxIndices || header.height_count > MaxHeights ||
        !RangeFits(header.shape_offset, header.shape_count, header.shape_stride, payload.size) ||
        !RangeFits(header.vertex_offset, header.vertex_count, sizeof(float) * 3u, payload.size) ||
        !RangeFits(header.index_offset, header.index_count, sizeof(std::uint32_t), payload.size) ||
        !RangeFits(header.height_offset, header.height_count, sizeof(float), payload.size))
    {
        return AssetError("physics payload header is invalid");
    }

    std::vector<DiskPhysicsShape> disk_shapes(header.shape_count);
    for (std::uint32_t index = 0; index < header.shape_count; ++index)
    {
        if (!ReadShape(payload,
                       static_cast<std::size_t>(header.shape_offset) +
                           (static_cast<std::size_t>(index) * header.shape_stride),
                       disk_shapes[index]))
        {
            return AssetError("physics shape table is truncated");
        }
    }

    std::vector<PhysicsShape> shapes(header.shape_count);
    std::vector<std::vector<std::uint32_t>> children(header.shape_count);
    for (std::uint32_t index = 0; index < header.shape_count; ++index)
    {
        const DiskPhysicsShape &disk = disk_shapes[index];
        if ((index == 0 && disk.parent != InvalidShapeIndex) || (index != 0 && disk.parent >= index) ||
            disk.flags != 0 || disk.material != 0 ||
            !SpanFits(disk.first_vertex, disk.vertex_count, header.vertex_count) ||
            !SpanFits(disk.first_index, disk.index_count, header.index_count) ||
            !SpanFits(disk.first_height, disk.height_count, header.height_count))
        {
            return AssetError("physics shape record is invalid");
        }

        PhysicsShape &decoded = shapes[index];
        if (!DecodeShapeType(disk.type, decoded.type))
        {
            return AssetError("physics shape type is unsupported");
        }
        decoded.local_position = {disk.local_position[0], disk.local_position[1], disk.local_position[2]};
        decoded.local_rotation = {disk.local_rotation[3], disk.local_rotation[0], disk.local_rotation[1],
                                  disk.local_rotation[2]};
        decoded.half_extents = {disk.half_extents[0], disk.half_extents[1], disk.half_extents[2]};
        decoded.radius = disk.radius;
        decoded.half_height = disk.half_height;
        decoded.samples_per_side = disk.samples_per_side;
        decoded.height_field_offset = {disk.height_field_offset[0], disk.height_field_offset[1],
                                       disk.height_field_offset[2]};
        decoded.height_field_scale = {disk.height_field_scale[0], disk.height_field_scale[1],
                                      disk.height_field_scale[2]};
        if (!Finite(decoded.local_position) || !std::isfinite(decoded.local_rotation.x) ||
            !std::isfinite(decoded.local_rotation.y) || !std::isfinite(decoded.local_rotation.z) ||
            !std::isfinite(decoded.local_rotation.w))
        {
            return AssetError("physics shape transform is invalid");
        }

        decoded.vertices.reserve(disk.vertex_count);
        std::size_t vertex_offset =
            static_cast<std::size_t>(header.vertex_offset) + (static_cast<std::size_t>(disk.first_vertex) * 12u);
        for (std::uint32_t vertex = 0; vertex < disk.vertex_count; ++vertex)
        {
            glm::vec3 point;
            if (!ReadF32LE(payload, vertex_offset, point.x) || !ReadF32LE(payload, vertex_offset, point.y) ||
                !ReadF32LE(payload, vertex_offset, point.z) || !Finite(point))
            {
                return AssetError("physics vertex buffer is invalid");
            }
            decoded.vertices.push_back(point);
        }

        decoded.indices.resize(disk.index_count);
        std::size_t index_offset =
            static_cast<std::size_t>(header.index_offset) + (static_cast<std::size_t>(disk.first_index) * 4u);
        for (std::uint32_t &vertex : decoded.indices)
        {
            if (!ReadU32LE(payload, index_offset, vertex) || vertex >= disk.vertex_count)
            {
                return AssetError("physics index buffer is invalid");
            }
        }

        decoded.heights.resize(disk.height_count);
        std::size_t height_offset =
            static_cast<std::size_t>(header.height_offset) + (static_cast<std::size_t>(disk.first_height) * 4u);
        for (float &height : decoded.heights)
        {
            if (!ReadF32LE(payload, height_offset, height) || !std::isfinite(height))
            {
                return AssetError("physics height buffer is invalid");
            }
        }

        if (index != 0)
        {
            children[disk.parent].push_back(index);
        }
    }

    std::function<PhysicsShape(std::uint32_t)> build = [&](std::uint32_t index) {
        PhysicsShape result = std::move(shapes[index]);
        for (std::uint32_t child : children[index])
        {
            result.children.push_back(build(child));
        }
        return result;
    };
    shape = build(0);
    if (!ValidShape(shape))
    {
        shape = {};
        return AssetError("physics shape geometry is invalid");
    }
    return true;
}

} // namespace CEngine::Assets
