//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/ui_renderer.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/opengl/ui_renderer.h"

#include "log.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{
namespace
{

constexpr const char *VertexSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;
uniform vec2 ui_viewport;
uniform vec2 ui_translation;
uniform mat4 ui_transform;
out vec4 vertex_color;
out vec2 texture_uv;

vec3 srgb_to_linear(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(color, vec3(0.04045)));
}

vec4 premultiplied_srgb_to_linear(vec4 color)
{
    if (color.a <= 0.0)
        return vec4(0.0);
    vec3 straight = clamp(color.rgb / color.a, vec3(0.0), vec3(1.0));
    return vec4(srgb_to_linear(straight) * color.a, color.a);
}

void main()
{
    vec4 transformed = ui_transform * vec4(in_position + ui_translation, 0.0, 1.0);
    vec2 pixel = transformed.xy / max(abs(transformed.w), 0.00001);
    vec2 clip = vec2(pixel.x * 2.0 / ui_viewport.x - 1.0,
                     1.0 - pixel.y * 2.0 / ui_viewport.y);
    gl_Position = vec4(clip, 0.0, 1.0);
    vertex_color = premultiplied_srgb_to_linear(in_color);
    texture_uv = in_uv;
}
)glsl";

constexpr const char *FragmentSource = R"glsl(
#version 330 core
in vec4 vertex_color;
in vec2 texture_uv;
uniform sampler2D ui_texture;
uniform bool ui_use_texture;
out vec4 output_color;
void main()
{
    output_color = vertex_color;
    if (ui_use_texture)
        output_color *= texture(ui_texture, texture_uv);
}
)glsl";

bool Compile(GLuint shader, const char *source)
{
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE)
        return true;

    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<std::size_t>(std::max(length, 1)));
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    Logging::Logger::Get().Error("renderer", "UI shader compilation failed: " + std::string(log.data()));
    return false;
}

GLuint BuildProgram()
{
    const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    if (!Compile(vertex, VertexSource) || !Compile(fragment, FragmentSource))
    {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint success = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == GL_TRUE)
        return program;

    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<std::size_t>(std::max(length, 1)));
    glGetProgramInfoLog(program, length, nullptr, log.data());
    Logging::Logger::Get().Error("renderer", "UI shader link failed: " + std::string(log.data()));
    glDeleteProgram(program);
    return 0;
}

float SrgbToLinear(float value)
{
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

std::vector<GLfloat> LinearPremultipliedPixels(const UiTexture &texture)
{
    constexpr float ByteToFloat = 1.0f / 255.0f;
    std::vector<GLfloat> result(texture.rgba.size());
    for (std::size_t index = 0; index < texture.rgba.size(); index += 4u)
    {
        const float alpha = static_cast<float>(texture.rgba[index + 3]) * ByteToFloat;
        result[index + 3] = alpha;
        if (alpha <= 0.0f)
        {
            result[index] = 0.0f;
            result[index + 1] = 0.0f;
            result[index + 2] = 0.0f;
            continue;
        }
        for (std::size_t channel = 0; channel < 3u; ++channel)
        {
            const float encoded_premultiplied = static_cast<float>(texture.rgba[index + channel]) * ByteToFloat;
            const float encoded_straight = std::clamp(encoded_premultiplied / alpha, 0.0f, 1.0f);
            result[index + channel] = SrgbToLinear(encoded_straight) * alpha;
        }
    }
    return result;
}

} // namespace

UiRenderer::~UiRenderer()
{
    Shutdown();
}

bool UiRenderer::Initialize()
{
    Shutdown();
    program_ = BuildProgram();
    if (program_ == 0)
        return false;

    glGenVertexArrays(1, &vertex_array_);
    glGenBuffers(1, &vertex_buffer_);
    glGenBuffers(1, &index_buffer_);
    glGenTextures(1, &white_texture_);
    glBindTexture(GL_TEXTURE_2D, white_texture_);
    constexpr std::array<unsigned char, 4> White = {255, 255, 255, 255};
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, White.data());
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                          reinterpret_cast<void *>(offsetof(UiVertex, position_x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(UiVertex),
                          reinterpret_cast<void *>(offsetof(UiVertex, red)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(UiVertex),
                          reinterpret_cast<void *>(offsetof(UiVertex, texture_u)));
    glBindVertexArray(0);

    viewport_location_ = glGetUniformLocation(program_, "ui_viewport");
    translation_location_ = glGetUniformLocation(program_, "ui_translation");
    transform_location_ = glGetUniformLocation(program_, "ui_transform");
    use_texture_location_ = glGetUniformLocation(program_, "ui_use_texture");
    glUseProgram(program_);
    glUniform1i(glGetUniformLocation(program_, "ui_texture"), 0);
    glUseProgram(0);
    return true;
}

void UiRenderer::Shutdown()
{
    for (const auto &[texture, resource] : textures_)
    {
        static_cast<void>(texture);
        glDeleteTextures(1, &resource.id);
    }
    textures_.clear();
    if (index_buffer_ != 0)
        glDeleteBuffers(1, &index_buffer_);
    if (vertex_buffer_ != 0)
        glDeleteBuffers(1, &vertex_buffer_);
    if (vertex_array_ != 0)
        glDeleteVertexArrays(1, &vertex_array_);
    if (program_ != 0)
        glDeleteProgram(program_);
    if (white_texture_ != 0)
        glDeleteTextures(1, &white_texture_);
    program_ = 0;
    vertex_array_ = 0;
    vertex_buffer_ = 0;
    index_buffer_ = 0;
    white_texture_ = 0;
}

GLuint UiRenderer::ResolveTexture(const std::shared_ptr<const UiTexture> &texture)
{
    if (texture == nullptr || !texture->Valid())
        return 0;
    if (const auto found = textures_.find(texture.get()); found != textures_.end())
        return found->second.id;

    TextureResource resource;
    resource.owner = texture;
    glGenTextures(1, &resource.id);
    glBindTexture(GL_TEXTURE_2D, resource.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLint unpack_alignment = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const std::vector<GLfloat> linear_pixels = LinearPremultipliedPixels(*texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, texture->width, texture->height, 0, GL_RGBA, GL_FLOAT,
                 linear_pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack_alignment);
    textures_.emplace(texture.get(), resource);
    return resource.id;
}

void UiRenderer::PruneTextures()
{
    for (auto it = textures_.begin(); it != textures_.end();)
    {
        if (it->second.owner.expired())
        {
            glDeleteTextures(1, &it->second.id);
            it = textures_.erase(it);
        }
        else
            ++it;
    }
}

void UiRenderer::Render(const UiFrame &frame, int drawable_width, int drawable_height)
{
    PruneTextures();
    if (program_ == 0 || frame.Empty() || frame.width <= 0 || frame.height <= 0 || drawable_width <= 0 ||
        drawable_height <= 0)
        return;

    std::array<GLboolean, 5> enabled = {glIsEnabled(GL_BLEND), glIsEnabled(GL_CULL_FACE), glIsEnabled(GL_DEPTH_TEST),
                                        glIsEnabled(GL_SCISSOR_TEST), glIsEnabled(GL_FRAMEBUFFER_SRGB)};
    GLint previous_program = 0;
    GLint previous_vertex_array = 0;
    GLint previous_active_texture = 0;
    GLint previous_texture = 0;
    GLint previous_blend_source = 0;
    GLint previous_blend_destination = 0;
    std::array<GLint, 4> previous_viewport{};
    std::array<GLint, 4> previous_scissor{};
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vertex_array);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previous_blend_source);
    glGetIntegerv(GL_BLEND_DST_RGB, &previous_blend_destination);
    glGetIntegerv(GL_VIEWPORT, previous_viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, previous_scissor.data());

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glUniform2f(viewport_location_, static_cast<float>(frame.width), static_cast<float>(frame.height));
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(frame.vertices.size() * sizeof(UiVertex)),
                 frame.vertices.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(frame.indices.size() * sizeof(std::uint32_t)),
                 frame.indices.data(), GL_STREAM_DRAW);

    const float scale_x = static_cast<float>(drawable_width) / static_cast<float>(frame.width);
    const float scale_y = static_cast<float>(drawable_height) / static_cast<float>(frame.height);
    glViewport(0, 0, drawable_width, drawable_height);
    for (const UiBatch &batch : frame.batches)
    {
        if (batch.index_count == 0 ||
            static_cast<std::size_t>(batch.first_index) + batch.index_count > frame.indices.size())
            continue;

        if (batch.scissor_enabled)
        {
            const int left = std::clamp(static_cast<int>(std::floor(batch.scissor.x * scale_x)), 0, drawable_width);
            const int right = std::clamp(static_cast<int>(std::ceil((batch.scissor.x + batch.scissor.width) * scale_x)),
                                         0, drawable_width);
            const int bottom = std::clamp(
                static_cast<int>(std::floor((frame.height - batch.scissor.y - batch.scissor.height) * scale_y)), 0,
                drawable_height);
            const int top =
                std::clamp(static_cast<int>(std::ceil((frame.height - batch.scissor.y) * scale_y)), 0, drawable_height);
            glEnable(GL_SCISSOR_TEST);
            glScissor(left, bottom, std::max(right - left, 0), std::max(top - bottom, 0));
        }
        else
            glDisable(GL_SCISSOR_TEST);

        const GLuint resolved_texture = ResolveTexture(batch.texture);
        const bool use_texture = resolved_texture != 0;
        const GLuint texture = use_texture ? resolved_texture : white_texture_;
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(use_texture_location_, use_texture ? 1 : 0);
        glUniform2fv(translation_location_, 1, &batch.translation.x);
        glUniformMatrix4fv(transform_location_, 1, GL_FALSE, glm::value_ptr(batch.transform));
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.index_count), GL_UNSIGNED_INT,
                       reinterpret_cast<void *>(static_cast<std::size_t>(batch.first_index) * sizeof(std::uint32_t)));
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    glActiveTexture(static_cast<GLenum>(previous_active_texture));
    glBindVertexArray(static_cast<GLuint>(previous_vertex_array));
    glUseProgram(static_cast<GLuint>(previous_program));
    glBlendFunc(static_cast<GLenum>(previous_blend_source), static_cast<GLenum>(previous_blend_destination));
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    glScissor(previous_scissor[0], previous_scissor[1], previous_scissor[2], previous_scissor[3]);
    enabled[0] ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    enabled[1] ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    enabled[2] ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    enabled[3] ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    enabled[4] ? glEnable(GL_FRAMEBUFFER_SRGB) : glDisable(GL_FRAMEBUFFER_SRGB);
}

} // namespace CEngine::Renderer::OpenGL
