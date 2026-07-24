//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/camera.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_CAMERA_H
#define CENGINE_RENDERER_CAMERA_H

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe CameraFrameData.
 */
struct CameraFrameData
{
    glm::vec3 camera_position = glm::vec3(0.0f);
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    float near_clip = 0.1f;
    float far_clip = 100.0f;
};

/**
 * @brief TODO: Describe Camera.
 */
struct Camera
{
    glm::vec3 position = glm::vec3(-5.0f, 0.0f, 2.0f);
    glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f);
    float vertical_fov_radians = 0.7853982f;
    float near_clip = 0.1f;
    float far_clip = 100.0f;

    /**
     * @brief TODO: Describe BuildFrameData.
     *
     * @param aspect_ratio TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] CameraFrameData BuildFrameData(float aspect_ratio) const;
};

} // namespace CEngine::Renderer

#endif
