#include "render_system.h"

#include "opengl/opengl_render_backend.h"

#if defined(CENGINE_ENABLE_VULKAN)
#include "vulkan/vulkan_render_backend.h"
#endif

#include <iostream>

std::unique_ptr<IRenderBackend> RenderSystem::backend;

std::vector<glm::vec3> RenderSystem::light_pos_list;
std::vector<glm::vec3> RenderSystem::light_col_list;
std::vector<float> RenderSystem::light_pow_list;

void RenderSystem::Initialize(int window_width, int window_height)
{
	(void)Initialize(RenderBackendType::OpenGL, nullptr, window_width, window_height);
}

bool RenderSystem::Initialize(RenderBackendType backend_type, GLFWwindow* window, int window_width, int window_height)
{
	Shutdown();

	switch (backend_type)
	{
	case RenderBackendType::OpenGL:
		backend = std::make_unique<OpenGLRenderBackend>();
		break;
	case RenderBackendType::Vulkan:
#if defined(CENGINE_ENABLE_VULKAN)
		backend = std::make_unique<VulkanRenderBackend>();
#else
		std::cout << "Vulkan backend was requested, but this build was compiled without Vulkan support.\n";
		return false;
#endif
		break;
	}

	if (backend == nullptr || !backend->Initialize(window, window_width, window_height))
	{
		backend.reset();
		return false;
	}

	return true;
}

void RenderSystem::Shutdown()
{
	if (backend != nullptr)
	{
		backend->Shutdown();
		backend.reset();
	}

	light_pos_list.clear();
	light_col_list.clear();
	light_pow_list.clear();
}

void RenderSystem::Render()
{
	if (backend != nullptr)
	{
		backend->Render();
	}
}

void RenderSystem::RegisterMesh(const Mesh* mesh)
{
	if (backend != nullptr)
	{
		backend->RegisterMesh(mesh);
	}
}

void RenderSystem::RegisterMaterial(Material* material)
{
	if (backend != nullptr)
	{
		backend->RegisterMaterial(material);
	}
}

unsigned int RenderSystem::RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
{
	size_t id = light_pos_list.size();
	light_pos_list.push_back(light_pos);
	light_col_list.push_back(light_col);
	light_pow_list.push_back(light_pow);

	return static_cast<unsigned int>(id);
}

void RenderSystem::UpdateLight(unsigned int id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
{
	if (id < light_pos_list.size())
	{
		light_pos_list[id] = light_pos;
		light_col_list[id] = light_col;
		light_pow_list[id] = light_pow;
	}
}

const std::vector<glm::vec3>& RenderSystem::GetLightPositions()
{
	return light_pos_list;
}

const std::vector<glm::vec3>& RenderSystem::GetLightColors()
{
	return light_col_list;
}

const std::vector<float>& RenderSystem::GetLightPowers()
{
	return light_pow_list;
}
