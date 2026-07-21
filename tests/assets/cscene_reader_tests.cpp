#include "assets/cscene_reader.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {
using namespace CEngine::Assets;
using namespace CEngine::Assets::CSceneFormat;

template <typename T> void Append(std::vector<std::uint8_t>& out, const T& value)
{ const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value); out.insert(out.end(), bytes, bytes + sizeof(T)); }
bool Expect(bool value, const char* message) { if (!value) std::cerr << message << '\n'; return value; }

std::filesystem::path Root()
{
    return std::filesystem::current_path() / "build" / "cscene-tests" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

std::vector<std::uint8_t> MinimalPayload()
{
    DiskSceneHeader header;
    header.header_size = sizeof(header);
    header.settings_offset = sizeof(header);
    header.entity_table_offset = header.settings_offset + sizeof(DiskSceneSettings);
    header.entity_count = 1; header.entity_stride = sizeof(DiskSceneEntity);
    header.class_table_offset = header.entity_table_offset + sizeof(DiskSceneEntity);
    header.class_count = 1; header.class_stride = sizeof(DiskEntityClassBlock);
    const std::uint64_t indices_offset = header.class_table_offset + sizeof(DiskEntityClassBlock);
    const std::uint64_t transforms_offset = indices_offset + sizeof(std::uint32_t);
    header.string_table_offset = transforms_offset + sizeof(DiskTransform);
    header.string_table_size = 9;
    DiskSceneSettings settings;
    DiskSceneEntity entity;
    entity.classname_size = 5;
    entity.name_offset = 5; entity.name_size = 4;
    DiskEntityClassBlock block;
    block.classname_size = 5;
    block.flags = EntityClassBlockRequired;
    block.count = 1; block.record_stride = sizeof(DiskTransform);
    block.entity_indices_offset = indices_offset; block.records_offset = transforms_offset;
    DiskTransform transform;
    std::vector<std::uint8_t> payload;
    Append(payload, header); Append(payload, settings); Append(payload, entity); Append(payload, block);
    Append(payload, std::uint32_t{0}); Append(payload, transform);
    payload.insert(payload.end(), {'e', 'm', 'p', 't', 'y', 'R', 'o', 'o', 't'});
    return payload;
}

bool Write(const std::filesystem::path& path, std::vector<std::uint8_t> payload)
{
    AssetWriteDesc desc; desc.type = AssetType::Scene; desc.payload = std::move(payload);
    std::string error; return WriteBinaryAsset(path, desc, &error);
}

bool ValidFixtureLoads()
{
    const auto root = Root(); const auto path = root / "minimal.cscene";
    if (!Write(path, MinimalPayload())) return false;
    CSceneFile file; std::string error;
    const bool result = Expect(file.Load(path, &error), error.c_str()) &&
        Expect(file.Entities().size == 1, "fixture should contain one entity") &&
        Expect(file.ClassBlocks().size == 1, "fixture should contain one class block") &&
        Expect(file.String(file.Entities()[0].classname_offset, file.Entities()[0].classname_size) == "empty", "classname should load") &&
        Expect(file.String(file.Entities()[0].name_offset, file.Entities()[0].name_size) == "Root", "name should load");
    std::filesystem::remove_all(root); return result;
}

bool InvalidRangesFail()
{
    const auto root = Root(); std::string error; CSceneFile file;
    auto payload = MinimalPayload();
    auto* header = reinterpret_cast<DiskSceneHeader*>(payload.data());
    header->entity_table_offset = UINT64_MAX - 4;
    const auto bad_offset = root / "bad-offset.cscene"; Write(bad_offset, payload);
    if (!Expect(!file.Load(bad_offset, &error), "overflowing table offset should fail")) return false;
    payload = MinimalPayload();
    header = reinterpret_cast<DiskSceneHeader*>(payload.data());
    auto* block = reinterpret_cast<DiskEntityClassBlock*>(payload.data() + header->class_table_offset);
    block->record_stride = UINT32_MAX; block->count = UINT32_MAX;
    const auto bad_size = root / "bad-size.cscene"; Write(bad_size, payload);
    if (!Expect(!file.Load(bad_size, &error), "overflowing class range should fail")) return false;
    payload = MinimalPayload(); header = reinterpret_cast<DiskSceneHeader*>(payload.data()); header->version = 99;
    const auto bad_version = root / "bad-version.cscene"; Write(bad_version, payload);
    const bool result = Expect(!file.Load(bad_version, &error), "unsupported scene version should fail");
    std::filesystem::remove_all(root); return result;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2)
    {
        CSceneFile file;
        std::string error;
        return Expect(file.Load(argv[1], &error), error.c_str()) &&
            Expect(file.Entities().size == 2, "Python fixture should contain two entities") &&
            Expect(file.ClassBlocks().size == 2, "Python fixture should contain two class blocks") &&
            Expect(file.Connections().size == 1, "Python fixture should contain one connection") &&
            Expect(file.String(file.Entities()[0].classname_offset,
                       file.Entities()[0].classname_size) == "prefab_instance",
                "Python fixture classname should match") ? 0 : 1;
    }
    return ValidFixtureLoads() && InvalidRangesFail() ? 0 : 1;
}
