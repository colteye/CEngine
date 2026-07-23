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
 * @brief Cross-language coverage for generated asset readers.
 * @author Erik Coltey
 */

#include "assets/animation_asset.h"
#include "assets/casset_asset.h"
#include "assets/material_asset.h"
#include "assets/mesh_asset.h"
#include "assets/particle_asset.h"
#include "assets/physics_asset.h"
#include "assets/reader.h"
#include "assets/skeleton_asset.h"
#include "assets/store.h"
#include "assets/texture_asset.h"

#include <filesystem>
#include <iostream>

namespace
{
using namespace CEngine;
using namespace CEngine::Assets;

bool Expect(bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

Reference Ref(const std::filesystem::path &root, const char *name, Type type)
{
    Header header;
    std::vector<std::byte> payload;
    if (!ReadAsset(root / name, header, payload))
    {
        return {};
    }
    return {name, header.guid, type};
}

bool Run(const std::filesystem::path &root)
{
    Renderer::Mesh mesh;
    Renderer::Material material;
    PhysicsShape physics;
    Skeleton skeleton;
    Animation animation;
    CAsset asset;
    Particle particle;
    Renderer::Texture texture;
    if (!Expect(LoadMeshAsset(root / "python_fixture.cmesh", mesh), "generated mesh should load") ||
        !Expect(mesh.lods.size() == 2u && mesh.Lod(0.1f) == &mesh.lods.back(), "mesh LODs should load and select") ||
        !Expect(LoadMaterialAsset(root / "python_fixture.cmat", material), "generated material should load") ||
        !Expect(LoadPhysicsAsset(root / "python_fixture.cphys", physics), "generated physics should load") ||
        !Expect(skeleton.Load(root / "python_fixture.cskel"), "generated skeleton should load") ||
        !Expect(LoadAnimationAsset(root / "python_fixture.canim", animation), "generated animation should load") ||
        !Expect(asset.Load(root / "python_fixture.casset"), "generated composition should load") ||
        !Expect(LoadParticleAsset(root / "python_fixture.cparticle", particle), "generated particle should load") ||
        !Expect(LoadTextureAsset(root / "python_fixture.dds", texture, false), "mipmapped RGBExp32 should load") ||
        !Expect(texture.mips.size() == 2u && texture.mips.back().width == 1u && texture.mips.back().height == 1u,
                "RGBExp32 mip chain should be retained"))
    {
        return false;
    }

    Store store(root);
    const Reference mesh_ref = Ref(root, "python_fixture.cmesh", Type::Mesh);
    const Reference audio_ref = {"python_fixture.ogg", {}, Type::Audio};
    const auto first = store.LoadMesh(mesh_ref);
    const auto second = store.LoadMesh(mesh_ref);
    return Expect(first && first == second, "store should share one immutable mesh") &&
           Expect(store.LoadSkeleton(Ref(root, "python_fixture.cskel", Type::Skeleton)) != nullptr,
                  "store should load skeletons") &&
           Expect(store.LoadAnimation(Ref(root, "python_fixture.canim", Type::Animation)) != nullptr,
                  "store should load animations") &&
           Expect(store.LoadCAsset(Ref(root, "python_fixture.casset", Type::Asset)) != nullptr,
                  "store should load compositions") &&
           Expect(store.LoadParticle(Ref(root, "python_fixture.cparticle", Type::Particle)) != nullptr,
                  "store should load particles") &&
           Expect(store.LoadAudio(audio_ref) != nullptr, "store should load standard Ogg/Vorbis audio");
}
} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "expected generated fixture directory\n";
        return 1;
    }
    return Run(argv[1]) ? 0 : 1;
}
