//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/assets/asset_store_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/asset_io.h"
#include "assets/asset_store.h"
#include "test_asset_writer.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using namespace CEngine::Assets;

/**
 * @brief TODO: Describe Expect.
 *
 * @param value TODO: Describe this parameter.
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

/**
 * @brief TODO: Describe TestRoot.
 *
 * @return TODO: Describe the return value.
 */
std::filesystem::path TestRoot()
{
    return std::filesystem::current_path() / "build" / "asset-store-tests" /
           std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
}

/**
 * @brief TODO: Describe ReferencesAreValidatedAndNormalized.
 *
 * @return TODO: Describe the return value.
 */
bool ReferencesAreValidatedAndNormalized()
{
    const auto root = TestRoot();
    const std::string path = "assets/compiled/props/crate.cmesh";
    const Guid guid = CEngine::Tests::TestGuid(path);
    if (!CEngine::Tests::WriteTestAsset(root / path, AssetType::Mesh, {1, 2, 3}, guid))
    {
        return false;
    }

    AssetStore assets(root);
    AssetReference first;
    AssetReference second;
    AssetReference invalid;
    const bool result =
        Expect(assets.Resolve(path, AssetType::Mesh, guid, first), "valid asset reference should resolve") &&
        Expect(assets.Resolve("assets/compiled/props/./crate.cmesh", AssetType::Mesh, guid, second),
               "equivalent asset path should resolve") &&
        Expect(first.path == second.path && first.guid == second.guid && first.type == second.type,
               "references should use one normalized identity") &&
        Expect(!assets.Resolve(path, AssetType::Material, guid, invalid), "type mismatch should fail") &&
        Expect(!assets.Resolve("../outside.cmesh", AssetType::Mesh, guid, invalid),
               "paths escaping the project should fail");
    std::filesystem::remove_all(root);
    return result;
}

/**
 * @brief TODO: Describe ExternalReferencesDoNotInventRuntimeHandles.
 *
 * @return TODO: Describe the return value.
 */
bool ExternalReferencesDoNotInventRuntimeHandles()
{
    const auto root = TestRoot();
    const std::string path = "assets/compiled/maps/lightmap.dds";
    std::filesystem::create_directories((root / path).parent_path());
    {
        std::ofstream file(root / path, std::ios::binary);
        file << "DDS ";
    }

    AssetStore assets(root);
    AssetReference reference;
    const bool result = Expect(assets.Resolve(path, AssetType::Texture, CEngine::Tests::TestGuid(path), reference),
                               "external texture should resolve") &&
                        Expect(reference.path == path && reference.type == AssetType::Texture,
                               "resolved reference should be a direct immutable value");
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
    return ReferencesAreValidatedAndNormalized() && ExternalReferencesDoNotInventRuntimeHandles() ? 0 : 1;
}
