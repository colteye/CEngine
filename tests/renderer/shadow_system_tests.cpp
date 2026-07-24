//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/renderer/shadow_system_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/opengl/shadow_system.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{

using CEngine::Renderer::Bounds;
using CEngine::Renderer::OpenGL::BuildDirectionalShadowCascade;
using CEngine::Renderer::OpenGL::DirectionalShadowCascade;

/**
 * @brief TODO: Describe BoxCorners.
 *
 * @param minimum TODO: Describe this parameter.
 * @param maximum TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
std::array<glm::vec3, 8> BoxCorners(const glm::vec3 &minimum, const glm::vec3 &maximum)
{
    return {
        glm::vec3(minimum.x, minimum.y, minimum.z), glm::vec3(maximum.x, minimum.y, minimum.z),
        glm::vec3(minimum.x, maximum.y, minimum.z), glm::vec3(maximum.x, maximum.y, minimum.z),
        glm::vec3(minimum.x, minimum.y, maximum.z), glm::vec3(maximum.x, minimum.y, maximum.z),
        glm::vec3(minimum.x, maximum.y, maximum.z), glm::vec3(maximum.x, maximum.y, maximum.z),
    };
}

/**
 * @brief TODO: Describe MakeBounds.
 *
 * @param minimum TODO: Describe this parameter.
 * @param maximum TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Bounds MakeBounds(const glm::vec3 &minimum, const glm::vec3 &maximum)
{
    Bounds bounds;
    bounds.min = minimum;
    bounds.max = maximum;
    bounds.valid = true;
    return bounds;
}

/**
 * @brief TODO: Describe Contains.
 *
 * @param cascade TODO: Describe this parameter.
 * @param corners TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe SameMatrix.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe Expect.
 *
 * @param condition TODO: Describe this parameter.
 * @param message TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool Expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

/**
 * @brief TODO: Describe TestDistantOverlappingCasterIsIncluded.
 *
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe TestCasterOutsideFootprintDoesNotExpandDepth.
 *
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe TestCasterRemainsIncludedAsReceiverMovesInDepth.
 *
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe TestObliqueLightFacingCasterIsIncluded.
 *
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe main.
 *
 * @return TODO: Describe the return value.
 */
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
