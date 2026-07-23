#ifndef PBR_GEOMETRY_PASS_H
#define PBR_GEOMETRY_PASS_H

#include "renderer/material.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

class RenderSystem;

class PBRGeometryPass
{
  public:
    PBRGeometryPass();

    void Use() const;
    void UpdateFrame(const RenderSystem &rendering) const;
    void UpdateObject(const glm::mat4 &model, const Material &material, const glm::vec2 &lightmap_scale,
                      const glm::vec2 &lightmap_offset, float lightmap_rgbm_range);
    void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap);

  private:
    void InitializeParameters();

    ShaderProgram shader_program_;
    GLuint albedo_tex_ = 0;
    GLuint normal_tex_ = 0;
    GLuint metallic_roughness_ao_tex_ = 0;
    GLuint lightmap_tex_ = 0;

    GLint model_id_ = -1;
    GLint view_id_ = -1;
    GLint projection_id_ = -1;
    GLint albedo_id_ = -1;
    GLint normal_id_ = -1;
    GLint metallic_roughness_ao_id_ = -1;
    GLint base_color_factor_id_ = -1;
    GLint metallic_roughness_ao_factors_id_ = -1;
    GLint alpha_cutoff_id_ = -1;
    GLint render_mode_id_ = -1;
    GLint receives_shadows_id_ = -1;
    GLint lightmap_id_ = -1;
    GLint lightmap_scale_offset_id_ = -1;
    GLint lightmap_rgbm_range_id_ = -1;
    GLint has_lightmap_id_ = -1;
};

} // namespace CEngine::Renderer
#endif
