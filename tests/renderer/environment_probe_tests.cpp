//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

#include "renderer/render_system.h"

#include <iostream>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

namespace
{

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

std::shared_ptr<const CEngine::Renderer::Texture> MakePanorama()
{
    auto texture = std::make_shared<CEngine::Renderer::Texture>();
    texture->format = CEngine::Renderer::TextureFormat::Rgbe8;
    texture->mips.push_back({2, 1, {1, 1, 1, 128, 1, 1, 1, 128}});
    return texture;
}

} // namespace

int main()
{
    using namespace CEngine::Renderer;

    RenderSystem rendering;
    EnvironmentProbe probe;
    probe.panorama = MakePanorama();
    probe.transform = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f, 3.0f, 2.0f));
    probe.blend_distance = 0.5f;
    const std::uint64_t initial_revision = rendering.GetEnvironmentProbeRevision();
    const EnvironmentProbeHandle first = rendering.RegisterEnvironmentProbe(probe);
    if (!Expect(first && rendering.ResolveEnvironmentProbe(first) != nullptr,
                "a valid environment probe should register") ||
        !Expect(rendering.GetEnvironmentProbeRevision() > initial_revision,
                "probe registration should invalidate backend resources"))
    {
        return 1;
    }

    const std::uint64_t registered_revision = rendering.GetEnvironmentProbeRevision();
    rendering.UpdateEnvironmentProbe(first, probe);
    if (!Expect(rendering.GetEnvironmentProbeRevision() == registered_revision,
                "an identical probe update should not churn resources"))
    {
        return 1;
    }
    probe.intensity = 1.5f;
    rendering.UpdateEnvironmentProbe(first, probe);
    if (!Expect(rendering.ResolveEnvironmentProbe(first)->intensity == 1.5f,
                "a valid probe update should replace retained state"))
    {
        return 1;
    }

    rendering.RemoveEnvironmentProbe(first);
    const EnvironmentProbeHandle second = rendering.RegisterEnvironmentProbe(probe);
    EnvironmentProbe invalid = probe;
    invalid.transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 1.0f));
    const EnvironmentProbeHandle rejected = rendering.RegisterEnvironmentProbe(invalid);
    if (!Expect(!rendering.ResolveEnvironmentProbe(first), "a removed probe handle should be stale") ||
        !Expect(second && second.Index() == first.Index() && second.Generation() != first.Generation(),
                "probe slots should reuse indices with a new generation") ||
        !Expect(!rejected, "a singular probe influence transform should be rejected"))
    {
        return 1;
    }

    std::cout << "Environment probe tests passed.\n";
    return 0;
}
