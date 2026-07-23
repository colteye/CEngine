#ifndef OPENGL_SHADOW_SYSTEM_H
#define OPENGL_SHADOW_SYSTEM_H

#include "renderer/light.h"
#include "renderer/opengl/opengl_render_data.h"
#include "renderer/opengl/opengl_shadow_types.h"
#include "renderer/opengl/shaders/depth_only.h"
#include "renderer/opengl/shaders/point_shadow_depth.h"

#include <array>
#include <memory>
#include <vector>

#include <glad/glad.h>

namespace CEngine::Renderer
{

class RenderSystem;

class OpenGLShadowSystem
{
  public:
    struct AtlasTile
    {
        int x = 0;
        int y = 0;
        int size = 0;
    };

    OpenGLShadowSystem() = default;
    OpenGLShadowSystem(const OpenGLShadowSystem &) = delete;
    OpenGLShadowSystem &operator=(const OpenGLShadowSystem &) = delete;
    ~OpenGLShadowSystem();

    bool Initialize(RenderSystem &rendering);
    void Destroy();
    void Render(const std::vector<OpenGLDrawItem> &draw_items, const OpenGLRenderQueues &queues);

    [[nodiscard]] const OpenGLShadowGpuData &GetGpuData() const
    {
        return gpu_data_;
    }
    [[nodiscard]] GLuint GetAtlasTexture() const
    {
        return atlas_texture_;
    }
    [[nodiscard]] const std::array<GLuint, OpenGLShadows::KMaxPointShadows> &GetPointTextures() const
    {
        return point_textures_;
    }

  private:
    AtlasTile AllocateAtlasTile(int requested_size);
    int AllocatePointSlot(size_t light_index, int resolution);
    void RenderSpotShadow(size_t light_index, const Light &light, const std::vector<OpenGLDrawItem> &draw_items,
                          const OpenGLRenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderDirectionalShadow(size_t light_index, const Light &light, const std::vector<OpenGLDrawItem> &draw_items,
                                 const OpenGLRenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderPointShadow(size_t light_index, const Light &light, const std::vector<OpenGLDrawItem> &draw_items,
                           const OpenGLRenderQueues &queues, std::vector<LightShadowBinding> &handles);
    void RenderDepthToAtlas(const AtlasTile &tile, const glm::mat4 &view, const glm::mat4 &projection,
                            const std::vector<OpenGLDrawItem> &draw_items, const OpenGLRenderQueues &queues);
    void RenderPointFace(GLenum face, GLuint texture, int resolution, const glm::mat4 &view,
                         const glm::mat4 &projection, const glm::vec3 &position, float far_plane,
                         const std::vector<OpenGLDrawItem> &draw_items, const OpenGLRenderQueues &queues);
    static bool SameMatrix(const glm::mat4 &left, const glm::mat4 &right);
    static bool SameRect(const glm::vec4 &left, const glm::vec4 &right);

    GLuint atlas_texture_ = 0;
    GLuint depth_fbo_ = 0;
    RenderSystem *rendering_ = nullptr;
    OpenGLShadowGpuData gpu_data_;
    std::array<GLuint, OpenGLShadows::KMaxPointShadows> point_textures_{};
    std::array<size_t, OpenGLShadows::KMaxPointShadows> point_light_indices_{};
    std::array<int, OpenGLShadows::KMaxPointShadows> point_resolutions_{};
    std::array<bool, OpenGLShadows::KMaxPointShadows> point_valid_{};
    std::array<bool, OpenGLShadows::KMaxPointShadows> point_slot_used_{};
    std::array<glm::mat4, OpenGLShadows::KMaxSpotShadows> cached_spot_matrices_{};
    std::array<glm::vec4, OpenGLShadows::KMaxSpotShadows> cached_spot_rects_{};
    std::array<bool, OpenGLShadows::KMaxSpotShadows> cached_spot_valid_{};
    std::array<glm::mat4, OpenGLShadows::KMaxDirectionalCascades> cached_cascade_matrices_{};
    std::array<glm::vec4, OpenGLShadows::KMaxDirectionalCascades> cached_cascade_rects_{};
    std::array<bool, OpenGLShadows::KMaxDirectionalCascades> cached_cascade_valid_{};
    std::unique_ptr<DepthOnly> depth_only_;
    std::unique_ptr<PointShadowDepth> point_depth_;
    int atlas_cursor_x_ = 0;
    int atlas_cursor_y_ = 0;
    int atlas_row_height_ = 0;
    std::uint64_t cached_mesh_instance_revision_ = 0;
    std::uint64_t cached_light_state_revision_ = 0;
    bool shadow_content_changed_ = true;
};

} // namespace CEngine::Renderer
#endif
