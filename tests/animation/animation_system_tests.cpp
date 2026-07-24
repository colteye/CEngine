// Copyright (c) CEngine contributors.

#include "animation/animation_system.h"
#include "assets/reader.h"
#include "assets/store.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace
{
using namespace CEngine;

bool Expect(bool value, std::string_view message)
{
    if (!value)
    {
        std::cerr << message << '\n';
    }
    return value;
}

Assets::Reference ReferenceFor(const std::filesystem::path &root, const char *name,
                               Assets::Type type)
{
    Assets::Header header;
    std::vector<std::byte> payload;
    if (!Assets::ReadAsset(root / name, header, payload))
    {
        return {};
    }
    return {name, header.guid, type};
}

bool Run(const std::filesystem::path &root)
{
    Assets::Store assets(root);
    const auto skeleton = assets.LoadSkeleton(
        ReferenceFor(root, "python_fixture.cskel", Assets::Type::Skeleton));
    const auto clip = assets.LoadAnimation(
        ReferenceFor(root, "python_fixture.canim", Assets::Type::Animation));
    if (!Expect(skeleton != nullptr, "animation fixture skeleton should load") ||
        !Expect(clip != nullptr, "animation fixture clip should load"))
    {
        return false;
    }

    Animations::AnimationSystem system;
    if (!Expect(system.Initialize(), "animation system should initialize its backend"))
    {
        return false;
    }
    const Animations::AnimationInstanceHandle instance =
        system.CreateInstance({skeleton});
    if (!Expect(static_cast<bool>(instance), "Ozz backend should accept the neutral skeleton") ||
        !Expect(system.Play(instance, clip), "Ozz backend should accept the neutral animation"))
    {
        return false;
    }

    system.Evaluate(0.5f);
    const auto palette = system.SkinningPalette(instance);
    if (!Expect(palette.size() == 1u, "one-bone fixture should produce one palette matrix") ||
        !Expect(std::abs(palette.front()[3].x - 0.5f) < 0.001f,
                "backend should interpolate the raw engine track at half time") ||
        !Expect(system.Events().size() == 1u &&
                    system.Events().front().name == "Footstep",
                "animation system should emit crossed cosmetic markers"))
    {
        return false;
    }

    if (!Expect(system.SetPaused(instance, true), "live animation instance should pause"))
    {
        return false;
    }
    system.Evaluate(0.25f);
    const auto paused_palette = system.SkinningPalette(instance);
    if (!Expect(paused_palette.size() == 1u &&
                    std::abs(paused_palette.front()[3].x - 0.5f) < 0.001f,
                "paused animation should preserve its pose") ||
        !Expect(system.Events().empty(), "paused animation should not emit markers"))
    {
        return false;
    }

    return Expect(system.SetPaused(instance, false), "live animation instance should resume") &&
           Expect(system.CrossFade(instance, clip, 0.25f),
                  "animation system should cross-fade through its backend") &&
           Expect(system.DestroyInstance(instance), "live animation instance should be destroyable") &&
           Expect(!system.DestroyInstance(instance), "stale animation handle should be rejected") &&
           Expect(system.SkinningPalette(instance).empty(),
                  "stale animation handle should expose no palette");
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
