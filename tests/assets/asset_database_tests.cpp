#include "assets/asset_database.h"
#include "assets/asset_io.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace CEngine::Assets;
bool Expect(bool value, const char* message) { if (!value) std::cerr << message << '\n'; return value; }

std::filesystem::path TestRoot()
{
    return std::filesystem::current_path() / "build" / "asset-database-tests" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

bool DeduplicationAndTypesWork()
{
    const auto root = TestRoot();
    const std::string path = "assets/compiled/props/crate.cmesh";
    const Guid guid = GuidFromStableName(path);
    AssetWriteDesc desc; desc.type = AssetType::Mesh; desc.guid = guid; desc.payload = {1, 2, 3};
    if (!WriteBinaryAsset(root / path, desc)) return false;
    AssetDatabase database(root);
    const AssetHandle first = database.Acquire(path, AssetType::Mesh, guid);
    const AssetHandle second = database.Acquire(
        "assets/compiled/props/./crate.cmesh", AssetType::Mesh, guid);
    const bool result = Expect(first && second && first == second, "duplicate asset requests should share a handle") &&
        Expect(database.Size() == 1, "deduplicated asset should occupy one slot") &&
        Expect(database.Type(first) == AssetType::Mesh, "asset handle should preserve type") &&
        Expect(!database.Acquire(path, AssetType::Material, guid),
            "type mismatch should fail") &&
        Expect(database.Release(first) && database.IsValid(second), "one retained reference should keep asset live") &&
        Expect(database.Release(second) && !database.IsValid(second), "final release should invalidate asset");
    std::filesystem::remove_all(root);
    return result;
}

bool ReleaseInvalidatesHandles()
{
    const auto root = TestRoot();
    const std::string path = "assets/compiled/maps/lightmap.dds";
    std::filesystem::create_directories((root / path).parent_path());
    { std::ofstream file(root / path, std::ios::binary); file << "DDS "; }
    const Guid guid = GuidFromStableName(path);
    AssetDatabase database(root);
    const AssetHandle old =
        database.Acquire(path, AssetType::Texture, guid);
    database.ReleaseAll();
    const AssetHandle replacement =
        database.Acquire(path, AssetType::Texture, guid);
    const bool result = Expect(old && replacement, "texture references should resolve") &&
        Expect(!database.IsValid(old), "released handle should become stale") &&
        Expect(replacement.index == old.index && replacement.generation != old.generation,
            "reused asset slot should advance generation") &&
        Expect(!database.Acquire("../outside.cmesh", AssetType::Mesh, guid),
            "paths escaping the project should fail");
    std::filesystem::remove_all(root);
    return result;
}

} // namespace

int main()
{
    return DeduplicationAndTypesWork() && ReleaseInvalidatesHandles() ? 0 : 1;
}
