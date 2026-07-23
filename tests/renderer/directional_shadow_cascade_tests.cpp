#include "renderer/opengl/directional_shadow_cascade.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using CEngine::Renderer::Bounds;
using CEngine::Renderer::BuildDirectionalShadowCascade;
using CEngine::Renderer::DirectionalShadowCascade;

std::array<glm::vec3, 8> BoxCorners(const glm::vec3 &minimum, const glm::vec3 &maximum)
{
    return {
        glm::vec3(minimum.x, minimum.y, minimum.z), glm::vec3(maximum.x, minimum.y, minimum.z),
        glm::vec3(minimum.x, maximum.y, minimum.z), glm::vec3(maximum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, minimum.y, maximum.z), glm::vec3(maximum.x, minimum.y, maximum.z),
        glm::vec3(minimum.x, maximum.y, maximum.z), glm::vec3(maximum.x, maximum.y, maximum.z),
    };
}

Bounds MakeBounds(const glm::vec3 &minimum, const glm::vec3 &maximum)
{
    Bounds bounds;
    bounds.min = minimum;
    bounds.max = maximum;
    bounds.valid = true;
    return bounds;
}

bool Contains(const DirectionalShadowCascade &cascade, const std::array<glm::vec3, 8> &corners)
{
    const glm::mat4 view_projection = cascade.projection * cascade.view;
    for (const glm::vec3 &corner : corners)
    {
        const glm::vec4 clip = view_projection * glm::vec4(corner, 1.0f);
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        constexpr float Epsilon = 0.0001f;
        if (ndc.x < -1.0f - Epsilon || ndc.x > 1.0f + Epsilon || ndc.y < -1.0f - Epsilon || ndc.y > 1.0f + Epsilon ||
            ndc.z < -1.0f - Epsilon || ndc.z > 1.0f + Epsilon)
        {
            return false;
        }
    }
    return true;
}

bool SameMatrix(const glm::mat4 &left, const glm::mat4 &right)
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (std::abs(left[column][row] - right[column][row]) > 0.000001f)
            {
                return false;
            }
        }
    }
    return true;
}

bool Expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

bool TestDistantOverlappingCasterIsIncluded()
{
    const auto receiver = BoxCorners(glm::vec3(-5.0f), glm::vec3(5.0f));
    const Bounds caster = MakeBounds(glm::vec3(-1.0f, -1.0f, 49.0f), glm::vec3(1.0f, 1.0f, 51.0f));
    const DirectionalShadowCascade cascade =
        BuildDirectionalShadowCascade(receiver, glm::vec3(0.0f, 0.0f, -1.0f), 1024, {caster}, 200.0f);
    return Expect(Contains(cascade, receiver), "cascade must contain its complete receiver slice") &&
           Expect(Contains(cascade, BoxCorners(caster.min, caster.max)),
                  "a distant light-facing caster overlapping the receiver footprint must not be "
                  "depth-clipped");
}

bool TestCasterOutsideFootprintDoesNotExpandDepth()
{
    const auto receiver = BoxCorners(glm::vec3(-5.0f), glm::vec3(5.0f));
    const DirectionalShadowCascade without_caster =
        BuildDirectionalShadowCascade(receiver, glm::vec3(0.0f, 0.0f, -1.0f), 1024, {}, 200.0f);
    const Bounds outside = MakeBounds(glm::vec3(100.0f, 100.0f, 49.0f), glm::vec3(102.0f, 102.0f, 51.0f));
    const DirectionalShadowCascade with_outside_caster =
        BuildDirectionalShadowCascade(receiver, glm::vec3(0.0f, 0.0f, -1.0f), 1024, {outside}, 200.0f);
    return Expect(SameMatrix(without_caster.view, with_outside_caster.view) &&
                      SameMatrix(without_caster.projection, with_outside_caster.projection),
                  "casters outside the cascade footprint must not reduce its depth precision");
}

bool TestCasterRemainsIncludedAsReceiverMovesInDepth()
{
    const Bounds caster = MakeBounds(glm::vec3(-1.0f, -1.0f, 49.0f), glm::vec3(1.0f, 1.0f, 51.0f));
    for (float receiver_z : {0.0f, -20.0f, -40.0f})
    {
        const auto receiver =
            BoxCorners(glm::vec3(-5.0f, -5.0f, receiver_z - 5.0f), glm::vec3(5.0f, 5.0f, receiver_z + 5.0f));
        const DirectionalShadowCascade cascade =
            BuildDirectionalShadowCascade(receiver, glm::vec3(0.0f, 0.0f, -1.0f), 1024, {caster}, 200.0f);
        if (!Expect(Contains(cascade, receiver), "moving receiver slice must remain inside its cascade") ||
            !Expect(Contains(cascade, BoxCorners(caster.min, caster.max)),
                    "overlapping caster must remain included as camera depth changes"))
        {
            return false;
        }
    }
    return true;
}

bool TestObliqueLightFacingCasterIsIncluded()
{
    const auto receiver = BoxCorners(glm::vec3(-5.0f), glm::vec3(5.0f));
    const glm::vec3 light_direction = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
    const glm::vec3 caster_center = -light_direction * 50.0f;
    const Bounds caster = MakeBounds(caster_center - glm::vec3(1.0f), caster_center + glm::vec3(1.0f));
    const DirectionalShadowCascade cascade =
        BuildDirectionalShadowCascade(receiver, light_direction, 1024, {caster}, 200.0f);
    return Expect(Contains(cascade, receiver), "oblique cascade must contain its complete receiver slice") &&
           Expect(Contains(cascade, BoxCorners(caster.min, caster.max)),
                  "caster toward an oblique directional light must not be depth-clipped");
}

} // namespace

int main()
{
    bool ok = true;
    ok = TestDistantOverlappingCasterIsIncluded() && ok;
    ok = TestCasterOutsideFootprintDoesNotExpandDepth() && ok;
    ok = TestCasterRemainsIncludedAsReceiverMovesInDepth() && ok;
    ok = TestObliqueLightFacingCasterIsIncluded() && ok;
    if (!ok)
    {
        return 1;
    }
    std::cout << "Directional shadow cascade tests passed.\n";
    return 0;
}
