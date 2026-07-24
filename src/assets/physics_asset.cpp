//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/physics_asset.cpp
 * @brief Instantiates collision geometry from generated wire data.
 * @author Erik Coltey
 */

#include "assets/physics_asset.h"

#include "engine/engine_entities.generated.h"
#include "log.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace CEngine::Assets
{
namespace
{
namespace Wire = Generated::EngineEntities::Wire;

bool Finite(const glm::vec3 &value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool Valid(const PhysicsShape &shape, std::uint32_t depth = 0)
{
    if (depth > 8 || !Finite(shape.local_position) || !std::isfinite(shape.local_rotation.x) ||
        !std::isfinite(shape.local_rotation.y) || !std::isfinite(shape.local_rotation.z) ||
        !std::isfinite(shape.local_rotation.w) || glm::dot(shape.local_rotation, shape.local_rotation) <= 1.0e-12f)
    {
        return false;
    }
    const bool empty =
        shape.vertices.empty() && shape.indices.empty() && shape.heights.empty() && shape.samples_per_side == 0;
    switch (shape.type)
    {
    case PhysicsShapeType::Box:
        return Finite(shape.half_extents) && shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               shape.half_extents.z > 0.0f && empty && shape.children.empty();
    case PhysicsShapeType::Sphere:
        return shape.radius > 0.0f && empty && shape.children.empty();
    case PhysicsShapeType::Capsule:
    case PhysicsShapeType::Cylinder:
        return shape.radius > 0.0f && shape.half_height > 0.0f && empty && shape.children.empty();
    case PhysicsShapeType::Plane:
        return Finite(shape.half_extents) && shape.half_extents.x > 0.0f && shape.half_extents.y > 0.0f &&
               empty && shape.children.empty();
    case PhysicsShapeType::ConvexHull:
        return shape.vertices.size() >= 4 && shape.indices.empty() && shape.children.empty();
    case PhysicsShapeType::TriangleMesh:
        return shape.vertices.size() >= 3 && !shape.indices.empty() && shape.indices.size() % 3u == 0u &&
               shape.children.empty();
    case PhysicsShapeType::HeightField: {
        const std::size_t side = shape.samples_per_side;
        return side >= 2 && shape.heights.size() == side * side && shape.height_field_scale.x > 0.0f &&
               shape.height_field_scale.y > 0.0f && shape.height_field_scale.z > 0.0f && shape.children.empty();
    }
    case PhysicsShapeType::Compound:
        return empty && !shape.children.empty() &&
               std::all_of(shape.children.begin(), shape.children.end(),
                           [depth](const PhysicsShape &child) { return Valid(child, depth + 1u); });
    }
    return false;
}
} // namespace

bool LoadPhysicsAsset(const std::filesystem::path &path, PhysicsShape &shape)
{
    Wire::Physics decoded;
    if (!Decode(path, Type::Physics, decoded))
    {
        Logging::Logger::Get().Error("assets", "physics payload is invalid");
        return false;
    }

    std::vector<PhysicsShape> nodes(decoded.nodes.size());
    std::vector<std::vector<std::uint32_t>> children(decoded.nodes.size());
    for (std::size_t index = 0; index < decoded.nodes.size(); ++index)
    {
        const Wire::PhysicsNode &source = decoded.nodes[index];
        if ((index == 0u && source.parent != -1) ||
            (index != 0u && (source.parent < 0 || static_cast<std::size_t>(source.parent) >= index)) ||
            source.vertices.size() % 3u != 0u)
        {
            Logging::Logger::Get().Error("assets", "physics hierarchy is invalid");
            return false;
        }
        PhysicsShape &target = nodes[index];
        target.type = static_cast<PhysicsShapeType>(source.type);
        target.local_position = {source.position[0], source.position[1], source.position[2]};
        target.local_rotation = {source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]};
        target.half_extents = {source.half_extents[0], source.half_extents[1], source.half_extents[2]};
        target.radius = source.radius;
        target.half_height = source.half_height;
        target.indices = source.indices;
        target.heights = source.heights;
        target.samples_per_side = source.samples_per_side;
        target.height_field_offset = {source.height_offset[0], source.height_offset[1], source.height_offset[2]};
        target.height_field_scale = {source.height_scale[0], source.height_scale[1], source.height_scale[2]};
        target.vertices.reserve(source.vertices.size() / 3u);
        for (std::size_t component = 0; component < source.vertices.size(); component += 3u)
        {
            target.vertices.emplace_back(source.vertices[component], source.vertices[component + 1u],
                                         source.vertices[component + 2u]);
        }
        if (index != 0u)
        {
            children[static_cast<std::size_t>(source.parent)].push_back(static_cast<std::uint32_t>(index));
        }
    }

    std::function<PhysicsShape(std::uint32_t)> build = [&](std::uint32_t index) {
        PhysicsShape result = std::move(nodes[index]);
        for (std::uint32_t child : children[index])
        {
            result.children.push_back(build(child));
        }
        return result;
    };
    PhysicsShape loaded = build(0);
    if (!Valid(loaded))
    {
        Logging::Logger::Get().Error("assets", "physics shape geometry is invalid");
        return false;
    }
    shape = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
