//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/mesh.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef MESH_H
#define MESH_H

#include <cstdint>
#include <array>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe Bounds.
 */
struct Bounds
{
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
    bool valid = false;
};

/**
 * @brief TODO: Describe MeshVertex.
 */
struct MeshVertex
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec2 uv = glm::vec2(0.0f);
    glm::vec2 lightmap_uv = glm::vec2(0.0f);
    glm::vec3 tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    std::array<std::uint16_t, 4> joints = {};
    glm::vec4 weights = glm::vec4(0.0f);
};

struct MeshLod
{
    float screen_size = 1.0f;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

/**
 * @brief TODO: Describe Mesh.
 */
struct Mesh
{
    std::string name;
    std::vector<MeshLod> lods;
    std::array<std::uint8_t, 16> skeleton_guid = {};
    bool skinned = false;
    bool has_lightmap_uv = false;
    Bounds local_bounds;
    /**
     * @brief TODO: Describe Empty.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool Empty() const
    {
        return lods.empty() || lods.front().vertices.empty() || lods.front().indices.empty();
    }

    [[nodiscard]] const MeshLod *Lod(float screen_size) const
    {
        if (lods.empty())
        {
            return nullptr;
        }
        for (const MeshLod &lod : lods)
        {
            if (screen_size >= lod.screen_size)
            {
                return &lod;
            }
        }
        return &lods.back();
    }
};

} // namespace CEngine::Renderer

#endif
