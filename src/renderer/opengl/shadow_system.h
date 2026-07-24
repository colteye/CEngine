//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shadow_system.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADOW_SYSTEM_H
#define CENGINE_RENDERER_OPENGL_SHADOW_SYSTEM_H

#include "renderer/light.h"
#include "renderer/opengl/render_data.h"
#include "renderer/opengl/shaders/depth_only.h"
#include "renderer/opengl/shaders/point_shadow_depth.h"
#include "renderer/opengl/shadow_types.h"

#include <array>
#include <memory>
#include <vector>

#include <glad/glad.h>

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe DirectionalShadowCascade.
 */
struct DirectionalShadowCascade
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

/**
 * @brief TODO: Describe BuildDirectionalShadowCascade.
 *
 * @param receiver_corners TODO: Describe this parameter.
 * @param light_direction TODO: Describe this parameter.
 * @param resolution TODO: Describe this parameter.
 * @param shadow_caster_bounds TODO: Describe this parameter.
 * @param unbounded_caster_depth TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
DirectionalShadowCascade BuildDirectionalShadowCascade(const std::array<glm::vec3, 8> &receiver_corners,
                                                       const glm::vec3 &light_direction, int resolution,
                                                       const std::vector<Bounds> &shadow_caster_bounds,
                                                       float unbounded_caster_depth);

/**
 * @brief TODO: Describe ShadowSystem.
 */
class ShadowSystem
{
  public:
    /**
     * @brief TODO: Describe AtlasTile.
     */
    struct AtlasTile
    {
        int x = 0;
        int y = 0;
        int size = 0;
    };

    /**
     * @brief TODO: Describe ShadowSystem.
     */
    ShadowSystem() = default;
    /**
     * @brief TODO: Describe ShadowSystem.
     */
    ShadowSystem(const ShadowSystem &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShadowSystem &operator=(const ShadowSystem &) = delete;
    /**
     * @brief TODO: Describe ShadowSystem.
     */
    ShadowSystem(ShadowSystem &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShadowSystem &operator=(ShadowSystem &&) = delete;
    /**
     * @brief TODO: Describe ~ShadowSystem.
     */
    ~ShadowSystem();

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param rendering TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Initialize(RenderSystem &rendering);
    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();
    /**
     * @brief TODO: Describe Render.
     *
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     */
    void Render(const std::vector<DrawItem> &draw_items, const RenderQueues &queues);

    /**
     * @brief TODO: Describe GetGpuData.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const ShadowGpuData &GetGpuData() const
    {
        return gpu_data_;
    }
    /**
     * @brief TODO: Describe GetAtlasTexture.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] GLuint GetAtlasTexture() const
    {
        return atlas_texture_;
    }
    /**
     * @brief TODO: Describe GetPointTextures.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::array<GLuint, ShadowLimits::KMaxPointShadows> &GetPointTextures() const
    {
        return point_textures_;
    }

  private:
    /**
     * @brief TODO: Describe AllocateAtlasTile.
     *
     * @param requested_size TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    AtlasTile AllocateAtlasTile(int requested_size);
    /**
     * @brief TODO: Describe AllocatePointSlot.
     *
     * @param light_index TODO: Describe this parameter.
     * @param resolution TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    int AllocatePointSlot(size_t light_index, int resolution);
    /**
     * @brief TODO: Describe RenderSpotShadow.
     *
     * @param light_index TODO: Describe this parameter.
     * @param light TODO: Describe this parameter.
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     * @param handles TODO: Describe this parameter.
     */
    void RenderSpotShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                          const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    /**
     * @brief TODO: Describe RenderDirectionalShadow.
     *
     * @param light_index TODO: Describe this parameter.
     * @param light TODO: Describe this parameter.
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     * @param handles TODO: Describe this parameter.
     */
    void RenderDirectionalShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                                 const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    /**
     * @brief TODO: Describe RenderPointShadow.
     *
     * @param light_index TODO: Describe this parameter.
     * @param light TODO: Describe this parameter.
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     * @param handles TODO: Describe this parameter.
     */
    void RenderPointShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                           const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    /**
     * @brief TODO: Describe RenderDepthToAtlas.
     *
     * @param tile TODO: Describe this parameter.
     * @param view TODO: Describe this parameter.
     * @param projection TODO: Describe this parameter.
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     */
    void RenderDepthToAtlas(const AtlasTile &tile, const glm::mat4 &view, const glm::mat4 &projection,
                            const std::vector<DrawItem> &draw_items, const RenderQueues &queues);
    /**
     * @brief TODO: Describe RenderPointFace.
     *
     * @param face TODO: Describe this parameter.
     * @param texture TODO: Describe this parameter.
     * @param resolution TODO: Describe this parameter.
     * @param view TODO: Describe this parameter.
     * @param projection TODO: Describe this parameter.
     * @param position TODO: Describe this parameter.
     * @param far_plane TODO: Describe this parameter.
     * @param draw_items TODO: Describe this parameter.
     * @param queues TODO: Describe this parameter.
     */
    void RenderPointFace(GLenum face, GLuint texture, int resolution, const glm::mat4 &view,
                         const glm::mat4 &projection, const glm::vec3 &position, float far_plane,
                         const std::vector<DrawItem> &draw_items, const RenderQueues &queues);
    /**
     * @brief TODO: Describe SameMatrix.
     *
     * @param left TODO: Describe this parameter.
     * @param right TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    static bool SameMatrix(const glm::mat4 &left, const glm::mat4 &right);
    /**
     * @brief TODO: Describe SameRect.
     *
     * @param left TODO: Describe this parameter.
     * @param right TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    static bool SameRect(const glm::vec4 &left, const glm::vec4 &right);

    GLuint atlas_texture_ = 0;
    GLuint depth_fbo_ = 0;
    RenderSystem *rendering_ = nullptr;
    ShadowGpuData gpu_data_;
    std::array<GLuint, ShadowLimits::KMaxPointShadows> point_textures_{};
    std::array<size_t, ShadowLimits::KMaxPointShadows> point_light_indices_{};
    std::array<int, ShadowLimits::KMaxPointShadows> point_resolutions_{};
    std::array<bool, ShadowLimits::KMaxPointShadows> point_valid_{};
    std::array<bool, ShadowLimits::KMaxPointShadows> point_slot_used_{};
    std::array<glm::mat4, ShadowLimits::KMaxSpotShadows> cached_spot_matrices_{};
    std::array<glm::vec4, ShadowLimits::KMaxSpotShadows> cached_spot_rects_{};
    std::array<bool, ShadowLimits::KMaxSpotShadows> cached_spot_valid_{};
    std::array<glm::mat4, ShadowLimits::KMaxDirectionalCascades> cached_cascade_matrices_{};
    std::array<glm::vec4, ShadowLimits::KMaxDirectionalCascades> cached_cascade_rects_{};
    std::array<bool, ShadowLimits::KMaxDirectionalCascades> cached_cascade_valid_{};
    std::unique_ptr<DepthOnly> depth_only_;
    std::unique_ptr<PointShadowDepth> point_depth_;
    int atlas_cursor_x_ = 0;
    int atlas_cursor_y_ = 0;
    int atlas_row_height_ = 0;
    std::uint64_t cached_mesh_instance_revision_ = 0;
    std::uint64_t cached_light_state_revision_ = 0;
    bool shadow_content_changed_ = true;
};

} // namespace CEngine::Renderer::OpenGL
#endif
