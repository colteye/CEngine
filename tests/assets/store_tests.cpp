//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/assets/store_tests.cpp
 * @brief Reference and immutable-cache tests using Python-generated assets.
 * @author Erik Coltey
 */

#include "assets/reader.h"
#include "assets/store.h"

#include <filesystem>
#include <iostream>

namespace
{
using namespace CEngine::Assets;

bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

bool Run(const std::filesystem::path &root)
{
    Header header;
    std::vector<std::byte> payload;
    if (!ReadAsset(root / "python_fixture.cmesh", header, payload))
    {
        return false;
    }
    Store store(root);
    Reference first;
    Reference second;
    Reference invalid;
    return Expect(store.Resolve("python_fixture.cmesh", Type::Mesh, header.guid, first),
                  "valid reference should resolve") &&
           Expect(store.Resolve("./python_fixture.cmesh", Type::Mesh, header.guid, second),
                  "equivalent reference should resolve") &&
           Expect(first.path == second.path && first.guid == second.guid,
                  "references should have one normalized identity") &&
           Expect(!store.Resolve("python_fixture.cmesh", Type::Material, header.guid, invalid),
                  "type mismatch should fail") &&
           Expect(!store.Resolve("../outside.cmesh", Type::Mesh, header.guid, invalid),
                  "escaping paths should fail") &&
           Expect(store.Resolve("python_fixture.ogg", Type::Audio, {}, first),
                  "standard audio should resolve without an engine envelope");
}
} // namespace

int main(int argc, char **argv)
{
    return argc == 2 && Run(argv[1]) ? 0 : 1;
}
