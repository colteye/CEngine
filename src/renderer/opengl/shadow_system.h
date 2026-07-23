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
struct DirectionalShadowCascade
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

DirectionalShadowCascade BuildDirectionalShadowCascade(const std::array<glm::vec3, 8> &receiver_corners,
                                                       const glm::vec3 &light_direction, int resolution,
                                                       const std::vector<Bounds> &shadow_caster_bounds,
                                                       float unbounded_caster_depth);

class ShadowSystem
{
  public:
    struct AtlasTile
    {
        int x = 0;
        int y = 0;
        int size = 0;
    };

    ShadowSystem() = default;
    ShadowSystem(const ShadowSystem &) = delete;
    ShadowSystem &operator=(const ShadowSystem &) = delete;
    ShadowSystem(ShadowSystem &&) = delete;
    ShadowSystem &operator=(ShadowSystem &&) = delete;
    ~ShadowSystem();

    bool Initialize(RenderSystem &rendering);
    void Destroy();
    void Render(const std::vector<DrawItem> &draw_items, const RenderQueues &queues);

    [[nodiscard]] const ShadowGpuData &GetGpuData() const
    {
        return gpu_data_;
    }
    [[nodiscard]] GLuint GetAtlasTexture() const
    {
        return atlas_texture_;
    }
    [[nodiscard]] const std::array<GLuint, ShadowLimits::KMaxPointShadows> &GetPointTextures() const
    {
        return point_textures_;
    }

  private:
    AtlasTile AllocateAtlasTile(int requested_size);
    int AllocatePointSlot(size_t light_index, int resolution);
    void RenderSpotShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                          const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderDirectionalShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                                 const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderPointShadow(size_t light_index, const Light &light, const std::vector<DrawItem> &draw_items,
                           const RenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderDepthToAtlas(const AtlasTile &tile, const glm::mat4 &view, const glm::mat4 &projection,
                            const std::vector<DrawItem> &draw_items, const RenderQueues &queues);
    void RenderPointFace(GLenum face, GLuint texture, int resolution, const glm::mat4 &view,
                         const glm::mat4 &projection, const glm::vec3 &position, float far_plane,
                         const std::vector<DrawItem> &draw_items, const RenderQueues &queues);
    static bool SameMatrix(const glm::mat4 &left, const glm::mat4 &right);
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
