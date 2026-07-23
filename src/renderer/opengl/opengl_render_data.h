#ifndef OPENGL_RENDER_DATA_H
#define OPENGL_RENDER_DATA_H

#include "renderer/material.h"
#include "renderer/renderable.h"

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace CEngine::Renderer {

struct OpenGLDrawItem {
	const MeshData* mesh_data = nullptr;
	Material* material = nullptr;
	glm::mat4 transform = glm::mat4(1.0f);
	Bounds world_bounds;
	GLint start_index = 0;
	GLsizei count = 0;
	uint32_t flags = 0;
	GLuint vertex_array_obj = 0;
	GLuint albedo_tex = 0;
	GLuint normal_tex = 0;
	GLuint metallic_roughness_ao_tex = 0;
	GLuint lightmap_tex = 0;
	glm::vec2 lightmap_scale = glm::vec2(1.0f);
	glm::vec2 lightmap_offset = glm::vec2(0.0f);
	float lightmap_rgbm_range = 8.0f;
};

enum class OpenGLRenderQueue
{
	DeferredOpaque,
	ForwardOpaque,
	Transparent,
	None
};

struct OpenGLRenderQueues {
	std::vector<uint32_t> opaque_deferred;
	std::vector<uint32_t> forward;
	std::vector<uint32_t> transparent;
	std::vector<uint32_t> shadow_casters;

	void ClearAndReserve(size_t draw_item_count);
};


} // namespace CEngine::Renderer
#endif
