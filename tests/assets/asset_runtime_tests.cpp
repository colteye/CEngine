//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/assets/asset_runtime_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/asset_store.h"
#include "assets/casset_loader.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "assets/skeleton_loader.h"
#include "test_asset_writer.h"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using namespace CEngine::Assets;
namespace Renderer = CEngine::Renderer;

constexpr std::array<char, 4> MeshMetadataMagic = {'C', 'E', 'M', 'H'};
constexpr std::uint16_t MeshMetadataVersion = 2;
constexpr std::uint32_t MeshFlagSkinned = 1u << 0u;
constexpr std::uint32_t MeshFlagLightmapUv = 1u << 1u;
constexpr std::array<char, 4> MaterialPayloadMagic = {'C', 'E', 'M', 'A'};
constexpr std::uint16_t MaterialPayloadVersion = 4;

#pragma pack(push, 1)
/**
 * @brief TODO: Describe DiskMaterialHeader.
 */
struct DiskMaterialHeader
{
    std::array<char, 4> magic = MaterialPayloadMagic;
    std::uint16_t version = MaterialPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t shader = 0;
    std::uint32_t render_mode = 0;
    std::uint32_t texture_count = 0;
    std::uint32_t texture_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
    float base_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 0.5f;
    float ao_factor = 1.0f;
    float alpha_cutoff = 0.5f;
};

/**
 * @brief TODO: Describe DiskMaterialTexture.
 */
struct DiskMaterialTexture
{
    std::uint32_t slot = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};

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

/**
 * @brief TODO: Describe Expect.
 *
 * @param condition TODO: Describe this parameter.
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

/**
 * @brief TODO: Describe TestRoot.
 *
 * @return TODO: Describe the return value.
 */
std::filesystem::path TestRoot()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::current_path() / "build" / "asset-tests" / ("runtime_" + std::to_string(now));
}

/**
 * @brief TODO: Describe Bytes.
 *
 * @param text TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::vector<std::uint8_t> Bytes(std::string_view text)
{
    return {text.begin(), text.end()};
}

/**
 * @brief TODO: Describe AppendString.
 *
 * @param bytes TODO: Describe this parameter.
 * @param text TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::uint32_t AppendString(std::vector<std::uint8_t> &bytes, std::string_view text)
{
    const auto offset = static_cast<std::uint32_t>(bytes.size());
    bytes.insert(bytes.end(), text.begin(), text.end());
    return offset;
}

/**
 * @brief TODO: Describe AppendStruct.
 *
 * @tparam T TODO: Describe this template parameter.
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 */
template <typename T> void AppendStruct(std::vector<std::uint8_t> &bytes, const T &value)
{
    const auto *data = reinterpret_cast<const std::uint8_t *>(&value);
    bytes.insert(bytes.end(), data, data + sizeof(T));
}

/**
 * @brief TODO: Describe AppendF32.
 *
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 */
void AppendF32(std::vector<std::uint8_t> &bytes, float value)
{
    AppendStruct(bytes, value);
}

/**
 * @brief TODO: Describe AppendU32.
 *
 * @param bytes TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 */
void AppendU32(std::vector<std::uint8_t> &bytes, std::uint32_t value)
{
    AppendStruct(bytes, value);
}

/**
 * @brief TODO: Describe WriteDdsStub.
 *
 * @param path TODO: Describe this parameter.
 */
void WriteDdsStub(const std::filesystem::path &path)
{
    std::vector<std::uint8_t> bytes(128 + 8);
    const auto write_u32 = [&](std::size_t offset, std::uint32_t value) {
        for (std::size_t byte = 0; byte < 4; ++byte)
        {
            bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
        }
    };
    write_u32(0, 0x20534444u);
    write_u32(4, 124);
    write_u32(12, 4);
    write_u32(16, 4);
    write_u32(76, 32);
    write_u32(84, 0x31545844u);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

/**
 * @brief TODO: Describe AppendStaticVertex.
 *
 * @param bytes TODO: Describe this parameter.
 * @param x TODO: Describe this parameter.
 * @param y TODO: Describe this parameter.
 * @param z TODO: Describe this parameter.
 * @param nx TODO: Describe this parameter.
 * @param ny TODO: Describe this parameter.
 * @param nz TODO: Describe this parameter.
 * @param u TODO: Describe this parameter.
 * @param v TODO: Describe this parameter.
 */
void AppendStaticVertex(std::vector<std::uint8_t> &bytes, float x, float y, float z, float nx, float ny, float nz,
                        float u, float v)
{
    AppendF32(bytes, x);
    AppendF32(bytes, y);
    AppendF32(bytes, z);
    AppendF32(bytes, nx);
    AppendF32(bytes, ny);
    AppendF32(bytes, nz);
    AppendF32(bytes, u);
    AppendF32(bytes, v);
}

/**
 * @brief TODO: Describe AppendSkinnedStaticVertex.
 *
 * @param bytes TODO: Describe this parameter.
 * @param x TODO: Describe this parameter.
 * @param y TODO: Describe this parameter.
 * @param z TODO: Describe this parameter.
 * @param nx TODO: Describe this parameter.
 * @param ny TODO: Describe this parameter.
 * @param nz TODO: Describe this parameter.
 * @param u TODO: Describe this parameter.
 * @param v TODO: Describe this parameter.
 */
void AppendSkinnedStaticVertex(std::vector<std::uint8_t> &bytes, float x, float y, float z, float nx, float ny,
                               float nz, float u, float v)
{
    AppendStaticVertex(bytes, x, y, z, nx, ny, nz, u, v);
    AppendF32(bytes, 0.0f);
    AppendF32(bytes, 0.0f);
    for (int index = 0; index < 8; ++index)
    {
        const std::uint16_t value = 0;
        AppendStruct(bytes, value);
    }
}

/**
 * @brief TODO: Describe AppendLightmappedVertex.
 *
 * @param bytes TODO: Describe this parameter.
 * @param x TODO: Describe this parameter.
 * @param y TODO: Describe this parameter.
 * @param z TODO: Describe this parameter.
 * @param nx TODO: Describe this parameter.
 * @param ny TODO: Describe this parameter.
 * @param nz TODO: Describe this parameter.
 * @param u TODO: Describe this parameter.
 * @param v TODO: Describe this parameter.
 * @param lightmap_u TODO: Describe this parameter.
 * @param lightmap_v TODO: Describe this parameter.
 */
void AppendLightmappedVertex(std::vector<std::uint8_t> &bytes, float x, float y, float z, float nx, float ny, float nz,
                             float u, float v, float lightmap_u, float lightmap_v)
{
    AppendStaticVertex(bytes, x, y, z, nx, ny, nz, u, v);
    AppendF32(bytes, lightmap_u);
    AppendF32(bytes, lightmap_v);
}

/**
 * @brief TODO: Describe StaticTriangleGeometry.
 *
 * @return TODO: Describe the return value.
 */
std::vector<std::uint8_t> StaticTriangleGeometry()
{
    std::vector<std::uint8_t> geometry;
    geometry.reserve((3 * 40) + (3 * 4));
    AppendLightmappedVertex(geometry, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    AppendLightmappedVertex(geometry, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    AppendLightmappedVertex(geometry, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    AppendU32(geometry, 0);
    AppendU32(geometry, 1);
    AppendU32(geometry, 2);
    return geometry;
}

/**
 * @brief TODO: Describe SkinnedTriangleGeometry.
 *
 * @return TODO: Describe the return value.
 */
std::vector<std::uint8_t> SkinnedTriangleGeometry()
{
    std::vector<std::uint8_t> geometry;
    geometry.reserve((3 * 56) + (3 * 4));
    AppendSkinnedStaticVertex(geometry, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    AppendSkinnedStaticVertex(geometry, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    AppendSkinnedStaticVertex(geometry, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    AppendU32(geometry, 0);
    AppendU32(geometry, 1);
    AppendU32(geometry, 2);
    return geometry;
}

/**
 * @brief TODO: Describe LightmappedTriangleGeometry.
 *
 * @return TODO: Describe the return value.
 */
std::vector<std::uint8_t> LightmappedTriangleGeometry()
{
    std::vector<std::uint8_t> geometry;
    geometry.reserve((3 * 40) + (3 * 4));
    AppendLightmappedVertex(geometry, 0, 0, 0, 0, 0, 1, 0, 0, 0.25f, 0.25f);
    AppendLightmappedVertex(geometry, 1, 0, 0, 0, 0, 1, 1, 0, 0.75f, 0.25f);
    AppendLightmappedVertex(geometry, 0, 2, 0, 0, 0, 1, 0, 1, 0.25f, 0.75f);
    AppendU32(geometry, 0);
    AppendU32(geometry, 1);
    AppendU32(geometry, 2);
    return geometry;
}

/**
 * @brief TODO: Describe StaticMeshPayload.
 *
 * @param flags TODO: Describe this parameter.
 * @param vertex_stride TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::vector<std::uint8_t> StaticMeshPayload(std::uint32_t flags = 0, std::uint32_t vertex_stride = 40)
{
    DiskMeshMetadata mesh_metadata;
    mesh_metadata.header_size = sizeof(DiskMeshMetadata);
    mesh_metadata.flags = flags;
    mesh_metadata.vertex_count = 3;
    mesh_metadata.index_count = 3;
    mesh_metadata.vertex_stride = vertex_stride;
    mesh_metadata.index_size = 4;
    mesh_metadata.bounds_max[0] = 1.0f;
    mesh_metadata.bounds_max[1] = 2.0f;
    mesh_metadata.material_slot_count = 1;
    mesh_metadata.geometry_offset = sizeof(DiskMeshMetadata);

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, mesh_metadata);
    std::vector<std::uint8_t> geometry;
    if (vertex_stride == 56)
    {
        geometry = SkinnedTriangleGeometry();
    }
    else if ((flags & MeshFlagLightmapUv) != 0)
    {
        geometry = LightmappedTriangleGeometry();
    }
    else
    {
        geometry = StaticTriangleGeometry();
    }
    payload.insert(payload.end(), geometry.begin(), geometry.end());
    return payload;
}

/**
 * @brief TODO: Describe CommonAssetPayloadRemainsDirectlyAddressable.
 *
 * @return TODO: Describe the return value.
 */
bool CommonAssetPayloadRemainsDirectlyAddressable()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path material_path = root / "assets" / "compiled" / "hero" / "materials" / "Hero.cmat";
    const std::string material_key = "assets/compiled/hero/materials/Hero.cmat";

    const std::vector<std::uint8_t> material_payload = Bytes("binary material payload");

    if (!Expect(CEngine::Tests::WriteTestAsset(material_path, AssetType::Material, material_payload,
                                               CEngine::Tests::TestGuid(material_key), 7),
                "material asset should write"))
    {
        return false;
    }

    AssetFile loaded;
    if (!Expect(loaded.Load(material_path), "material asset should load"))
    {
        return false;
    }
    const ByteView payload = loaded.Payload();
    if (!Expect(payload.size == material_payload.size(), "asset payload should preserve its byte size"))
    {
        return false;
    }
    if (!Expect(std::memcmp(payload.data, material_payload.data(), material_payload.size()) == 0,
                "asset payload should stay byte-exact"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe MeshLoaderBuildsRendererMeshFromTargetAsset.
 *
 * @return TODO: Describe the return value.
 */
bool MeshLoaderBuildsRendererMeshFromTargetAsset()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path mesh_path = root / "assets" / "compiled" / "hero" / "meshes" / "SM_Body.cmesh";
    const std::string mesh_key = "assets/compiled/hero/meshes/SM_Body.cmesh";
    const std::vector<std::uint8_t> mesh_payload = StaticMeshPayload();

    if (!Expect(CEngine::Tests::WriteTestAsset(mesh_path, AssetType::Mesh, mesh_payload,
                                               CEngine::Tests::TestGuid(mesh_key), 9),
                "mesh asset should write"))
    {
        return false;
    }

    Renderer::Mesh mesh;
    if (!Expect(LoadMeshAsset(mesh_path, mesh), "mesh asset should load"))
    {
        return false;
    }

    const Renderer::Mesh &data = mesh;
    if (!Expect(data.vertices.size() == 3 && data.indices.size() == 3,
                "loaded render mesh should retain indexed geometry"))
    {
        return false;
    }
    if (!Expect(data.vertices[2].position.y == 2.0f, "loaded render mesh should decode vertex positions"))
    {
        return false;
    }
    if (!Expect(data.local_bounds.valid, "loaded render mesh should preserve metadata bounds"))
    {
        return false;
    }

    AssetStore assets(root);
    AssetReference reference;
    if (!Expect(assets.Resolve(mesh_key, AssetType::Mesh, CEngine::Tests::TestGuid(mesh_key), reference),
                "mesh reference should resolve"))
    {
        return false;
    }
    const auto first = assets.LoadMesh(reference);
    const auto second = assets.LoadMesh(reference);
    if (!Expect(first != nullptr && first == second, "asset store should share one immutable mesh payload"))
    {
        return false;
    }
    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe MeshLoaderTreatsSkinnedMeshesAsStaticGeometry.
 *
 * @return TODO: Describe the return value.
 */
bool MeshLoaderTreatsSkinnedMeshesAsStaticGeometry()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path mesh_path = root / "assets" / "compiled" / "hero" / "meshes" / "SK_Body.cmesh";
    const std::string mesh_key = "assets/compiled/hero/meshes/SK_Body.cmesh";
    const std::vector<std::uint8_t> mesh_payload = StaticMeshPayload(MeshFlagSkinned, 56);

    if (!Expect(CEngine::Tests::WriteTestAsset(mesh_path, AssetType::Mesh, mesh_payload,
                                               CEngine::Tests::TestGuid(mesh_key), 13),
                "skinned mesh asset should write"))
    {
        return false;
    }

    Renderer::Mesh mesh;
    if (!Expect(LoadMeshAsset(mesh_path, mesh), "skinned mesh asset should load"))
    {
        return false;
    }
    if (!Expect(!mesh.Empty(), "skinned mesh payloads should load as static render geometry when skinning is ignored"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe MeshLoaderReadsLightmapUvStream.
 *
 * @return TODO: Describe the return value.
 */
bool MeshLoaderReadsLightmapUvStream()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path path = root / "assets" / "compiled" / "maps" / "Floor.cmesh";
    const std::vector<std::uint8_t> mesh_payload = StaticMeshPayload(MeshFlagLightmapUv, 40);
    if (!Expect(CEngine::Tests::WriteTestAsset(path, AssetType::Mesh, mesh_payload,
                                               CEngine::Tests::TestGuid("assets/compiled/maps/Floor.cmesh")),
                "lightmap mesh asset should write"))
    {
        return false;
    }

    Renderer::Mesh mesh;
    if (!Expect(LoadMeshAsset(path, mesh), "lightmap mesh asset should load"))
    {
        return false;
    }
    const Renderer::Mesh &data = mesh;
    const bool result = Expect(data.has_lightmap_uv, "mesh should retain its lightmap UV flag") &&
                        Expect(data.vertices.size() == 3, "mesh should decode its lightmap UV stream") &&
                        Expect(data.vertices[1].lightmap_uv == glm::vec2(0.75f, 0.25f),
                               "mesh should decode UV1 independently from UV0");
    std::filesystem::remove_all(root);
    return result;
}

/**
 * @brief TODO: Describe MaterialLoaderAllowsNeutralSurfaceSlots.
 *
 * @return TODO: Describe the return value.
 */
bool MaterialLoaderAllowsNeutralSurfaceSlots()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path material_path = root / "assets" / "compiled" / "hero" / "materials" / "Empty.cmat";
    const std::string material_key = "assets/compiled/hero/materials/Empty.cmat";
    const std::string material_name = "Empty";

    std::vector<std::uint8_t> strings;
    const std::uint32_t name_offset = AppendString(strings, material_name);

    DiskMaterialHeader material_header;
    material_header.header_size = sizeof(DiskMaterialHeader);
    material_header.shader = static_cast<std::uint32_t>(Renderer::MaterialShaderType::PBRStandard);
    material_header.texture_count = 0;
    material_header.texture_table_offset = sizeof(DiskMaterialHeader);
    material_header.string_table_offset = sizeof(DiskMaterialHeader);
    material_header.string_table_size = static_cast<std::uint32_t>(strings.size());
    material_header.name_offset = name_offset;
    material_header.name_size = static_cast<std::uint32_t>(material_name.size());
    material_header.base_color[0] = 0.25f;
    material_header.base_color[1] = 0.5f;
    material_header.base_color[2] = 0.75f;
    material_header.metallic_factor = 0.1f;
    material_header.roughness_factor = 0.8f;
    material_header.ao_factor = 0.9f;
    material_header.render_mode = 2;
    material_header.alpha_cutoff = 0.42f;

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, material_header);
    payload.insert(payload.end(), strings.begin(), strings.end());

    if (!Expect(CEngine::Tests::WriteTestAsset(material_path, AssetType::Material, payload,
                                               CEngine::Tests::TestGuid(material_key), 11),
                "neutral material asset should write"))
    {
        return false;
    }

    Renderer::Material material;
    if (!Expect(LoadMaterialAsset(material_path, material), "neutral material asset should load"))
    {
        return false;
    }
    if (!Expect(material.albedo.Empty(),
                "material loader should preserve an empty albedo slot for a neutral GPU fallback"))
    {
        return false;
    }
    if (!Expect(material.normal.Empty(),
                "material loader should preserve an empty normal slot for a flat GPU fallback"))
    {
        return false;
    }
    if (!Expect(material.metallic_roughness_ao.Empty(),
                "material loader should preserve an empty MRA slot when constants are authored"))
    {
        return false;
    }
    if (!Expect(material.base_color_factor == glm::vec4(0.25f, 0.5f, 0.75f, 1.0f),
                "material loader should preserve an authored base-color constant"))
    {
        return false;
    }
    if (!Expect(material.metallic_roughness_ao_factors == glm::vec3(0.1f, 0.8f, 0.9f),
                "material loader should preserve authored metallic, roughness, and AO constants"))
    {
        return false;
    }
    if (!Expect(material.render_mode == Renderer::MaterialRenderMode::AlphaHashDither,
                "material loader should preserve the authored render mode"))
    {
        return false;
    }
    if (!Expect(std::abs(material.alpha_cutoff - 0.42f) < 0.0001f,
                "material loader should preserve the authored alpha cutoff"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe MaterialLoaderResolvesProjectTexturePaths.
 *
 * @return TODO: Describe the return value.
 */
bool MaterialLoaderResolvesProjectTexturePaths()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path material_path = root / "assets" / "compiled" / "gallery" / "materials" / "Stone.cmat";
    const std::filesystem::path texture_path = root / "assets" / "compiled" / "gallery" / "textures" / "Stone.dds";
    const std::string material_key = "assets/compiled/gallery/materials/Stone.cmat";
    const std::string material_name = "Stone";
    const std::string stored_texture_path = "assets/compiled/gallery/textures/Stone.dds";
    WriteDdsStub(texture_path);

    std::vector<std::uint8_t> strings;
    const std::uint32_t name_offset = AppendString(strings, material_name);
    const std::uint32_t texture_offset = AppendString(strings, stored_texture_path);

    DiskMaterialHeader material_header;
    material_header.header_size = sizeof(DiskMaterialHeader);
    material_header.shader = static_cast<std::uint32_t>(Renderer::MaterialShaderType::PBRStandard);
    material_header.texture_count = 1;
    material_header.texture_table_offset = sizeof(DiskMaterialHeader);
    material_header.string_table_offset = sizeof(DiskMaterialHeader) + sizeof(DiskMaterialTexture);
    material_header.string_table_size = static_cast<std::uint32_t>(strings.size());
    material_header.name_offset = name_offset;
    material_header.name_size = static_cast<std::uint32_t>(material_name.size());

    DiskMaterialTexture texture;
    texture.slot = 1;
    texture.path_offset = texture_offset;
    texture.path_size = static_cast<std::uint32_t>(stored_texture_path.size());
    std::vector<std::uint8_t> payload;
    AppendStruct(payload, material_header);
    AppendStruct(payload, texture);
    payload.insert(payload.end(), strings.begin(), strings.end());

    if (!Expect(CEngine::Tests::WriteTestAsset(material_path, AssetType::Material, payload,
                                               CEngine::Tests::TestGuid(material_key), 19),
                "textured material asset should write"))
    {
        return false;
    }

    Renderer::Material material;
    if (!Expect(LoadMaterialAsset(material_path, material), "textured material asset should load"))
    {
        return false;
    }
    if (!Expect(!material.albedo.Empty() && material.albedo.format == Renderer::TextureFormat::Dxt1 &&
                    material.albedo.mips.size() == 1 && material.albedo.mips.front().width == 4 &&
                    material.albedo.mips.front().height == 4,
                "material loader should load project-relative DDS texture data"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe CAssetLoaderReadsBinaryComposition.
 *
 * @return TODO: Describe the return value.
 */
bool CAssetLoaderReadsBinaryComposition()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path casset_path = root / "assets" / "compiled" / "statue" / "Statue.casset";
    const std::string casset_key = "assets/compiled/statue/Statue.casset";
    const std::string source_path = "assets/source/statue.blend";
    const std::string collection_name = "PREFAB_Statue";
    const std::string object_name = "SM_Statue";
    const std::string mesh_path = "assets/compiled/statue/meshes/SM_Statue.cmesh";

    std::vector<std::uint8_t> strings;
    const std::uint32_t source_offset = AppendString(strings, source_path);
    const std::uint32_t collection_offset = AppendString(strings, collection_name);
    const std::uint32_t object_offset = AppendString(strings, object_name);
    const std::uint32_t mesh_offset = AppendString(strings, mesh_path);

    DiskCAssetObject object;
    object.name_offset = object_offset;
    object.name_size = static_cast<std::uint32_t>(object_name.size());
    object.role = 1;
    object.object_type = 1;
    object.first_component = 0;
    object.component_count = 1;
    object.world_from_local[0] = 1.0f;
    object.world_from_local[5] = 1.0f;
    object.world_from_local[10] = 1.0f;
    object.world_from_local[15] = 1.0f;

    DiskCAssetComponent component;
    component.kind = static_cast<std::uint32_t>(CAssetComponentKind::Mesh);
    component.path_offset = mesh_offset;
    component.path_size = static_cast<std::uint32_t>(mesh_path.size());

    DiskCAssetHeader casset_header;
    casset_header.header_size = sizeof(DiskCAssetHeader);
    casset_header.composition_type = static_cast<std::uint32_t>(CAssetCompositionType::Prefab);
    casset_header.object_count = 1;
    casset_header.object_table_offset = sizeof(DiskCAssetHeader);
    casset_header.component_count = 1;
    casset_header.component_table_offset = sizeof(DiskCAssetHeader) + sizeof(DiskCAssetObject);
    casset_header.string_table_offset =
        sizeof(DiskCAssetHeader) + sizeof(DiskCAssetObject) + sizeof(DiskCAssetComponent);
    casset_header.string_table_size = static_cast<std::uint32_t>(strings.size());
    casset_header.source_path_offset = source_offset;
    casset_header.source_path_size = static_cast<std::uint32_t>(source_path.size());
    casset_header.collection_name_offset = collection_offset;
    casset_header.collection_name_size = static_cast<std::uint32_t>(collection_name.size());

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, casset_header);
    AppendStruct(payload, object);
    AppendStruct(payload, component);
    payload.insert(payload.end(), strings.begin(), strings.end());

    if (!Expect(CEngine::Tests::WriteTestAsset(casset_path, AssetType::Asset, payload,
                                               CEngine::Tests::TestGuid(casset_key), 21),
                "composition asset should write"))
    {
        return false;
    }

    CAsset casset;
    if (!Expect(casset.Load(casset_path), "composition asset should load"))
    {
        return false;
    }
    if (!Expect(casset.CompositionType() == CAssetCompositionType::Prefab, "casset should expose composition type"))
    {
        return false;
    }
    if (!Expect(casset.CollectionName() == collection_name, "casset should expose collection name"))
    {
        return false;
    }

    CAssetObject loaded_object;
    if (!Expect(casset.Object(0, loaded_object), "casset should expose object rows"))
    {
        return false;
    }
    if (!Expect(loaded_object.name == object_name, "casset object should expose its name"))
    {
        return false;
    }

    CAssetComponent loaded_component;
    if (!Expect(casset.Component(loaded_object, 0, loaded_component), "casset should expose object component rows"))
    {
        return false;
    }
    if (!Expect(loaded_component.kind == CAssetComponentKind::Mesh, "casset component should expose its kind"))
    {
        return false;
    }
    if (!Expect(loaded_component.path == mesh_path, "casset component should expose target path"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe SkeletonViewReadsBinaryBonesAndNames.
 *
 * @return TODO: Describe the return value.
 */
bool SkeletonViewReadsBinaryBonesAndNames()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path skeleton_path = root / "assets" / "compiled" / "hero" / "skeletons" / "ARM_Hero.cskel";
    const std::string skeleton_key = "assets/compiled/hero/skeletons/ARM_Hero.cskel";
    const std::string armature_name = "ARM_Hero";
    const std::string root_name = "root";
    const std::string child_name = "spine";

    std::vector<std::uint8_t> strings;
    strings.insert(strings.end(), armature_name.begin(), armature_name.end());
    const auto root_name_offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), root_name.begin(), root_name.end());
    const auto child_name_offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), child_name.begin(), child_name.end());

    DiskSkeletonHeader skeleton_header;
    skeleton_header.header_size = sizeof(DiskSkeletonHeader);
    skeleton_header.bone_count = 2;
    skeleton_header.bone_table_offset = sizeof(DiskSkeletonHeader);
    skeleton_header.string_table_offset = sizeof(DiskSkeletonHeader) + (2 * sizeof(DiskSkeletonBone));
    skeleton_header.string_table_size = static_cast<std::uint32_t>(strings.size());
    skeleton_header.armature_name_offset = 0;
    skeleton_header.armature_name_size = static_cast<std::uint32_t>(armature_name.size());

    DiskSkeletonBone root_bone;
    root_bone.parent_index = -1;
    root_bone.name_offset = root_name_offset;
    root_bone.name_size = static_cast<std::uint32_t>(root_name.size());
    root_bone.armature_from_bone[0] = 1.0f;
    root_bone.armature_from_bone[5] = 1.0f;
    root_bone.armature_from_bone[10] = 1.0f;
    root_bone.armature_from_bone[15] = 1.0f;

    DiskSkeletonBone child_bone = root_bone;
    child_bone.parent_index = 0;
    child_bone.name_offset = child_name_offset;
    child_bone.name_size = static_cast<std::uint32_t>(child_name.size());

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, skeleton_header);
    AppendStruct(payload, root_bone);
    AppendStruct(payload, child_bone);
    payload.insert(payload.end(), strings.begin(), strings.end());

    if (!Expect(CEngine::Tests::WriteTestAsset(skeleton_path, AssetType::Skeleton, payload,
                                               CEngine::Tests::TestGuid(skeleton_key), 17),
                "skeleton asset should write"))
    {
        return false;
    }

    AssetFile file;
    if (!Expect(file.Load(skeleton_path), "skeleton file should load"))
    {
        return false;
    }

    SkeletonAsset skeleton_asset;
    if (!Expect(skeleton_asset.Load(skeleton_path), "skeleton asset should load"))
    {
        return false;
    }
    if (!Expect(skeleton_asset.ArmatureName() == armature_name, "skeleton loader should expose armature name"))
    {
        return false;
    }
    if (!Expect(skeleton_asset.BoneCount() == 2, "skeleton loader should expose bone count"))
    {
        return false;
    }
    if (!Expect(skeleton_asset.BoneName(1) == child_name, "skeleton loader should expose bone names"))
    {
        return false;
    }
    DiskSkeletonBone loaded_child;
    if (!Expect(skeleton_asset.Bone(1, loaded_child), "skeleton loader should copy a bone row"))
    {
        return false;
    }
    if (!Expect(loaded_child.parent_index == 0, "skeleton loader should expose parent indices"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

/**
 * @brief TODO: Describe TextureLoadsAreShared.
 *
 * @return TODO: Describe the return value.
 */
bool TextureLoadsAreShared()
{
    const std::filesystem::path root = TestRoot();
    const std::string path = "assets/compiled/maps/shared.dds";
    WriteDdsStub(root / path);

    AssetStore assets(root);
    AssetReference reference;
    if (!Expect(assets.Resolve(path, AssetType::Texture, CEngine::Tests::TestGuid(path), reference),
                "texture reference should resolve"))
    {
        return false;
    }
    const auto first = assets.LoadTexture(reference, false);
    const auto second = assets.LoadTexture(reference, false);
    const auto flipped = assets.LoadTexture(reference, true);
    const bool result =
        Expect(first && second && flipped, "shared textures should load") &&
        Expect(first.get() == second.get(), "matching texture requests should share one decoded value") &&
        Expect(first.get() != flipped.get(), "different texture orientations should not share decoded values");
    std::filesystem::remove_all(root);
    return result;
}

} // namespace

/**
 * @brief TODO: Describe main.
 *
 * @return TODO: Describe the return value.
 */
int main()
{
    if (!CommonAssetPayloadRemainsDirectlyAddressable() || !MeshLoaderBuildsRendererMeshFromTargetAsset() ||
        !MeshLoaderTreatsSkinnedMeshesAsStaticGeometry() || !MeshLoaderReadsLightmapUvStream() ||
        !MaterialLoaderAllowsNeutralSurfaceSlots() || !MaterialLoaderResolvesProjectTexturePaths() ||
        !CAssetLoaderReadsBinaryComposition() || !SkeletonViewReadsBinaryBonesAndNames() || !TextureLoadsAreShared())
    {
        return 1;
    }
    return 0;
}
