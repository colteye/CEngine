//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/render_backend.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/opengl/render_backend.h"

#include "renderer/mesh.h"
#include "renderer/opengl/texture.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{

namespace
{
/**
 * @brief TODO: Describe ConfigureFramebufferTexture.
 *
 * @param texture TODO: Describe this parameter.
 * @param internal_format TODO: Describe this parameter.
 * @param format TODO: Describe this parameter.
 * @param type TODO: Describe this parameter.
 * @param width TODO: Describe this parameter.
 * @param height TODO: Describe this parameter.
 */
void ConfigureFramebufferTexture(GLuint texture, GLenum internal_format, GLenum format, GLenum type, int width,
                                 int height)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_format), width, height, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/**
 * @brief TODO: Describe BoundsCenter.
 *
 * @param bounds TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
glm::vec3 BoundsCenter(const Bounds &bounds, const glm::mat4 &transform)
{
    if (!bounds.valid)
    {
        return {transform[3]};
    }
    return (bounds.min + bounds.max) * 0.5f;
}
} // namespace

/**
 * @brief TODO: Describe EnvironmentResources::Destroy.
 */
void EnvironmentResources::Destroy()
{
    panorama_to_cube.reset();
    irradiance.reset();
    prefilter.reset();
    skybox.reset();
    const GLuint textures[] = {panorama, environment_map, irradiance_map, prefiltered_map};
    glDeleteTextures(4, textures);
    panorama = environment_map = irradiance_map = prefiltered_map = 0;
    if (capture_fbo != 0)
    {
        glDeleteFramebuffers(1, &capture_fbo);
    }
    if (capture_rbo != 0)
    {
        glDeleteRenderbuffers(1, &capture_rbo);
    }
    if (cube_vbo != 0)
    {
        glDeleteBuffers(1, &cube_vbo);
    }
    if (cube_vao != 0)
    {
        glDeleteVertexArrays(1, &cube_vao);
    }
    capture_fbo = capture_rbo = cube_vbo = cube_vao = 0;
    skybox_view = -1;
    skybox_projection = -1;
    skybox_intensity = -1;
    skybox_rotation = -1;
    skybox_environment_map = -1;
}

/**
 * @brief TODO: Describe MaterialResources::Destroy.
 */
void MaterialResources::Destroy()
{
    if (owns_albedo_tex && albedo_tex != 0)
    {
        glDeleteTextures(1, &albedo_tex);
    }
    if (owns_normal_tex && normal_tex != 0)
    {
        glDeleteTextures(1, &normal_tex);
    }
    if (owns_metallic_roughness_ao_tex && metallic_roughness_ao_tex != 0)
    {
        glDeleteTextures(1, &metallic_roughness_ao_tex);
    }
    albedo_tex = 0;
    normal_tex = 0;
    metallic_roughness_ao_tex = 0;
    owns_albedo_tex = false;
    owns_normal_tex = false;
    owns_metallic_roughness_ao_tex = false;
}

/**
 * @brief TODO: Describe MeshResources::MeshResources.
 *
 * @param other TODO: Describe this parameter.
 */
MeshResources::MeshResources(MeshResources &&other) noexcept
{
    *this = std::move(other);
}

/**
 * @brief TODO: Describe operator=.
 *
 * @param other TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
MeshResources &MeshResources::operator=(MeshResources &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    Destroy();

    vertex_array_obj = other.vertex_array_obj;
    vertex_buffer = other.vertex_buffer;
    index_buffer = other.index_buffer;
    index_count = other.index_count;
    references = other.references;

    other.vertex_array_obj = 0;
    other.vertex_buffer = 0;
    other.index_buffer = 0;
    other.index_count = 0;
    other.references = 0;

    return *this;
}

/**
 * @brief TODO: Describe MeshResources::~MeshResources.
 */
MeshResources::~MeshResources()
{
    Destroy();
}

/**
 * @brief TODO: Describe FrameResources::Destroy.
 */
void FrameResources::Destroy()
{
    if (g_albedo != 0)
    {
        glDeleteTextures(1, &g_albedo);
        g_albedo = 0;
    }
    if (g_normal_roughness != 0)
    {
        glDeleteTextures(1, &g_normal_roughness);
        g_normal_roughness = 0;
    }
    if (g_material != 0)
    {
        glDeleteTextures(1, &g_material);
        g_material = 0;
    }
    if (g_baked_light != 0)
    {
        glDeleteTextures(1, &g_baked_light);
        g_baked_light = 0;
    }
    if (scene_depth != 0)
    {
        glDeleteTextures(1, &scene_depth);
        scene_depth = 0;
    }
    if (opaque_lit_color != 0)
    {
        glDeleteTextures(1, &opaque_lit_color);
        opaque_lit_color = 0;
    }
    if (ssao != 0)
    {
        glDeleteTextures(1, &ssao);
        ssao = 0;
    }
    if (scene_color != 0)
    {
        glDeleteTextures(1, &scene_color);
        scene_color = 0;
    }
    if (g_buffer_fbo != 0)
    {
        glDeleteFramebuffers(1, &g_buffer_fbo);
        g_buffer_fbo = 0;
    }
    if (opaque_lit_fbo != 0)
    {
        glDeleteFramebuffers(1, &opaque_lit_fbo);
        opaque_lit_fbo = 0;
    }
    if (ssao_fbo != 0)
    {
        glDeleteFramebuffers(1, &ssao_fbo);
        ssao_fbo = 0;
    }
    if (scene_color_fbo != 0)
    {
        glDeleteFramebuffers(1, &scene_color_fbo);
        scene_color_fbo = 0;
    }
}

/**
 * @brief TODO: Describe RenderQueues::ClearAndReserve.
 *
 * @param draw_item_count TODO: Describe this parameter.
 */
void RenderQueues::ClearAndReserve(size_t draw_item_count)
{
    opaque_deferred.clear();
    forward.clear();
    transparent.clear();
    shadow_casters.clear();

    if (opaque_deferred.capacity() < draw_item_count)
    {
        opaque_deferred.reserve(draw_item_count);
        forward.reserve(draw_item_count);
        transparent.reserve(draw_item_count);
        shadow_casters.reserve(draw_item_count);
    }
}

/**
 * @brief TODO: Describe MeshResources::Destroy.
 */
void MeshResources::Destroy()
{
    if (vertex_buffer != 0)
    {
        glDeleteBuffers(1, &vertex_buffer);
        vertex_buffer = 0;
    }
    if (index_buffer != 0)
    {
        glDeleteBuffers(1, &index_buffer);
        index_buffer = 0;
    }
    if (vertex_array_obj != 0)
    {
        glDeleteVertexArrays(1, &vertex_array_obj);
        vertex_array_obj = 0;
    }

    index_count = 0;
    references = 0;
}

/**
 * @brief TODO: Describe RenderBackend::Initialize.
 *
 * @param in_rendering TODO: Describe this parameter.
 * @param in_window_width TODO: Describe this parameter.
 * @param in_window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::Initialize(RenderSystem &in_rendering, Window::WindowSystem & /*window*/, int in_window_width,
                               int in_window_height)
{
    rendering_ = &in_rendering;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    if (!CreateFrameResources(in_window_width, in_window_height))
    {
        DestroyFrameResources();
        return false;
    }
    if (!shadow_system_.Initialize(*rendering_))
    {
        DestroyFrameResources();
        return false;
    }
    if (!ui_renderer_.Initialize())
    {
        shadow_system_.Destroy();
        DestroyFrameResources();
        return false;
    }
    default_white_texture_ = TextureLoader::CreateSolid(255, 255, 255);
    default_normal_texture_ = TextureLoader::CreateSolid(128, 128, 255);
    if (default_white_texture_ == 0 || default_normal_texture_ == 0)
    {
        if (default_white_texture_ != 0)
        {
            glDeleteTextures(1, &default_white_texture_);
        }
        if (default_normal_texture_ != 0)
        {
            glDeleteTextures(1, &default_normal_texture_);
        }
        default_white_texture_ = 0;
        default_normal_texture_ = 0;
        shadow_system_.Destroy();
        DestroyFrameResources();
        return false;
    }

    shader_passes_.ssao = std::make_unique<SSAO>();
    shader_passes_.ssao->SetTextures(frame_resources_.opaque_lit_color, frame_resources_.scene_depth,
                                     frame_resources_.g_normal_roughness, frame_resources_.ssao, in_window_width,
                                     in_window_height);

    window_width_ = in_window_width;
    window_height_ = in_window_height;
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::Shutdown.
 */
void RenderBackend::Shutdown()
{
    ui_renderer_.Shutdown();
    environment_resources_.Destroy();
    shader_passes_ = ShaderPasses();

    for (auto &it : material_resources_)
    {
        it.second.Destroy();
    }
    material_resources_.clear();
    if (default_white_texture_ != 0)
    {
        glDeleteTextures(1, &default_white_texture_);
    }
    if (default_normal_texture_ != 0)
    {
        glDeleteTextures(1, &default_normal_texture_);
    }
    default_white_texture_ = 0;
    default_normal_texture_ = 0;
    for (const auto &lightmap : lightmap_resources_)
    {
        if (lightmap.second.texture != 0)
        {
            glDeleteTextures(1, &lightmap.second.texture);
        }
    }
    lightmap_resources_.clear();
    mesh_resources_.clear();

    if (quad_VBO_ != 0)
    {
        glDeleteBuffers(1, &quad_VBO_);
        quad_VBO_ = 0;
    }
    if (quad_VAO_ != 0)
    {
        glDeleteVertexArrays(1, &quad_VAO_);
        quad_VAO_ = 0;
    }
    DestroyFrameResources();
    shadow_system_.Destroy();

    for (DrawItem &item : draw_items_)
    {
        if (item.joint_palette_texture != 0)
        {
            glDeleteTextures(1, &item.joint_palette_texture);
        }
        if (item.joint_palette_buffer != 0)
        {
            glDeleteBuffers(1, &item.joint_palette_buffer);
        }
    }
    draw_items_.clear();
    camera_distance_squared_.clear();
    render_queues_.ClearAndReserve(0);
    window_width_ = 0;
    window_height_ = 0;
    rendering_ = nullptr;
}

/**
 * @brief TODO: Describe RenderBackend::Resize.
 *
 * @param in_window_width TODO: Describe this parameter.
 * @param in_window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::Resize(int in_window_width, int in_window_height)
{
    if (in_window_width <= 0 || in_window_height <= 0)
    {
        return false;
    }
    if (in_window_width == window_width_ && in_window_height == window_height_)
    {
        return true;
    }

    DestroyFrameResources();
    if (!CreateFrameResources(in_window_width, in_window_height))
    {
        DestroyFrameResources();
        return false;
    }
    if (shader_passes_.ssao != nullptr)
    {
        shader_passes_.ssao->SetTextures(frame_resources_.opaque_lit_color, frame_resources_.scene_depth,
                                         frame_resources_.g_normal_roughness, frame_resources_.ssao, in_window_width,
                                         in_window_height);
    }
    window_width_ = in_window_width;
    window_height_ = in_window_height;
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::RegisterMeshInstance.
 *
 * @param slot TODO: Describe this parameter.
 * @param mesh_instance TODO: Describe this parameter.
 * @param world_bounds TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                                         const Bounds &world_bounds)
{
    assert(mesh_instance.mesh != nullptr);
    assert(mesh_instance.material != nullptr);

    const Material *material = mesh_instance.material.get();
    if (!RegisterMaterial(material))
    {
        return false;
    }
    if (mesh_instance.lightmap != nullptr && !RegisterLightmap(mesh_instance.lightmap.get()))
    {
        RemoveMaterial(material);
        return false;
    }

    auto mesh = mesh_resources_.find(mesh_instance.mesh.get());
    if (mesh == mesh_resources_.end())
    {
        MeshResources uploaded = UploadMesh(*mesh_instance.mesh);
        if (uploaded.index_count == 0)
        {
            if (mesh_instance.lightmap != nullptr)
            {
                RemoveLightmap(mesh_instance.lightmap.get());
            }
            RemoveMaterial(material);
            return false;
        }
        mesh = mesh_resources_.emplace(mesh_instance.mesh.get(), std::move(uploaded)).first;
    }

    DrawItem draw_item;
    draw_item.mesh = mesh_instance.mesh.get();
    draw_item.material = material;
    draw_item.lightmap = mesh_instance.lightmap.get();
    draw_item.transform = mesh_instance.transform;
    draw_item.world_bounds = world_bounds;
    draw_item.count = mesh->second.index_count;
    draw_item.flags = mesh_instance.flags;
    draw_item.vertex_array_obj = mesh->second.vertex_array_obj;
    const MaterialResources &resources = material_resources_.at(material);
    draw_item.albedo_tex = resources.albedo_tex;
    draw_item.normal_tex = resources.normal_tex;
    draw_item.metallic_roughness_ao_tex = resources.metallic_roughness_ao_tex;
    if (mesh_instance.lightmap != nullptr)
    {
        const auto lightmap = lightmap_resources_.find(mesh_instance.lightmap.get());
        if (lightmap != lightmap_resources_.end())
        {
            draw_item.lightmap_tex = lightmap->second.texture;
        }
    }
    draw_item.lightmap_scale = mesh_instance.lightmap_scale;
    draw_item.lightmap_offset = mesh_instance.lightmap_offset;
    draw_item.lightmap_rgbm_range = mesh_instance.lightmap_rgbm_range;
    if (slot >= draw_items_.size())
    {
        draw_items_.resize(static_cast<std::size_t>(slot) + 1);
    }
    draw_items_[slot] = draw_item;
    ++mesh->second.references;
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::UpdateMeshInstance.
 *
 * @param slot TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @param world_bounds TODO: Describe this parameter.
 * @param flags TODO: Describe this parameter.
 */
void RenderBackend::UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                                       std::uint32_t flags)
{
    if (slot >= draw_items_.size())
    {
        return;
    }

    draw_items_[slot].transform = transform;
    draw_items_[slot].world_bounds = world_bounds;
    draw_items_[slot].flags = flags;
}

bool RenderBackend::UpdateSkinningPalette(
    std::uint32_t slot, std::span<const glm::mat4> palette)
{
    if (slot >= draw_items_.size() || draw_items_[slot].mesh == nullptr ||
        !draw_items_[slot].mesh->skinned || palette.empty() ||
        palette.size() > 1024u)
    {
        return false;
    }
    DrawItem &item = draw_items_[slot];
    while (glGetError() != GL_NO_ERROR)
    {
    }
    if (item.joint_palette_buffer == 0)
    {
        glGenBuffers(1, &item.joint_palette_buffer);
    }
    if (item.joint_palette_texture == 0)
    {
        glGenTextures(1, &item.joint_palette_texture);
    }
    glBindBuffer(GL_TEXTURE_BUFFER, item.joint_palette_buffer);
    glBufferData(GL_TEXTURE_BUFFER,
                 static_cast<GLsizeiptr>(palette.size_bytes()),
                 palette.data(), GL_STREAM_DRAW);
    glBindTexture(GL_TEXTURE_BUFFER, item.joint_palette_texture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, item.joint_palette_buffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    if (glGetError() != GL_NO_ERROR)
    {
        return false;
    }
    item.joint_count = static_cast<std::uint32_t>(palette.size());
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::RemoveMeshInstance.
 *
 * @param slot TODO: Describe this parameter.
 */
void RenderBackend::RemoveMeshInstance(std::uint32_t slot)
{
    if (slot >= draw_items_.size())
    {
        return;
    }
    const Mesh *mesh_resource = draw_items_[slot].mesh;
    if (mesh_resource != nullptr)
    {
        const auto mesh = mesh_resources_.find(mesh_resource);
        if (mesh != mesh_resources_.end())
        {
            assert(mesh->second.references > 0);
            if (--mesh->second.references == 0)
            {
                mesh_resources_.erase(mesh);
            }
        }
    }
    if (draw_items_[slot].lightmap != nullptr)
    {
        RemoveLightmap(draw_items_[slot].lightmap);
    }
    if (draw_items_[slot].material != nullptr)
    {
        RemoveMaterial(draw_items_[slot].material);
    }
    if (draw_items_[slot].joint_palette_texture != 0)
    {
        glDeleteTextures(1, &draw_items_[slot].joint_palette_texture);
    }
    if (draw_items_[slot].joint_palette_buffer != 0)
    {
        glDeleteBuffers(1, &draw_items_[slot].joint_palette_buffer);
    }
    draw_items_[slot] = {};
}

/**
 * @brief TODO: Describe RenderBackend::UploadMesh.
 *
 * @param mesh TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
MeshResources RenderBackend::UploadMesh(const Mesh &mesh)
{
    MeshResources buffers;
    if (mesh.Empty())
    {
        return buffers;
    }
    const MeshLod &lod = mesh.lods.front();

    while (glGetError() != GL_NO_ERROR)
    {
    }

    glGenVertexArrays(1, &buffers.vertex_array_obj);
    glBindVertexArray(buffers.vertex_array_obj);

    glGenBuffers(1, &buffers.vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffers.vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lod.vertices.size() * sizeof(MeshVertex)),
                 lod.vertices.data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(MeshVertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(MeshVertex, position)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(MeshVertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(MeshVertex, uv)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(offsetof(MeshVertex, lightmap_uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(offsetof(MeshVertex, tangent)));
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_UNSIGNED_SHORT, stride,
                          reinterpret_cast<void *>(offsetof(MeshVertex, joints)));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void *>(offsetof(MeshVertex, weights)));

    glGenBuffers(1, &buffers.index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(lod.indices.size() * sizeof(std::uint32_t)),
                 lod.indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    if (glGetError() != GL_NO_ERROR)
    {
        buffers.Destroy();
        return {};
    }
    buffers.index_count = static_cast<GLsizei>(lod.indices.size());
    return buffers;
}

/**
 * @brief TODO: Describe RenderBackend::RegisterMaterial.
 *
 * @param material TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::RegisterMaterial(const Material *material)
{
    assert(material != nullptr);
    const auto existing = material_resources_.find(material);
    if (existing != material_resources_.end())
    {
        ++existing->second.references;
        return true;
    }

    MaterialResources resources;
    resources.references = 1;
    resources.owns_albedo_tex = !material->albedo.Empty();
    resources.owns_normal_tex = !material->normal.Empty();
    resources.owns_metallic_roughness_ao_tex = !material->metallic_roughness_ao.Empty();
    resources.albedo_tex = resources.owns_albedo_tex
                               ? TextureLoader::Load(material->albedo, TextureColorSpace::Srgb)
                               : default_white_texture_;
    resources.normal_tex = resources.owns_normal_tex
                               ? TextureLoader::Load(material->normal, TextureColorSpace::Linear)
                               : default_normal_texture_;
    resources.metallic_roughness_ao_tex = resources.owns_metallic_roughness_ao_tex
                                              ? TextureLoader::Load(material->metallic_roughness_ao,
                                                                    TextureColorSpace::Linear)
                                              : default_white_texture_;
    if (resources.albedo_tex == 0 || resources.normal_tex == 0 || resources.metallic_roughness_ao_tex == 0)
    {
        resources.Destroy();
        return false;
    }
    material_resources_.emplace(material, resources);
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::RemoveMaterial.
 *
 * @param material TODO: Describe this parameter.
 */
void RenderBackend::RemoveMaterial(const Material *material)
{
    const auto found = material_resources_.find(material);
    if (found == material_resources_.end())
    {
        return;
    }
    assert(found->second.references > 0);
    if (--found->second.references != 0)
    {
        return;
    }
    found->second.Destroy();
    material_resources_.erase(found);
}

/**
 * @brief TODO: Describe RenderBackend::RegisterLightmap.
 *
 * @param lightmap TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::RegisterLightmap(const Texture *lightmap)
{
    assert(lightmap != nullptr);
    const auto found = lightmap_resources_.find(lightmap);
    if (found != lightmap_resources_.end())
    {
        ++found->second.references;
        return true;
    }

    const GLuint texture = TextureLoader::Load(*lightmap, TextureColorSpace::Linear);
    if (texture == 0)
    {
        return false;
    }
    lightmap_resources_.emplace(lightmap, LightmapResources{texture, 1});
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::RemoveLightmap.
 *
 * @param lightmap TODO: Describe this parameter.
 */
void RenderBackend::RemoveLightmap(const Texture *lightmap)
{
    const auto found = lightmap_resources_.find(lightmap);
    if (found == lightmap_resources_.end())
    {
        return;
    }
    if (--found->second.references != 0)
    {
        return;
    }
    if (found->second.texture != 0)
    {
        glDeleteTextures(1, &found->second.texture);
    }
    lightmap_resources_.erase(found);
}

/**
 * @brief TODO: Describe RenderBackend::GetShader.
 *
 * @param shader_type TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
PBRStandard *RenderBackend::GetShader(MaterialShaderType shader_type)
{
    switch (shader_type)
    {
    case MaterialShaderType::PBRStandard:
        if (shader_passes_.pbr_forward == nullptr)
        {
            shader_passes_.pbr_forward = std::make_unique<PBRStandard>();
        }
        return shader_passes_.pbr_forward.get();
    case MaterialShaderType::Unknown:
    case MaterialShaderType::Unlit:
        return nullptr;
    }

    return nullptr;
}

/**
 * @brief TODO: Describe RenderBackend::CreateFrameResources.
 *
 * @param width TODO: Describe this parameter.
 * @param height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::CreateFrameResources(int width, int height)
{
    FrameResources &resources = frame_resources_;

    glGenFramebuffers(1, &resources.g_buffer_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.g_buffer_fbo);

    glGenTextures(1, &resources.g_albedo);
    ConfigureFramebufferTexture(resources.g_albedo, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.g_albedo, 0);

    glGenTextures(1, &resources.g_normal_roughness);
    ConfigureFramebufferTexture(resources.g_normal_roughness, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, resources.g_normal_roughness, 0);

    glGenTextures(1, &resources.g_material);
    ConfigureFramebufferTexture(resources.g_material, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, resources.g_material, 0);

    glGenTextures(1, &resources.g_baked_light);
    ConfigureFramebufferTexture(resources.g_baked_light, GL_RGB16F, GL_RGB, GL_FLOAT, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, resources.g_baked_light, 0);

    glGenTextures(1, &resources.scene_depth);
    ConfigureFramebufferTexture(resources.scene_depth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, width,
                                height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, resources.scene_depth, 0);

    const GLenum g_buffer_attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                            GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, g_buffer_attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "OpenGL G-buffer framebuffer is incomplete.\n";
        return false;
    }

    glGenFramebuffers(1, &resources.opaque_lit_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.opaque_lit_fbo);

    glGenTextures(1, &resources.opaque_lit_color);
    ConfigureFramebufferTexture(resources.opaque_lit_color, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.opaque_lit_color, 0);

    const GLenum color_attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &color_attachment);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "OpenGL opaque lighting framebuffer is incomplete.\n";
        return false;
    }

    glGenFramebuffers(1, &resources.ssao_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.ssao_fbo);
    glGenTextures(1, &resources.ssao);
    ConfigureFramebufferTexture(resources.ssao, GL_R8, GL_RED, GL_UNSIGNED_BYTE, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.ssao, 0);
    glDrawBuffers(1, &color_attachment);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "OpenGL SSAO framebuffer is incomplete.\n";
        return false;
    }

    glGenFramebuffers(1, &resources.scene_color_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.scene_color_fbo);

    glGenTextures(1, &resources.scene_color);
    ConfigureFramebufferTexture(resources.scene_color, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.scene_color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, resources.scene_depth, 0);
    glDrawBuffers(1, &color_attachment);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "OpenGL scene color framebuffer is incomplete.\n";
        return false;
    }

    std::cout << "OpenGL G-buffer ready: albedo, normal/roughness, material/flags, baked light, depth.\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

/**
 * @brief TODO: Describe RenderBackend::DestroyFrameResources.
 */
void RenderBackend::DestroyFrameResources()
{
    frame_resources_.Destroy();
}

/**
 * @brief TODO: Describe RenderBackend::BuildRenderQueues.
 */
void RenderBackend::BuildRenderQueues()
{
    render_queues_.ClearAndReserve(draw_items_.size());
    const CameraFrameData &frame = rendering_->GetCameraFrameData();
    const Frustum camera_frustum = ExtractFrustum(frame.proj * frame.view);
    camera_distance_squared_.resize(draw_items_.size());

    for (uint32_t index = 0; index < draw_items_.size(); ++index)
    {
        const DrawItem &item = draw_items_[index];
        if (item.material == nullptr || item.count == 0)
        {
            continue;
        }
        if (item.mesh != nullptr && item.mesh->skinned && item.joint_count == 0u)
        {
            continue;
        }
        if ((item.flags & MeshInstanceFlagVisible) == 0)
        {
            continue;
        }
        const glm::vec3 camera_offset = BoundsCenter(item.world_bounds, item.transform) - frame.camera_position;
        camera_distance_squared_[index] = glm::dot(camera_offset, camera_offset);
        if ((item.flags & MeshInstanceFlagShadowOnly) == 0 && IntersectsFrustum(item.world_bounds, camera_frustum))
        {
            switch (ClassifyRenderMode(item.material->render_mode))
            {
            case RenderQueue::DeferredOpaque:
                render_queues_.opaque_deferred.push_back(index);
                break;
            case RenderQueue::ForwardOpaque:
                render_queues_.forward.push_back(index);
                break;
            case RenderQueue::Transparent:
                render_queues_.transparent.push_back(index);
                break;
            case RenderQueue::None:
                break;
            }
        }

        if (DrawsShadowCaster(item))
        {
            render_queues_.shadow_casters.push_back(index);
        }
    }

    const auto front_to_back = [&](uint32_t left, uint32_t right) {
        return camera_distance_squared_[left] < camera_distance_squared_[right];
    };
    std::sort(render_queues_.opaque_deferred.begin(), render_queues_.opaque_deferred.end(), front_to_back);
    std::sort(render_queues_.forward.begin(), render_queues_.forward.end(), front_to_back);
    std::sort(render_queues_.transparent.begin(), render_queues_.transparent.end(),
              [&](uint32_t left_index, uint32_t right_index) {
                  return camera_distance_squared_[left_index] > camera_distance_squared_[right_index];
              });
}

/**
 * @brief TODO: Describe RenderBackend::RenderGeometryPass.
 */
void RenderBackend::RenderGeometryPass()
{
    if (shader_passes_.pbr_geometry == nullptr)
    {
        shader_passes_.pbr_geometry = std::make_unique<PBRGeometryPass>();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.g_buffer_fbo);
    glViewport(0, 0, window_width_, window_height_);
    const GLenum attachments[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                                   GL_COLOR_ATTACHMENT3};
    glDrawBuffers(4, attachments);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_passes_.pbr_geometry->Use();
    shader_passes_.pbr_geometry->UpdateFrame(*rendering_);
    GLuint bound_vertex_array = 0;
    for (uint32_t draw_index : render_queues_.opaque_deferred)
    {
        const DrawItem &item = draw_items_[draw_index];

        if (bound_vertex_array != item.vertex_array_obj)
        {
            glBindVertexArray(item.vertex_array_obj);
            bound_vertex_array = item.vertex_array_obj;
        }
        shader_passes_.pbr_geometry->SetTextures(item.albedo_tex, item.normal_tex, item.metallic_roughness_ao_tex,
                                                 item.lightmap_tex);
        shader_passes_.pbr_geometry->UpdateObject(item.transform, *item.material, item.lightmap_scale,
                                                  item.lightmap_offset, item.lightmap_rgbm_range);
        shader_passes_.pbr_geometry->UpdateSkinning(
            item.joint_palette_texture, item.joint_count);
        Draw(item);
    }
    glBindVertexArray(0);
}

/**
 * @brief TODO: Describe RenderBackend::RenderDeferredLightingPass.
 */
void RenderBackend::RenderDeferredLightingPass()
{
    if (shader_passes_.deferred_lighting == nullptr)
    {
        shader_passes_.deferred_lighting = std::make_unique<DeferredLighting>();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.opaque_lit_fbo);
    glViewport(0, 0, window_width_, window_height_);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT);

    shader_passes_.deferred_lighting->Use();
    shader_passes_.deferred_lighting->Update(
        *rendering_, frame_resources_.g_albedo, frame_resources_.g_normal_roughness, frame_resources_.g_material,
        frame_resources_.g_baked_light, frame_resources_.scene_depth, window_width_, window_height_,
        shadow_system_.GetGpuData(), shadow_system_.GetAtlasTexture(), shadow_system_.GetPointTextures(),
        environment_resources_.irradiance_map, environment_resources_.prefiltered_map);
    RenderScreenSpaceQuad();
}

/**
 * @brief TODO: Describe RenderBackend::RenderAmbientOcclusionPass.
 */
void RenderBackend::RenderAmbientOcclusionPass()
{
    assert(shader_passes_.ssao != nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.ssao_fbo);
    glViewport(0, 0, window_width_, window_height_);
    const GLenum attachment = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &attachment);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    shader_passes_.ssao->UseCompute();
    shader_passes_.ssao->UpdateCompute(*rendering_);
    RenderScreenSpaceQuad();

    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.scene_color_fbo);
    shader_passes_.ssao->UseComposite();
    shader_passes_.ssao->UpdateComposite(*rendering_);
    RenderScreenSpaceQuad();
}

/**
 * @brief TODO: Describe RenderBackend::BuildEnvironmentResources.
 *
 * @return TODO: Describe the return value.
 */
bool RenderBackend::BuildEnvironmentResources()
{
    const ImageBasedLighting &lighting = rendering_->GetImageBasedLighting();
    auto &resources = environment_resources_;
    if (lighting.panorama == nullptr)
    {
        return false;
    }
    resources.panorama = TextureLoader::Load(*lighting.panorama, TextureColorSpace::Linear);
    if (resources.panorama == 0)
    {
        return false;
    }
    resources.panorama_to_cube = std::make_unique<ShaderProgram>();
    resources.irradiance = std::make_unique<ShaderProgram>();
    resources.prefilter = std::make_unique<ShaderProgram>();
    resources.skybox = std::make_unique<ShaderProgram>();
    if (!resources.panorama_to_cube->Load("shaders/opengl/environment_cube.vert",
                                          "shaders/opengl/equirectangular_to_cube.frag") ||
        !resources.irradiance->Load("shaders/opengl/environment_cube.vert",
                                    "shaders/opengl/irradiance_convolution.frag") ||
        !resources.prefilter->Load("shaders/opengl/environment_cube.vert",
                                   "shaders/opengl/prefilter_environment.frag") ||
        !resources.skybox->Load("shaders/opengl/skybox.vert", "shaders/opengl/skybox.frag"))
    {
        return false;
    }
    const GLuint skybox_program = resources.skybox->GetId();
    resources.skybox_view = glGetUniformLocation(skybox_program, "view");
    resources.skybox_projection = glGetUniformLocation(skybox_program, "projection");
    resources.skybox_intensity = glGetUniformLocation(skybox_program, "intensity");
    resources.skybox_rotation = glGetUniformLocation(skybox_program, "rotation_radians");
    resources.skybox_environment_map = glGetUniformLocation(skybox_program, "environment_map");

    constexpr float Vertices[] = {-1, -1, -1, -1, -1, 1,  -1, 1,  1,  -1, 1,  1,  -1, -1, -1, -1, 1,  -1, 1,  -1, 1, 1,
                                  -1, -1, 1,  1,  -1, 1,  1,  -1, 1,  1,  1,  1,  -1, 1,  -1, -1, 1,  -1, -1, -1, 1, -1,
                                  -1, 1,  -1, -1, 1,  -1, 1,  -1, -1, 1,  -1, 1,  -1, -1, 1,  1,  1,  1,  1,  1,  1, 1,
                                  1,  1,  -1, -1, 1,  -1, -1, -1, 1,  1,  -1, 1,  1,  1,  1,  1,  1,  1,  -1, 1,  1, -1,
                                  -1, 1,  1,  -1, -1, -1, -1, -1, -1, 1,  -1, -1, 1,  -1, 1,  1,  -1, 1,  -1, -1};
    glGenVertexArrays(1, &resources.cube_vao);
    glGenBuffers(1, &resources.cube_vbo);
    glBindVertexArray(resources.cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertices), Vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glGenFramebuffers(1, &resources.capture_fbo);
    glGenRenderbuffers(1, &resources.capture_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, resources.capture_rbo);

    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const glm::vec3 origin(0.0f);
    const std::array<glm::mat4, 6> views = {glm::lookAt(origin, glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
                                            glm::lookAt(origin, glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
                                            glm::lookAt(origin, glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
                                            glm::lookAt(origin, glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
                                            glm::lookAt(origin, glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
                                            glm::lookAt(origin, glm::vec3(0, 0, -1), glm::vec3(0, -1, 0))};
    auto configure_cube = [](GLuint texture, bool mipmapped) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    };
    auto allocate_cube = [&](GLuint &texture, int size, bool mipmapped) {
        glGenTextures(1, &texture);
        configure_cube(texture, mipmapped);
        for (int face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    };
    auto set_capture_matrices = [&](GLuint program) {
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    };
    auto draw_faces = [&](GLuint program, GLuint target, int size, int mip) {
        glViewport(0, 0, size, size);
        for (int face = 0; face < 6; ++face)
        {
            glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(views[face]));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, target,
                                   mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawEnvironmentCube();
        }
    };

    glDisable(GL_CULL_FACE);
    allocate_cube(resources.environment_map, 512, true);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    resources.panorama_to_cube->Use();
    GLuint program = resources.panorama_to_cube->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resources.panorama);
    glUniform1i(glGetUniformLocation(program, "panorama"), 0);
    draw_faces(program, resources.environment_map, 512, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    allocate_cube(resources.irradiance_map, 32, false);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    resources.irradiance->Use();
    program = resources.irradiance->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glUniform1i(glGetUniformLocation(program, "environment_map"), 0);
    draw_faces(program, resources.irradiance_map, 32, 0);

    glGenTextures(1, &resources.prefiltered_map);
    configure_cube(resources.prefiltered_map, true);
    constexpr int PrefilterSize = 128;
    constexpr int MipCount = 5;
    for (int mip = 0; mip < MipCount; ++mip)
    {
        const int size = PrefilterSize >> mip;
        for (int face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGB16F, size, size, 0, GL_RGB, GL_FLOAT,
                         nullptr);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, MipCount - 1);
    resources.prefilter->Use();
    program = resources.prefilter->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glUniform1i(glGetUniformLocation(program, "environment_map"), 0);
    for (int mip = 0; mip < MipCount; ++mip)
    {
        const int size = PrefilterSize >> mip;
        glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glUniform1f(glGetUniformLocation(program, "roughness"), float(mip) / float(MipCount - 1));
        draw_faces(program, resources.prefiltered_map, size, mip);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width_, window_height_);
    glEnable(GL_CULL_FACE);
    return glGetError() == GL_NO_ERROR;
}

/**
 * @brief TODO: Describe RenderBackend::SyncEnvironmentResources.
 */
void RenderBackend::SyncEnvironmentResources()
{
    if (!rendering_->ConsumeImageBasedLightingResourcesDirty())
    {
        return;
    }
    environment_resources_.Destroy();
    if (rendering_->GetImageBasedLighting().enabled && !BuildEnvironmentResources())
    {
        std::cerr << "Failed to build image-based lighting resources.\n";
        environment_resources_.Destroy();
    }
}

/**
 * @brief TODO: Describe RenderBackend::DrawEnvironmentCube.
 */
void RenderBackend::DrawEnvironmentCube() const
{
    glBindVertexArray(environment_resources_.cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

/**
 * @brief TODO: Describe RenderBackend::RenderSkybox.
 */
void RenderBackend::RenderSkybox()
{
    const ImageBasedLighting &lighting = rendering_->GetImageBasedLighting();
    if (!lighting.enabled || environment_resources_.environment_map == 0 || !environment_resources_.skybox)
    {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.scene_color_fbo);
    glViewport(0, 0, window_width_, window_height_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    environment_resources_.skybox->Use();
    const CameraFrameData &frame = rendering_->GetCameraFrameData();
    glUniformMatrix4fv(environment_resources_.skybox_view, 1, GL_FALSE, glm::value_ptr(frame.view));
    glUniformMatrix4fv(environment_resources_.skybox_projection, 1, GL_FALSE, glm::value_ptr(frame.proj));
    glUniform1f(environment_resources_.skybox_intensity, lighting.sky_intensity);
    glUniform1f(environment_resources_.skybox_rotation, lighting.rotation_radians);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environment_resources_.environment_map);
    glUniform1i(environment_resources_.skybox_environment_map, 0);
    DrawEnvironmentCube();
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

/**
 * @brief TODO: Describe RenderBackend::RenderForwardQueue.
 *
 * @param queue TODO: Describe this parameter.
 * @param transparent TODO: Describe this parameter.
 */
void RenderBackend::RenderForwardQueue(const std::vector<uint32_t> &queue, bool transparent)
{
    if (queue.empty())
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources_.scene_color_fbo);
    glViewport(0, 0, window_width_, window_height_);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    if (transparent)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    else
    {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    GLuint bound_vertex_array = 0;
    PBRStandard *bound_shader = nullptr;
    for (uint32_t draw_index : queue)
    {
        const DrawItem &item = draw_items_[draw_index];
        PBRStandard *shader = GetShader(item.material->shader_type);
        if (shader == nullptr)
        {
            continue;
        }

        if (bound_vertex_array != item.vertex_array_obj)
        {
            glBindVertexArray(item.vertex_array_obj);
            bound_vertex_array = item.vertex_array_obj;
        }
        if (bound_shader != shader)
        {
            shader->Use();
            shader->UpdateFrame(*rendering_, shadow_system_.GetGpuData(), shadow_system_.GetAtlasTexture(),
                                shadow_system_.GetPointTextures(), environment_resources_.irradiance_map,
                                environment_resources_.prefiltered_map);
            bound_shader = shader;
        }
        shader->SetTextures(item.albedo_tex, item.normal_tex, item.metallic_roughness_ao_tex, item.lightmap_tex);
        shader->UpdateObject(item.transform, *item.material, item.lightmap_scale, item.lightmap_offset,
                             item.lightmap_rgbm_range);
        shader->UpdateSkinning(item.joint_palette_texture, item.joint_count);
        Draw(item);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

/**
 * @brief TODO: Describe RenderBackend::PresentSceneColor.
 */
void RenderBackend::PresentSceneColor()
{
    if (shader_passes_.fullscreen_blit == nullptr)
    {
        shader_passes_.fullscreen_blit = std::make_unique<FullscreenBlit>();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width_, window_height_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_passes_.fullscreen_blit->Use();
    shader_passes_.fullscreen_blit->Update(*rendering_, frame_resources_.scene_color, frame_resources_.scene_depth);
    RenderScreenSpaceQuad();
}

/**
 * @brief TODO: Describe RenderBackend::Draw.
 *
 * @param item TODO: Describe this parameter.
 */
void RenderBackend::Draw(const DrawItem &item)
{
    glDrawElements(GL_TRIANGLES, item.count, GL_UNSIGNED_INT, nullptr);
}

/**
 * @brief TODO: Describe RenderBackend::ClassifyRenderMode.
 *
 * @param mode TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
RenderQueue RenderBackend::ClassifyRenderMode(MaterialRenderMode mode)
{
    switch (mode)
    {
    case MaterialRenderMode::OpaqueDeferred:
    case MaterialRenderMode::AlphaClip:
    case MaterialRenderMode::AlphaHashDither:
        return RenderQueue::DeferredOpaque;
    case MaterialRenderMode::Unlit:
        return RenderQueue::ForwardOpaque;
    case MaterialRenderMode::TransparentBlend:
        return RenderQueue::Transparent;
    }

    return RenderQueue::None;
}

/**
 * @brief TODO: Describe RenderBackend::DrawsShadowCaster.
 *
 * @param item TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderBackend::DrawsShadowCaster(const DrawItem &item)
{
    return item.material->casts_shadows && (item.flags & MeshInstanceFlagCastsShadow) != 0;
}

/**
 * @brief TODO: Describe RenderBackend::Render.
 */
void RenderBackend::Render()
{
    glEnable(GL_FRAMEBUFFER_SRGB);
    SyncEnvironmentResources();
    BuildRenderQueues();
    shadow_system_.Render(draw_items_, render_queues_);
    RenderGeometryPass();
    RenderDeferredLightingPass();
    RenderAmbientOcclusionPass();
    RenderSkybox();
    RenderForwardQueue(render_queues_.forward, false);
    RenderForwardQueue(render_queues_.transparent, true);
    PresentSceneColor();
    ui_renderer_.Render(rendering_->GetUiFrame(), window_width_, window_height_);

    glEnable(GL_DEPTH_TEST);
}

/**
 * @brief TODO: Describe RenderBackend::RenderScreenSpaceQuad.
 */
void RenderBackend::RenderScreenSpaceQuad()
{
    if (quad_VAO_ == 0)
    {
        float quad_vertices[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f,
            -1.0f, 1.0f,  0.0f, 1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 0.0f,
        };
        glGenVertexArrays(1, &quad_VAO_);
        glGenBuffers(1, &quad_VBO_);
        glBindVertexArray(quad_VAO_);
        glBindBuffer(GL_ARRAY_BUFFER, quad_VBO_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    glBindVertexArray(quad_VAO_);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

} // namespace CEngine::Renderer::OpenGL
