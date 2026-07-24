//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/skeleton_asset.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/skeleton_asset.h"

#include "log.h"

#include <cmath>
#include <unordered_set>

#include <glm/gtc/matrix_inverse.hpp>

namespace CEngine::Assets
{
namespace
{
constexpr float QuaternionEpsilon = 1.0e-12f;
constexpr float UniformScaleTolerance = 1.0e-4f;

glm::mat4 Matrix(const std::array<float, 16> &source)
{
    glm::mat4 result(1.0f);
    for (std::size_t row = 0; row < 4u; ++row)
    {
        for (std::size_t column = 0; column < 4u; ++column)
        {
            result[column][row] = source[row * 4u + column];
        }
    }
    return result;
}

bool ValidTransform(const AnimationTransform &transform)
{
    float quaternion_length = 0.0f;
    for (float component : transform.rotation)
    {
        if (!std::isfinite(component))
        {
            return false;
        }
        quaternion_length += component * component;
    }
    if (quaternion_length <= QuaternionEpsilon)
    {
        return false;
    }
    for (float component : transform.translation)
    {
        if (!std::isfinite(component))
        {
            return false;
        }
    }
    for (float component : transform.scale)
    {
        if (!std::isfinite(component) || component <= 0.0f)
        {
            return false;
        }
    }
    return std::abs(transform.scale[0] - transform.scale[1]) <=
               UniformScaleTolerance &&
           std::abs(transform.scale[0] - transform.scale[2]) <=
               UniformScaleTolerance;
}
} // namespace

Skeleton::Skeleton(Skeleton &&) noexcept = default;
Skeleton &Skeleton::operator=(Skeleton &&) noexcept = default;

bool Skeleton::Load(const std::filesystem::path &path, const Guid &identity)
{
    SkeletonWire::Skeleton decoded;
    if (!Decode(path, Type::Skeleton, decoded) || decoded.bones.empty() ||
        decoded.bones.front().parent != -1)
    {
        Logging::Logger::Get().Error("assets", "skeleton payload is invalid");
        return false;
    }

    std::unordered_set<std::string> unique_names;
    std::vector<glm::mat4> inverse_bind;
    inverse_bind.reserve(decoded.bones.size());
    for (std::size_t index = 0; index < decoded.bones.size(); ++index)
    {
        const SkeletonBone &bone = decoded.bones[index];
        const bool hierarchy_valid =
            index == 0u
                ? bone.parent == -1
                : bone.parent >= 0 &&
                      static_cast<std::size_t>(bone.parent) < index;
        if (!hierarchy_valid || !unique_names.insert(bone.name).second ||
            !ValidTransform(bone.rest))
        {
            Logging::Logger::Get().Error(
                "assets", "skeleton hierarchy or rest transform is invalid");
            return false;
        }
        const glm::mat4 matrix = Matrix(bone.joint_from_armature_bind);
        const float determinant = glm::determinant(matrix);
        if (!std::isfinite(determinant) ||
            std::abs(determinant) <= 1.0e-8f)
        {
            Logging::Logger::Get().Error(
                "assets", "skeleton inverse bind matrix is singular");
            return false;
        }
        inverse_bind.push_back(matrix);
    }

    data_ = std::move(decoded);
    identity_ = identity;
    joint_from_armature_bind_ = std::move(inverse_bind);
    return true;
}

std::uint32_t Skeleton::BoneCount() const
{
    return static_cast<std::uint32_t>(data_.bones.size());
}

std::string_view Skeleton::ArmatureName() const
{
    return data_.name;
}

const SkeletonBone *Skeleton::Bone(std::uint32_t index) const
{
    return index < data_.bones.size() ? &data_.bones[index] : nullptr;
}

std::string_view Skeleton::BoneName(std::uint32_t index) const
{
    const SkeletonBone *bone = Bone(index);
    return bone == nullptr ? std::string_view{} : std::string_view(bone->name);
}

const AnimationTransform *Skeleton::RestTransform(std::uint32_t index) const
{
    const SkeletonBone *bone = Bone(index);
    return bone == nullptr ? nullptr : &bone->rest;
}

const glm::mat4 *
Skeleton::JointFromArmatureBind(std::uint32_t index) const
{
    return index < joint_from_armature_bind_.size()
               ? &joint_from_armature_bind_[index]
               : nullptr;
}

} // namespace CEngine::Assets
