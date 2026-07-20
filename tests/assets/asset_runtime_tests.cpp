#include "assets/casset_loader.h"
#include "assets/material_loader.h"
#include "assets/mesh_loader.h"
#include "assets/skeleton_loader.h"

#include <array>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace CEngine::Assets;
namespace Renderer = CEngine::Renderer;

constexpr std::array<char, 4> MeshMetadataMagic = {'C', 'E', 'M', 'H'};
constexpr std::uint16_t MeshMetadataVersion = 1;
constexpr std::uint32_t MeshFlagSkinned = 1u << 0u;
constexpr std::array<char, 4> MaterialPayloadMagic = {'C', 'E', 'M', 'A'};
constexpr std::uint16_t MaterialPayloadVersion = 1;
constexpr std::string_view DefaultTexturePath = "assets/missing/missing.DDS";

#pragma pack(push, 1)
struct DiskMaterialHeader {
    std::array<char, 4> magic = MaterialPayloadMagic;
    std::uint16_t version = MaterialPayloadVersion;
    std::uint16_t header_size = 0;
    std::uint32_t shader = 0;
    std::uint32_t texture_count = 0;
    std::uint32_t texture_table_offset = 0;
    std::uint32_t string_table_offset = 0;
    std::uint32_t string_table_size = 0;
    std::uint32_t name_offset = 0;
    std::uint32_t name_size = 0;
};

struct DiskMaterialTexture {
    std::uint32_t slot = 0;
    std::uint32_t path_offset = 0;
    std::uint32_t path_size = 0;
};

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

bool Expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

std::filesystem::path TestRoot()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::current_path() / "build" / "asset-tests" /
        ("runtime_" + std::to_string(now));
}

std::vector<std::uint8_t> Bytes(std::string_view text)
{
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

std::uint32_t AppendString(std::vector<std::uint8_t>& bytes, std::string_view text)
{
    const std::uint32_t offset = static_cast<std::uint32_t>(bytes.size());
    bytes.insert(bytes.end(), text.begin(), text.end());
    return offset;
}

template <typename T>
void AppendStruct(std::vector<std::uint8_t>& bytes, const T& value)
{
    const auto* data = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), data, data + sizeof(T));
}

void AppendF32(std::vector<std::uint8_t>& bytes, float value)
{
    AppendStruct(bytes, value);
}

void AppendU32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    AppendStruct(bytes, value);
}

void AppendStaticVertex(std::vector<std::uint8_t>& bytes,
    float x, float y, float z, float nx, float ny, float nz, float u, float v)
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

void AppendSkinnedStaticVertex(std::vector<std::uint8_t>& bytes,
    float x, float y, float z, float nx, float ny, float nz, float u, float v)
{
    AppendStaticVertex(bytes, x, y, z, nx, ny, nz, u, v);
    for (int index = 0; index < 8; ++index)
    {
        const std::uint16_t value = 0;
        AppendStruct(bytes, value);
    }
}

std::vector<std::uint8_t> StaticTriangleGeometry()
{
    std::vector<std::uint8_t> geometry;
    geometry.reserve(3 * 32 + 3 * 4);
    AppendStaticVertex(geometry, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    AppendStaticVertex(geometry, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    AppendStaticVertex(geometry, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    AppendU32(geometry, 0);
    AppendU32(geometry, 1);
    AppendU32(geometry, 2);
    return geometry;
}

std::vector<std::uint8_t> SkinnedTriangleGeometry()
{
    std::vector<std::uint8_t> geometry;
    geometry.reserve(3 * 48 + 3 * 4);
    AppendSkinnedStaticVertex(geometry, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    AppendSkinnedStaticVertex(geometry, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    AppendSkinnedStaticVertex(geometry, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    AppendU32(geometry, 0);
    AppendU32(geometry, 1);
    AppendU32(geometry, 2);
    return geometry;
}

std::vector<std::uint8_t> StaticMeshPayload(std::uint32_t flags = 0, std::uint32_t vertex_stride = 32)
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
    const std::vector<std::uint8_t> geometry =
        vertex_stride == 48 ? SkinnedTriangleGeometry() : StaticTriangleGeometry();
    payload.insert(payload.end(), geometry.begin(), geometry.end());
    return payload;
}

bool CommonAssetPayloadRemainsDirectlyAddressable()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path material_path = root / "assets" / "compiled" / "hero" / "materials" / "Hero.cmat";
    const std::string material_key = "assets/compiled/hero/materials/Hero.cmat";

    AssetWriteDesc material;
    material.type = AssetType::Material;
    material.guid = GuidFromStableName(material_key);
    material.source_hash = 7;
    material.payload = Bytes("binary material payload");

    std::string error;
    if (!Expect(WriteBinaryAsset(material_path, material, &error), error.c_str()))
    {
        return false;
    }

    AssetFile loaded;
    if (!Expect(loaded.Load(material_path, &error), error.c_str()))
    {
        return false;
    }
    const ByteView payload = loaded.Payload();
    if (!Expect(payload.size == material.payload.size(), "asset payload should preserve its byte size"))
    {
        return false;
    }
    if (!Expect(std::memcmp(payload.data, material.payload.data(), material.payload.size()) == 0,
            "asset payload should stay byte-exact"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

bool MeshLoaderBuildsRendererMeshFromTargetAsset()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path mesh_path = root / "assets" / "compiled" / "hero" / "meshes" / "SM_Body.cmesh";
    const std::string mesh_key = "assets/compiled/hero/meshes/SM_Body.cmesh";
    AssetWriteDesc mesh_asset;
    mesh_asset.type = AssetType::Mesh;
    mesh_asset.guid = GuidFromStableName(mesh_key);
    mesh_asset.source_hash = 9;
    mesh_asset.payload = StaticMeshPayload();

    std::string error;
    if (!Expect(WriteBinaryAsset(mesh_path, mesh_asset, &error), error.c_str()))
    {
        return false;
    }

    auto* material = reinterpret_cast<Renderer::Material*>(static_cast<std::uintptr_t>(1));
    Renderer::Mesh mesh;
    if (!Expect(LoadMeshAsset(mesh_path, { material }, mesh, &error), error.c_str()))
    {
        return false;
    }

    const auto& material_data = mesh.GetMaterialMeshData();
    if (!Expect(material_data.size() == 1, "loaded render mesh should have one material batch"))
    {
        return false;
    }
    const Renderer::MeshData& data = material_data.begin()->second;
    if (!Expect(data.vertices.size() == 3, "loaded render mesh should expand indexed vertices"))
    {
        return false;
    }
    if (!Expect(data.vertices[2].y == 2.0f, "loaded render mesh should decode vertex positions"))
    {
        return false;
    }
    if (!Expect(data.local_bounds.valid, "loaded render mesh should preserve metadata bounds"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

bool MeshLoaderTreatsSkinnedMeshesAsStaticGeometry()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path mesh_path = root / "assets" / "compiled" / "hero" / "meshes" / "SK_Body.cmesh";
    const std::string mesh_key = "assets/compiled/hero/meshes/SK_Body.cmesh";
    AssetWriteDesc mesh_asset;
    mesh_asset.type = AssetType::Mesh;
    mesh_asset.guid = GuidFromStableName(mesh_key);
    mesh_asset.source_hash = 13;
    mesh_asset.payload = StaticMeshPayload(MeshFlagSkinned, 48);

    std::string error;
    if (!Expect(WriteBinaryAsset(mesh_path, mesh_asset, &error), error.c_str()))
    {
        return false;
    }

    auto* material = reinterpret_cast<Renderer::Material*>(static_cast<std::uintptr_t>(1));
    Renderer::Mesh mesh;
    if (!Expect(LoadMeshAsset(mesh_path, { material }, mesh, &error), error.c_str()))
    {
        return false;
    }
    if (!Expect(mesh.GetMaterialMeshData().size() == 1,
            "skinned mesh payloads should load as static render geometry when skinning is ignored"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

bool MaterialLoaderFillsMissingTextureSlots()
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

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, material_header);
    payload.insert(payload.end(), strings.begin(), strings.end());

    AssetWriteDesc material_asset;
    material_asset.type = AssetType::Material;
    material_asset.guid = GuidFromStableName(material_key);
    material_asset.source_hash = 11;
    material_asset.payload = std::move(payload);

    std::string error;
    if (!Expect(WriteBinaryAsset(material_path, material_asset, &error), error.c_str()))
    {
        return false;
    }

    Renderer::Material material;
    if (!Expect(LoadMaterialAsset(material_path, material, &error), error.c_str()))
    {
        return false;
    }
    if (!Expect(material.GetAlbedoPath() == DefaultTexturePath,
            "material loader should fallback empty albedo texture paths"))
    {
        return false;
    }
    if (!Expect(material.GetNormalPath() == DefaultTexturePath,
            "material loader should fallback empty normal texture paths"))
    {
        return false;
    }
    if (!Expect(material.GetMetallicRoughnessAoPath() == DefaultTexturePath,
            "material loader should fallback empty metallic roughness ao texture paths"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

bool MaterialLoaderResolvesLegacyCompiledTexturePaths()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path material_path = root / "assets" / "barrel" / "materials" / "Barrel.cmat";
    const std::filesystem::path texture_path = root / "assets" / "barrel" / "textures" / "Albedo.dds";
    const std::string material_key = "assets/barrel/materials/Barrel.cmat";
    const std::string material_name = "Barrel";
    const std::string old_texture_path = "assets/compiled/barrel/textures/Albedo.dds";
    const std::string expected_texture_path = "assets/barrel/textures/Albedo.dds";

    std::filesystem::create_directories(texture_path.parent_path());
    std::ofstream(texture_path, std::ios::binary).put('\0');

    std::vector<std::uint8_t> strings;
    const std::uint32_t name_offset = AppendString(strings, material_name);
    const std::uint32_t texture_offset = AppendString(strings, old_texture_path);

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
    texture.path_size = static_cast<std::uint32_t>(old_texture_path.size());

    std::vector<std::uint8_t> payload;
    AppendStruct(payload, material_header);
    AppendStruct(payload, texture);
    payload.insert(payload.end(), strings.begin(), strings.end());

    AssetWriteDesc material_asset;
    material_asset.type = AssetType::Material;
    material_asset.guid = GuidFromStableName(material_key);
    material_asset.source_hash = 19;
    material_asset.payload = std::move(payload);

    std::string error;
    if (!Expect(WriteBinaryAsset(material_path, material_asset, &error), error.c_str()))
    {
        return false;
    }

    const std::filesystem::path previous_path = std::filesystem::current_path();
    std::filesystem::current_path(root);
    Renderer::Material material;
    const bool loaded = LoadMaterialAsset("assets/barrel/materials/Barrel.cmat", material, &error);
    std::filesystem::current_path(previous_path);

    if (!Expect(loaded, error.c_str()))
    {
        return false;
    }
    if (!Expect(material.GetAlbedoPath() == expected_texture_path,
            "material loader should remap assets/compiled texture paths into the flat assets folder"))
    {
        return false;
    }

    std::filesystem::remove_all(root);
    return true;
}

bool CAssetLoaderReadsBinaryComposition()
{
    const std::filesystem::path root = TestRoot();
    const std::filesystem::path casset_path = root / "assets" / "compiled" / "barrel" / "Barrel.casset";
    const std::string casset_key = "assets/compiled/barrel/Barrel.casset";
    const std::string source_path = "assets/source/barrel.blend";
    const std::string collection_name = "PREFAB_Barrel";
    const std::string object_name = "SM_Barrel";
    const std::string mesh_path = "assets/compiled/barrel/meshes/SM_Barrel.cmesh";

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

    AssetWriteDesc asset;
    asset.type = AssetType::Asset;
    asset.guid = GuidFromStableName(casset_key);
    asset.source_hash = 21;
    asset.payload = std::move(payload);

    std::string error;
    if (!Expect(WriteBinaryAsset(casset_path, asset, &error), error.c_str()))
    {
        return false;
    }

    CAsset casset;
    if (!Expect(casset.Load(casset_path, &error), error.c_str()))
    {
        return false;
    }
    if (!Expect(casset.CompositionType() == CAssetCompositionType::Prefab,
            "casset should expose composition type"))
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
    if (!Expect(casset.Component(loaded_object, 0, loaded_component),
            "casset should expose object component rows"))
    {
        return false;
    }
    if (!Expect(loaded_component.kind == CAssetComponentKind::Mesh,
            "casset component should expose its kind"))
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
    const std::uint32_t root_name_offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), root_name.begin(), root_name.end());
    const std::uint32_t child_name_offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), child_name.begin(), child_name.end());

    DiskSkeletonHeader skeleton_header;
    skeleton_header.header_size = sizeof(DiskSkeletonHeader);
    skeleton_header.bone_count = 2;
    skeleton_header.bone_table_offset = sizeof(DiskSkeletonHeader);
    skeleton_header.string_table_offset = sizeof(DiskSkeletonHeader) + 2 * sizeof(DiskSkeletonBone);
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

    AssetWriteDesc skeleton;
    skeleton.type = AssetType::Skeleton;
    skeleton.guid = GuidFromStableName(skeleton_key);
    skeleton.source_hash = 17;
    skeleton.payload = std::move(payload);

    std::string error;
    if (!Expect(WriteBinaryAsset(skeleton_path, skeleton, &error), error.c_str()))
    {
        return false;
    }

    AssetFile file;
    if (!Expect(file.Load(skeleton_path, &error), error.c_str()))
    {
        return false;
    }

    SkeletonAsset skeleton_asset;
    if (!Expect(skeleton_asset.Load(skeleton_path, &error), error.c_str()))
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

} // namespace

int main()
{
    if (!CommonAssetPayloadRemainsDirectlyAddressable() ||
        !MeshLoaderBuildsRendererMeshFromTargetAsset() ||
        !MeshLoaderTreatsSkinnedMeshesAsStaticGeometry() ||
        !MaterialLoaderFillsMissingTextureSlots() ||
        !MaterialLoaderResolvesLegacyCompiledTexturePaths() ||
        !CAssetLoaderReadsBinaryComposition() ||
        !SkeletonViewReadsBinaryBonesAndNames())
    {
        return 1;
    }
    return 0;
}
