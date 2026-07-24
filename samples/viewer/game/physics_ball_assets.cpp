#include "game/physics_ball_assets.h"

#include "renderer/material.h"
#include "renderer/mesh.h"
#include "renderer/particle_emitter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <utility>

namespace Viewer
{
namespace
{

using CEngine::Renderer::Material;
using CEngine::Renderer::Mesh;
using CEngine::Renderer::MeshLod;
using CEngine::Renderer::MeshVertex;

void FinishMesh(Mesh &mesh)
{
    glm::vec3 minimum(std::numeric_limits<float>::max());
    glm::vec3 maximum(std::numeric_limits<float>::lowest());
    for (const MeshVertex &vertex : mesh.lods.front().vertices)
    {
        minimum = glm::min(minimum, vertex.position);
        maximum = glm::max(maximum, vertex.position);
    }
    mesh.local_bounds = {
        .min = minimum,
        .max = maximum,
        .valid = true,
    };
}

std::shared_ptr<const Mesh> MakeSphereMesh(std::string name, bool markings)
{
    constexpr std::uint32_t Stacks = 20;
    constexpr std::uint32_t Segments = 32;
    constexpr float MarkingRadius = 1.012f;
    constexpr float EquatorHalfWidth = 0.17f;
    constexpr float LongitudeHalfWidth = 0.12f;
    constexpr float PoleMarkRadius = 0.27f;
    constexpr float Pi = std::numbers::pi_v<float>;

    auto mesh = std::make_shared<Mesh>();
    mesh->name = std::move(name);
    mesh->lods.emplace_back();
    MeshLod &lod = mesh->lods.front();
    lod.vertices.reserve(static_cast<std::size_t>(Stacks + 1u) * (Segments + 1u));

    const float radius = markings ? MarkingRadius : 1.0f;
    for (std::uint32_t stack = 0; stack <= Stacks; ++stack)
    {
        const float theta = Pi * static_cast<float>(stack) / static_cast<float>(Stacks);
        const float ring_radius = std::sin(theta);
        const float z = std::cos(theta);
        for (std::uint32_t segment = 0; segment <= Segments; ++segment)
        {
            const float phi = 2.0f * Pi * static_cast<float>(segment) / static_cast<float>(Segments);
            const glm::vec3 normal(ring_radius * std::cos(phi), ring_radius * std::sin(phi), z);
            MeshVertex vertex;
            vertex.position = normal * radius;
            vertex.normal = normal;
            vertex.tangent = {-std::sin(phi), std::cos(phi), 0.0f};
            vertex.uv = {
                static_cast<float>(segment) / static_cast<float>(Segments),
                static_cast<float>(stack) / static_cast<float>(Stacks),
            };
            lod.vertices.push_back(vertex);
        }
    }

    lod.indices.reserve(static_cast<std::size_t>(Stacks) * Segments * 6u);
    for (std::uint32_t stack = 0; stack < Stacks; ++stack)
    {
        const float theta = Pi * (static_cast<float>(stack) + 0.5f) / static_cast<float>(Stacks);
        for (std::uint32_t segment = 0; segment < Segments; ++segment)
        {
            const float phi = 2.0f * Pi * (static_cast<float>(segment) + 0.5f) / static_cast<float>(Segments);
            const bool marked = std::abs(theta - (0.5f * Pi)) < EquatorHalfWidth ||
                                std::abs(std::sin(phi)) < LongitudeHalfWidth || theta < PoleMarkRadius;
            if (markings && !marked)
            {
                continue;
            }

            const std::uint32_t top_left = (stack * (Segments + 1u)) + segment;
            const std::uint32_t bottom_left = top_left + Segments + 1u;
            const std::uint32_t top_right = top_left + 1u;
            const std::uint32_t bottom_right = bottom_left + 1u;
            lod.indices.insert(lod.indices.end(),
                               {top_left, bottom_left, top_right, top_right, bottom_left, bottom_right});
        }
    }
    FinishMesh(*mesh);
    return mesh;
}

void AppendBox(MeshLod &lod, const glm::vec3 &minimum, const glm::vec3 &maximum)
{
    struct Face
    {
        glm::vec3 normal;
        std::array<glm::vec3, 4> corners;
    };
    const std::array<Face, 6> faces = {{
        {
            .normal = {1.0f, 0.0f, 0.0f},
            .corners = {{{maximum.x, minimum.y, minimum.z},
                         {maximum.x, maximum.y, minimum.z},
                         {maximum.x, maximum.y, maximum.z},
                         {maximum.x, minimum.y, maximum.z}}},
        },
        {
            .normal = {-1.0f, 0.0f, 0.0f},
            .corners = {{{minimum.x, maximum.y, minimum.z},
                         {minimum.x, minimum.y, minimum.z},
                         {minimum.x, minimum.y, maximum.z},
                         {minimum.x, maximum.y, maximum.z}}},
        },
        {
            .normal = {0.0f, 1.0f, 0.0f},
            .corners = {{{maximum.x, maximum.y, minimum.z},
                         {minimum.x, maximum.y, minimum.z},
                         {minimum.x, maximum.y, maximum.z},
                         {maximum.x, maximum.y, maximum.z}}},
        },
        {
            .normal = {0.0f, -1.0f, 0.0f},
            .corners = {{{minimum.x, minimum.y, minimum.z},
                         {maximum.x, minimum.y, minimum.z},
                         {maximum.x, minimum.y, maximum.z},
                         {minimum.x, minimum.y, maximum.z}}},
        },
        {
            .normal = {0.0f, 0.0f, 1.0f},
            .corners = {{{minimum.x, minimum.y, maximum.z},
                         {maximum.x, minimum.y, maximum.z},
                         {maximum.x, maximum.y, maximum.z},
                         {minimum.x, maximum.y, maximum.z}}},
        },
        {
            .normal = {0.0f, 0.0f, -1.0f},
            .corners = {{{minimum.x, maximum.y, minimum.z},
                         {maximum.x, maximum.y, minimum.z},
                         {maximum.x, minimum.y, minimum.z},
                         {minimum.x, minimum.y, minimum.z}}},
        },
    }};
    constexpr std::array<glm::vec2, 4> Uvs = {
        glm::vec2(0.0f, 0.0f),
        glm::vec2(1.0f, 0.0f),
        glm::vec2(1.0f, 1.0f),
        glm::vec2(0.0f, 1.0f),
    };
    for (const Face &face : faces)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());
        const glm::vec3 tangent = glm::normalize(face.corners[1] - face.corners[0]);
        for (std::size_t index = 0; index < face.corners.size(); ++index)
        {
            MeshVertex vertex;
            vertex.position = face.corners[index];
            vertex.normal = face.normal;
            vertex.tangent = tangent;
            vertex.uv = Uvs[index];
            lod.vertices.push_back(vertex);
        }
        lod.indices.insert(lod.indices.end(), {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
    }
}

void AppendCylinder(MeshLod &lod, float minimum_x, float maximum_x, float radius)
{
    constexpr std::uint32_t Segments = 20;
    constexpr float Pi = std::numbers::pi_v<float>;
    const auto side_base = static_cast<std::uint32_t>(lod.vertices.size());
    for (std::uint32_t segment = 0; segment <= Segments; ++segment)
    {
        const float angle = 2.0f * Pi * static_cast<float>(segment) / static_cast<float>(Segments);
        const glm::vec3 normal(0.0f, std::cos(angle), std::sin(angle));
        for (float x : {minimum_x, maximum_x})
        {
            MeshVertex vertex;
            vertex.position = {x, normal.y * radius, normal.z * radius};
            vertex.normal = normal;
            vertex.tangent = {1.0f, 0.0f, 0.0f};
            vertex.uv = {
                static_cast<float>(segment) / static_cast<float>(Segments),
                x == minimum_x ? 0.0f : 1.0f,
            };
            lod.vertices.push_back(vertex);
        }
    }
    for (std::uint32_t segment = 0; segment < Segments; ++segment)
    {
        const std::uint32_t left = side_base + (segment * 2u);
        const std::uint32_t right = left + 2u;
        lod.indices.insert(lod.indices.end(), {left, right, left + 1u, left + 1u, right, right + 1u});
    }

    for (const auto [x, normal] :
         {std::pair{minimum_x, glm::vec3(-1.0f, 0.0f, 0.0f)}, std::pair{maximum_x, glm::vec3(1.0f, 0.0f, 0.0f)}})
    {
        const auto center = static_cast<std::uint32_t>(lod.vertices.size());
        MeshVertex center_vertex;
        center_vertex.position = {x, 0.0f, 0.0f};
        center_vertex.normal = normal;
        lod.vertices.push_back(center_vertex);
        for (std::uint32_t segment = 0; segment <= Segments; ++segment)
        {
            const float angle = 2.0f * Pi * static_cast<float>(segment) / static_cast<float>(Segments);
            MeshVertex vertex;
            vertex.position = {x, std::cos(angle) * radius, std::sin(angle) * radius};
            vertex.normal = normal;
            vertex.uv = {
                0.5f + (0.5f * std::cos(angle)),
                0.5f + (0.5f * std::sin(angle)),
            };
            lod.vertices.push_back(vertex);
        }
        for (std::uint32_t segment = 0; segment < Segments; ++segment)
        {
            const std::uint32_t first = center + 1u + segment;
            if (normal.x < 0.0f)
            {
                lod.indices.insert(lod.indices.end(), {center, first + 1u, first});
            }
            else
            {
                lod.indices.insert(lod.indices.end(), {center, first, first + 1u});
            }
        }
    }
}

std::shared_ptr<const Mesh> MakeLauncherMesh()
{
    auto mesh = std::make_shared<Mesh>();
    mesh->name = "PhysicsBallLauncher";
    mesh->lods.emplace_back();
    MeshLod &lod = mesh->lods.front();
    AppendBox(lod, {-0.20f, -0.10f, -0.11f}, {0.18f, 0.10f, 0.11f});
    AppendBox(lod, {-0.16f, -0.075f, -0.34f}, {0.03f, 0.075f, -0.08f});
    AppendCylinder(lod, 0.10f, 0.54f, 0.072f);
    FinishMesh(*mesh);
    return mesh;
}

std::shared_ptr<const Mesh> MakeLauncherAccentMesh()
{
    auto mesh = std::make_shared<Mesh>();
    mesh->name = "PhysicsBallLauncherAccent";
    mesh->lods.emplace_back();
    AppendCylinder(mesh->lods.front(), 0.50f, 0.58f, 0.092f);
    FinishMesh(*mesh);
    return mesh;
}

std::shared_ptr<const Material> MakeMaterial(std::string name, const glm::vec4 &color, float metallic, float roughness)
{
    auto material = std::make_shared<Material>();
    material->material_name = std::move(name);
    material->base_color_factor = color;
    material->metallic_roughness_ao_factors = {metallic, roughness, 1.0f};
    return material;
}

std::shared_ptr<const CEngine::Assets::Particle> MakeMuzzleParticles()
{
    auto particles = std::make_shared<CEngine::Assets::Particle>();
    particles->name = "PhysicsBallMuzzle";
    particles->flags = CEngine::Renderer::ParticleFlagWorldSpace;
    particles->blend_mode = static_cast<std::uint32_t>(CEngine::Renderer::ParticleBlendMode::Additive);
    particles->lifetime = {0.06f, 0.14f};
    particles->speed = {0.9f, 2.8f};
    particles->spread = 0.45f;
    particles->gravity = {0.0f, 0.0f, -1.2f};
    particles->size = {0.24f, 0.035f};
    particles->start_color = {1.0f, 0.42f, 0.06f, 1.0f};
    particles->end_color = {1.0f, 0.03f, 0.0f, 0.0f};
    return particles;
}

PhysicsBallAssets MakeAssets()
{
    PhysicsBallAssets assets;
    assets.ball = MakeSphereMesh("PhysicsBall", false);
    assets.ball_markings = MakeSphereMesh("PhysicsBallMarkings", true);
    assets.launcher = MakeLauncherMesh();
    assets.launcher_accent = MakeLauncherAccentMesh();
    assets.ball_material = MakeMaterial("PhysicsBallOrange", {0.95f, 0.20f, 0.035f, 1.0f}, 0.12f, 0.34f);
    assets.marking_material = MakeMaterial("PhysicsBallGraphite", {0.018f, 0.026f, 0.035f, 1.0f}, 0.35f, 0.26f);
    assets.launcher_material = MakeMaterial("LauncherGraphite", {0.045f, 0.060f, 0.075f, 1.0f}, 0.65f, 0.30f);
    assets.accent_material = assets.ball_material;
    assets.muzzle_particles = MakeMuzzleParticles();
    return assets;
}

} // namespace

const PhysicsBallAssets &GetPhysicsBallAssets()
{
    static const PhysicsBallAssets Assets = MakeAssets();
    return Assets;
}

} // namespace Viewer
