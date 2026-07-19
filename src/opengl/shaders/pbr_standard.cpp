#include "pbr_standard.h"

#include "render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr GLuint kDirectLightBindingPoint = 0;
}

PBRStandard::PBRStandard()
	: albedo_tex(0),
	  normal_tex(0),
	  metallic_roughness_ao_tex(0)
{
    shader_program.Load("shaders/opengl/pbr_standard.vert",
                        "shaders/opengl/pbr_standard.frag");
    InitializeParameters();
}

PBRStandard::~PBRStandard()
{
    if (light_ubo != 0)
    {
        glDeleteBuffers(1, &light_ubo);
        light_ubo = 0;
    }
}

void PBRStandard::Use() const
{
    shader_program.Use();
}

void PBRStandard::Update()
{
    SetParametersStatic();
    SetParametersDynamic();
}

void PBRStandard::InitializeParameters()
{
    const GLuint shader_id = shader_program.GetId();
    const GLuint light_block_index = glGetUniformBlockIndex(shader_id, "DirectLightBlock");
    if (light_block_index != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(shader_id, light_block_index, kDirectLightBindingPoint);
    }

    glGenBuffers(1, &light_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(glm::vec4) + RenderSystem::GetMaxGpuLights() * sizeof(GpuLight)),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, kDirectLightBindingPoint, light_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    cam_pos_id = glGetUniformLocation(shader_id, "cam_pos_world");

    m_id = glGetUniformLocation(shader_id, "model");
    v_id = glGetUniformLocation(shader_id, "view");
    p_id = glGetUniformLocation(shader_id, "projection");

    albedo_id = glGetUniformLocation(shader_id, "albedo");
    normal_id = glGetUniformLocation(shader_id, "normal");
    metallic_roughness_ao_id = glGetUniformLocation(shader_id, "metallic_roughness_ao");
}

void PBRStandard::SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao)
{
    albedo_tex = albedo;
    normal_tex = normal;
    metallic_roughness_ao_tex = metallic_roughness_ao;
}

void PBRStandard::SetParametersStatic()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo_tex);
    glUniform1i(albedo_id, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_tex);
    glUniform1i(normal_id, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex);
    glUniform1i(metallic_roughness_ao_id, 2);
}

void PBRStandard::SetParametersDynamic()
{
    const RenderFrameConstants& constants = RenderSystem::GetFrameConstants();
    glUniform3f(cam_pos_id, constants.camera_position[0],
                constants.camera_position[1], constants.camera_position[2]);
    glUniformMatrix4fv(m_id, 1, GL_FALSE, &constants.model[0][0]);
    glUniformMatrix4fv(v_id, 1, GL_FALSE, &constants.view[0][0]);
    glUniformMatrix4fv(p_id, 1, GL_FALSE, &constants.proj[0][0]);

    const uint64_t light_revision = RenderSystem::GetLightRevision();
    if (uploaded_light_revision != light_revision)
    {
        const std::vector<GpuLight>& gpu_lights = RenderSystem::GetGpuLights();
        const glm::vec4 light_info(static_cast<float>(gpu_lights.size()), 0.0f, 0.0f, 0.0f);

        glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(light_info));
        if (!gpu_lights.empty())
        {
            glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4),
                            static_cast<GLsizeiptr>(gpu_lights.size() * sizeof(GpuLight)), gpu_lights.data());
        }
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        uploaded_light_revision = light_revision;
    }
}
