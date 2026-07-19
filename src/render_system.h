#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "light.h"
#include "model.h"
#include "render_backend.h"

#include <memory>
#include <vector>

struct GLFWwindow;

class RenderSystem
{
public:
	static void Initialize(int window_width, int window_height);
	static bool Initialize(RenderBackendType backend_type, GLFWwindow* window, int window_width, int window_height);
	static void Shutdown();
	
	static void Render();

	static void RegisterMesh(const Mesh* mesh);
	static void RegisterMaterial(Material* material);

	static unsigned int RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static void UpdateLight(unsigned int id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);

	static const std::vector<glm::vec3>& GetLightPositions();
	static const std::vector<glm::vec3>& GetLightColors();
	static const std::vector<float>& GetLightPowers();

private:
	static std::unique_ptr<IRenderBackend> backend;

	// These properties must be separated for OpenGL.
	static std::vector<glm::vec3> light_pos_list;
	static std::vector<glm::vec3> light_col_list;
	static std::vector<float> light_pow_list;
};
#endif
