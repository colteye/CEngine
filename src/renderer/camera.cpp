#include "renderer/camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Renderer
{

CameraFrameData Camera::BuildFrameData(float aspect_ratio) const
{
    const glm::vec3 view_direction =
        glm::length(direction) > 0.00001f ? glm::normalize(direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::cross(view_direction, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::length(right) <= 0.00001f)
    {
        right = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        right = glm::normalize(right);
    }
    const glm::vec3 up = glm::normalize(glm::cross(right, view_direction));
    const float valid_aspect = std::isfinite(aspect_ratio) && aspect_ratio > 0.0f ? aspect_ratio : 1.0f;
    const float valid_near = std::max(near_clip, 0.001f);
    const float valid_far = std::max(far_clip, valid_near + 0.001f);
    const float valid_fov = std::clamp(vertical_fov_radians, 0.01f, 3.13f);

    CameraFrameData camera_frame_data;
    camera_frame_data.view = glm::lookAt(position, position + view_direction, up);
    camera_frame_data.proj = glm::perspective(valid_fov, valid_aspect, valid_near, valid_far);
    camera_frame_data.camera_position = position;
    camera_frame_data.near_clip = valid_near;
    camera_frame_data.far_clip = valid_far;
    return camera_frame_data;
}

} // namespace CEngine::Renderer
