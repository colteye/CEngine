#ifndef CENGINE_RENDERER_CAMERA_H
#define CENGINE_RENDERER_CAMERA_H

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

struct CameraFrameData
{
    glm::vec3 camera_position = glm::vec3(0.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    float near_clip = 0.1f;
    float far_clip = 100.0f;
};

struct Camera
{
    glm::vec3 position = glm::vec3(-5.0f, 0.0f, 2.0f);
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f);
    float vertical_fov_radians = 0.7853982f;
    float near_clip = 0.1f;
    float far_clip = 100.0f;

    [[nodiscard]] CameraFrameData BuildFrameData(float aspect_ratio) const;
};

} // namespace CEngine::Renderer

#endif
