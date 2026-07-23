#include "opengl_shadow_system.h"

#include "renderer/opengl/directional_shadow_cascade.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Renderer
{

namespace
{
constexpr float KDirectionalShadowDistance = 200.0f;
constexpr float KCascadeSplitLambda = 0.9f;

glm::vec3 NormalizeOrDefault(const glm::vec3 &value, const glm::vec3 &fallback)
{
    const float length = glm::length(value);
    if (length <= 0.00001f)
    {
        return fallback;
    }
    return value / length;
}

glm::vec3 StableUpForDirection(const glm::vec3 &direction)
{
    if (std::abs(glm::dot(direction, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.95f)
    {
        return {0.0f, 1.0f, 0.0f};
    }
    return {0.0f, 0.0f, 1.0f};
}

glm::mat4 LookAlong(const glm::vec3 &position, const glm::vec3 &direction)
{
    const glm::vec3 forward = NormalizeOrDefault(direction, glm::vec3(0.0f, 0.0f, -1.0f));
    return glm::lookAt(position, position + forward, StableUpForDirection(forward));
}

int ClampShadowResolution(uint32_t resolution)
{
    const uint32_t clamped = std::max<uint32_t>(64, std::min<uint32_t>(resolution, OpenGLShadows::KAtlasSize));
    int power_of_two = 64;
    while (power_of_two * 2 <= static_cast<int>(clamped))
    {
        power_of_two *= 2;
    }
    return power_of_two;
}

float ProjectionNear(const glm::mat4 &projection)
{
    return projection[3][2] / (projection[2][2] - 1.0f);
}

float ProjectionFar(const glm::mat4 &projection)
{
    return projection[3][2] / (projection[2][2] + 1.0f);
}

std::array<glm::vec3, 8> FrustumSliceCorners(const glm::mat4 &inverse_view, const glm::mat4 &projection,
                                             float near_depth, float far_depth)
{
    const float tan_y = 1.0f / projection[1][1];
    const float tan_x = 1.0f / projection[0][0];
    std::array<glm::vec3, 8> corners{};
    int index = 0;
    for (float depth : {near_depth, far_depth})
    {
        const float x = depth * tan_x;
        const float y = depth * tan_y;
        for (const glm::vec2 sign :
             {glm::vec2(-1.0f, -1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(-1.0f, 1.0f)})
        {
            const glm::vec4 view_pos(sign.x * x, sign.y * y, -depth, 1.0f);
            corners[index++] = glm::vec3(inverse_view * view_pos);
        }
    }
    return corners;
}

glm::vec4 AtlasRect(const OpenGLShadowSystem::AtlasTile &tile)
{
    const auto atlas_size = static_cast<float>(OpenGLShadows::KAtlasSize);
    const float inset = 0.5f / atlas_size;
    const float size = static_cast<float>(tile.size - 1) / atlas_size;
    return {(static_cast<float>(tile.x) / atlas_size) + inset, (static_cast<float>(tile.y) / atlas_size) + inset, size,
            size};
}

float SpotOuterDegrees(const Light &light)
{
    return glm::degrees(std::acos(std::max(-1.0f, std::min(1.0f, light.spot_outer_cos))));
}

float CascadeBlendRange(float split_depth, bool last_cascade)
{
    return last_cascade ? 0.0f : std::max(0.75f, split_depth * 0.05f);
}
} // namespace

bool OpenGLShadowSystem::SameMatrix(const glm::mat4 &left, const glm::mat4 &right)
{
    return std::memcmp(&left[0][0], &right[0][0], sizeof(glm::mat4)) == 0;
}

bool OpenGLShadowSystem::SameRect(const glm::vec4 &left, const glm::vec4 &right)
{
    return std::memcmp(&left[0], &right[0], sizeof(glm::vec4)) == 0;
}

OpenGLShadowSystem::~OpenGLShadowSystem()
{
    Destroy();
}

bool OpenGLShadowSystem::Initialize(RenderSystem &in_rendering)
{
    rendering_ = &in_rendering;
    glGenFramebuffers(1, &depth_fbo_);

    glGenTextures(1, &atlas_texture_);
    glBindTexture(GL_TEXTURE_2D, atlas_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, OpenGLShadows::KAtlasSize, OpenGLShadows::KAtlasSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D, 0);

    point_light_indices_.fill(std::numeric_limits<size_t>::max());
    point_resolutions_.fill(0);
    point_valid_.fill(false);
    point_slot_used_.fill(false);
    cached_spot_valid_.fill(false);
    cached_cascade_valid_.fill(false);
    cached_mesh_instance_revision_ = 0;
    cached_light_state_revision_ = 0;
    shadow_content_changed_ = true;
    for (GLuint &texture : point_textures_)
    {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        for (int face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT,
                         GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    depth_only_ = std::make_unique<DepthOnly>();
    point_depth_ = std::make_unique<PointShadowDepth>();
    return atlas_texture_ != 0 && depth_fbo_ != 0;
}

void OpenGLShadowSystem::Destroy()
{
    for (GLuint &texture : point_textures_)
    {
        if (texture != 0)
        {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
    }
    if (atlas_texture_ != 0)
    {
        glDeleteTextures(1, &atlas_texture_);
        atlas_texture_ = 0;
    }
    if (depth_fbo_ != 0)
    {
        glDeleteFramebuffers(1, &depth_fbo_);
        depth_fbo_ = 0;
    }

    point_light_indices_.fill(std::numeric_limits<size_t>::max());
    point_resolutions_.fill(0);
    point_valid_.fill(false);
    depth_only_.reset();
    point_depth_.reset();
    gpu_data_ = OpenGLShadowGpuData();
    point_slot_used_.fill(false);
    cached_spot_valid_.fill(false);
    cached_cascade_valid_.fill(false);
    cached_mesh_instance_revision_ = 0;
    cached_light_state_revision_ = 0;
    shadow_content_changed_ = true;
    rendering_ = nullptr;
}

OpenGLShadowSystem::AtlasTile OpenGLShadowSystem::AllocateAtlasTile(int requested_size)
{
    const int size = ClampShadowResolution(static_cast<uint32_t>(requested_size));
    if (atlas_cursor_x_ + size > OpenGLShadows::KAtlasSize)
    {
        atlas_cursor_x_ = 0;
        atlas_cursor_y_ += atlas_row_height_;
        atlas_row_height_ = 0;
    }
    if (atlas_cursor_y_ + size > OpenGLShadows::KAtlasSize)
    {
        return {};
    }

    AtlasTile tile;
    tile.x = atlas_cursor_x_;
    tile.y = atlas_cursor_y_;
    tile.size = size;
    atlas_cursor_x_ += size;
    atlas_row_height_ = std::max(atlas_row_height_, size);
    return tile;
}

int OpenGLShadowSystem::AllocatePointSlot(size_t light_index, int resolution)
{
    for (int slot = 0; slot < OpenGLShadows::KMaxPointShadows; ++slot)
    {
        if (point_light_indices_[slot] == light_index)
        {
            if (point_resolutions_[slot] == resolution && point_textures_[slot] != 0)
            {
                point_slot_used_[slot] = true;
                return slot;
            }
            if (point_textures_[slot] != 0)
            {
                glDeleteTextures(1, &point_textures_[slot]);
                point_textures_[slot] = 0;
            }
            point_light_indices_[slot] = std::numeric_limits<size_t>::max();
            point_resolutions_[slot] = 0;
            point_valid_[slot] = false;
            break;
        }
    }

    for (int slot = 0; slot < OpenGLShadows::KMaxPointShadows; ++slot)
    {
        if (point_light_indices_[slot] == std::numeric_limits<size_t>::max())
        {
            glDeleteTextures(1, &point_textures_[slot]);
            glGenTextures(1, &point_textures_[slot]);
            glBindTexture(GL_TEXTURE_CUBE_MAP, point_textures_[slot]);
            for (int face = 0; face < 6; ++face)
            {
                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, resolution, resolution, 0,
                             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            }
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            point_light_indices_[slot] = light_index;
            point_resolutions_[slot] = resolution;
            point_valid_[slot] = false;
            point_slot_used_[slot] = true;
            return slot;
        }
    }

    return -1;
}

void OpenGLShadowSystem::Render(const std::vector<OpenGLDrawItem> &draw_items, const OpenGLRenderQueues &queues)
{
    if (atlas_texture_ == 0 || depth_fbo_ == 0)
    {
        return;
    }

    gpu_data_ = OpenGLShadowGpuData();
    point_slot_used_.fill(false);
    atlas_cursor_x_ = 0;
    atlas_cursor_y_ = 0;
    atlas_row_height_ = 0;

    const std::vector<Light> &lights = rendering_->GetDirectLights();
    std::vector<LightShadowBinding> handles(lights.size());
    const std::uint64_t mesh_instance_revision = rendering_->GetMeshInstanceRevision();
    const std::uint64_t light_state_revision = rendering_->GetLightStateRevision();
    shadow_content_changed_ = mesh_instance_revision != cached_mesh_instance_revision_ ||
                              light_state_revision != cached_light_state_revision_;

    for (size_t light_index = 0; light_index < lights.size(); ++light_index)
    {
        const Light &light = lights[light_index];
        if (!light.enabled || !light.casts_shadows)
        {
            continue;
        }

        switch (light.type)
        {
        case LightType::Spot:
            RenderSpotShadow(light_index, light, draw_items, queues, handles);
            break;
        case LightType::Directional:
            RenderDirectionalShadow(light_index, light, draw_items, queues, handles);
            break;
        case LightType::Point:
            RenderPointShadow(light_index, light, draw_items, queues, handles);
            break;
        }
    }

    for (int slot = 0; slot < OpenGLShadows::KMaxPointShadows; ++slot)
    {
        if (point_slot_used_[slot])
        {
            continue;
        }
        point_light_indices_[slot] = std::numeric_limits<size_t>::max();
        point_resolutions_[slot] = 0;
        point_valid_[slot] = false;
    }
    cached_mesh_instance_revision_ = mesh_instance_revision;
    cached_light_state_revision_ = light_state_revision;
    gpu_data_.shadow_counts = glm::vec4(
        static_cast<float>(OpenGLShadows::KMaxSpotShadows), static_cast<float>(OpenGLShadows::KMaxDirectionalCascades),
        static_cast<float>(OpenGLShadows::KMaxPointShadows), static_cast<float>(OpenGLShadows::KAtlasSize));
    rendering_->SetLightShadowHandles(handles);
}

void OpenGLShadowSystem::RenderSpotShadow(size_t light_index, const Light &light,
                                          const std::vector<OpenGLDrawItem> &draw_items,
                                          const OpenGLRenderQueues &queues, std::vector<LightShadowBinding> &handles)
{
    int spot_index = -1;
    for (int index = 0; index < OpenGLShadows::KMaxSpotShadows; ++index)
    {
        if (gpu_data_.spot_params[index].z == 0.0f)
        {
            spot_index = index;
            break;
        }
    }
    if (spot_index < 0)
    {
        return;
    }

    const int resolution = ClampShadowResolution(light.shadow_resolution);
    const AtlasTile tile = AllocateAtlasTile(resolution);
    if (tile.size == 0)
    {
        return;
    }

    const float far_plane = light.range > 0.0f ? light.range : 50.0f;
    const float fov = std::max(1.0f, SpotOuterDegrees(light) * 2.0f);
    const glm::mat4 view = LookAlong(light.position, light.direction);
    const glm::mat4 projection = glm::perspective(glm::radians(fov), 1.0f, 0.05f, far_plane);

    gpu_data_.spot_matrices[spot_index] = projection * view;
    gpu_data_.spot_atlas_rects[spot_index] = AtlasRect(tile);
    gpu_data_.spot_params[spot_index] =
        glm::vec4(light.shadow_bias, light.shadow_normal_bias, static_cast<float>(resolution), far_plane);
    handles[light_index] = {spot_index, OpenGLShadows::KTypeSpot};

    const glm::mat4 matrix = projection * view;
    const glm::vec4 rect = gpu_data_.spot_atlas_rects[spot_index];
    if (shadow_content_changed_ || !cached_spot_valid_[spot_index] ||
        !SameMatrix(cached_spot_matrices_[spot_index], matrix) || !SameRect(cached_spot_rects_[spot_index], rect))
    {
        RenderDepthToAtlas(tile, view, projection, draw_items, queues);
        cached_spot_matrices_[spot_index] = matrix;
        cached_spot_rects_[spot_index] = rect;
        cached_spot_valid_[spot_index] = true;
    }
}

void OpenGLShadowSystem::RenderDirectionalShadow(size_t light_index, const Light &light,
                                                 const std::vector<OpenGLDrawItem> &draw_items,
                                                 const OpenGLRenderQueues &queues,
                                                 std::vector<LightShadowBinding> &handles)
{
    int cascade_base = -1;
    for (int index = 0; index <= OpenGLShadows::KMaxDirectionalCascades - OpenGLShadows::KCascadeCount;
         index += OpenGLShadows::KCascadeCount)
    {
        if (gpu_data_.cascade_params[index].x == 0.0f)
        {
            cascade_base = index;
            break;
        }
    }
    if (cascade_base < 0)
    {
        return;
    }

    const RenderFrameConstants &constants = rendering_->GetFrameConstants();
    const glm::mat4 inverse_view = glm::inverse(constants.view);
    const float near_plane = std::max(0.01f, ProjectionNear(constants.proj));
    const float far_plane =
        std::min(std::max(near_plane + 1.0f, ProjectionFar(constants.proj)), KDirectionalShadowDistance);
    std::array<float, OpenGLShadows::KCascadeCount + 1> splits{};
    splits[0] = near_plane;
    for (int cascade = 1; cascade <= OpenGLShadows::KCascadeCount; ++cascade)
    {
        const float ratio = static_cast<float>(cascade) / static_cast<float>(OpenGLShadows::KCascadeCount);
        const float log_split = near_plane * std::pow(far_plane / near_plane, ratio);
        const float linear_split = near_plane + ((far_plane - near_plane) * ratio);
        splits[cascade] = (KCascadeSplitLambda * log_split) + ((1.0f - KCascadeSplitLambda) * linear_split);
    }

    const glm::vec3 light_direction = NormalizeOrDefault(light.direction, glm::vec3(-0.4f, -1.0f, -0.2f));
    const int resolution = ClampShadowResolution(light.shadow_resolution);
    std::vector<Bounds> caster_bounds;
    caster_bounds.reserve(queues.shadow_casters.size());
    for (uint32_t draw_index : queues.shadow_casters)
    {
        caster_bounds.push_back(draw_items[draw_index].world_bounds);
    }

    for (int cascade = 0; cascade < OpenGLShadows::KCascadeCount; ++cascade)
    {
        const AtlasTile tile = AllocateAtlasTile(resolution);
        if (tile.size == 0)
        {
            return;
        }

        const float overlap = cascade == 0 ? 0.0f : CascadeBlendRange(splits[cascade], false);
        const float slice_near = std::max(near_plane, splits[cascade] - overlap);
        const std::array<glm::vec3, 8> corners =
            FrustumSliceCorners(inverse_view, constants.proj, slice_near, splits[cascade + 1]);
        const DirectionalShadowCascade cascade_matrices = BuildDirectionalShadowCascade(
            corners, light_direction, resolution, caster_bounds, KDirectionalShadowDistance);
        const glm::mat4 &view = cascade_matrices.view;
        const glm::mat4 &projection = cascade_matrices.projection;
        const int cascade_index = cascade_base + cascade;

        gpu_data_.cascade_matrices[cascade_index] = projection * view;
        gpu_data_.cascade_atlas_rects[cascade_index] = AtlasRect(tile);
        gpu_data_.cascade_params[cascade_index] =
            glm::vec4(splits[cascade + 1], light.shadow_bias, light.shadow_normal_bias,
                      CascadeBlendRange(splits[cascade + 1], cascade == OpenGLShadows::KCascadeCount - 1));

        const glm::mat4 matrix = projection * view;
        const glm::vec4 rect = gpu_data_.cascade_atlas_rects[cascade_index];
        if (shadow_content_changed_ || !cached_cascade_valid_[cascade_index] ||
            !SameMatrix(cached_cascade_matrices_[cascade_index], matrix) ||
            !SameRect(cached_cascade_rects_[cascade_index], rect))
        {
            RenderDepthToAtlas(tile, view, projection, draw_items, queues);
            cached_cascade_matrices_[cascade_index] = matrix;
            cached_cascade_rects_[cascade_index] = rect;
            cached_cascade_valid_[cascade_index] = true;
        }
    }

    handles[light_index] = {cascade_base, OpenGLShadows::KTypeDirectional};
}

void OpenGLShadowSystem::RenderPointShadow(size_t light_index, const Light &light,
                                           const std::vector<OpenGLDrawItem> &draw_items,
                                           const OpenGLRenderQueues &queues, std::vector<LightShadowBinding> &handles)
{
    const int resolution = ClampShadowResolution(light.shadow_resolution);
    const int slot = AllocatePointSlot(light_index, resolution);
    if (slot < 0)
    {
        return;
    }

    const bool should_update = shadow_content_changed_ || !point_valid_[slot];
    const float far_plane = light.range > 0.0f ? light.range : 25.0f;
    gpu_data_.point_params[slot] = glm::vec4(light.position, far_plane);
    gpu_data_.point_shadow_params[slot] =
        glm::vec4(light.shadow_bias, light.shadow_normal_bias, static_cast<float>(resolution), 0.0f);
    handles[light_index] = {slot, OpenGLShadows::KTypePoint};

    if (!should_update)
    {
        return;
    }
    point_valid_[slot] = true;

    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, far_plane);
    const std::array<glm::mat4, 6> views = {
        glm::lookAt(light.position, light.position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(light.position, light.position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(light.position, light.position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(light.position, light.position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(light.position, light.position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(light.position, light.position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    for (int face = 0; face < 6; ++face)
    {
        RenderPointFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, point_textures_[slot], resolution, views[face],
                        projection, light.position, far_plane, draw_items, queues);
    }
}

void OpenGLShadowSystem::RenderDepthToAtlas(const AtlasTile &tile, const glm::mat4 &view, const glm::mat4 &projection,
                                            const std::vector<OpenGLDrawItem> &draw_items,
                                            const OpenGLRenderQueues &queues)
{
    assert(depth_only_ != nullptr);
    const Frustum light_frustum = ExtractFrustum(projection * view);
    depth_only_->Use();
    depth_only_->UpdateFrame(view, projection);

    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, atlas_texture_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(tile.x, tile.y, tile.size, tile.size);
    glEnable(GL_SCISSOR_TEST);
    glScissor(tile.x, tile.y, tile.size, tile.size);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.1f, 2.0f);

    GLuint bound_vertex_array = 0;
    for (uint32_t draw_index : queues.shadow_casters)
    {
        const OpenGLDrawItem &item = draw_items[draw_index];
        if (!IntersectsFrustum(item.world_bounds, light_frustum))
        {
            continue;
        }
        if (bound_vertex_array != item.vertex_array_obj)
        {
            glBindVertexArray(item.vertex_array_obj);
            bound_vertex_array = item.vertex_array_obj;
        }
        depth_only_->UpdateObject(item.transform, *item.material, item.albedo_tex);
        glDrawElements(GL_TRIANGLES, item.count, GL_UNSIGNED_INT, nullptr);
    }

    glCullFace(GL_BACK);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SCISSOR_TEST);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLShadowSystem::RenderPointFace(GLenum face, GLuint texture, int resolution, const glm::mat4 &view,
                                         const glm::mat4 &projection, const glm::vec3 &position, float far_plane,
                                         const std::vector<OpenGLDrawItem> &draw_items,
                                         const OpenGLRenderQueues &queues)
{
    assert(point_depth_ != nullptr);
    const Frustum light_frustum = ExtractFrustum(projection * view);
    point_depth_->Use();
    point_depth_->UpdateFrame(view, projection, position, far_plane);

    glBindFramebuffer(GL_FRAMEBUFFER, depth_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, face, texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    GLuint bound_vertex_array = 0;
    for (uint32_t draw_index : queues.shadow_casters)
    {
        const OpenGLDrawItem &item = draw_items[draw_index];
        if (!IntersectsFrustum(item.world_bounds, light_frustum))
        {
            continue;
        }
        if (bound_vertex_array != item.vertex_array_obj)
        {
            glBindVertexArray(item.vertex_array_obj);
            bound_vertex_array = item.vertex_array_obj;
        }
        point_depth_->UpdateObject(item.transform, *item.material, item.albedo_tex);
        glDrawElements(GL_TRIANGLES, item.count, GL_UNSIGNED_INT, nullptr);
    }

    glCullFace(GL_BACK);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace CEngine::Renderer
