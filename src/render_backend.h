#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

class Material;
class Mesh;
struct Renderable;
struct GLFWwindow;

enum class RenderBackendType
{
	OpenGL,
	Vulkan
};

class IRenderBackend
{
public:
	virtual ~IRenderBackend() = default;

	virtual bool Initialize(GLFWwindow* window, int window_width, int window_height) = 0;
	virtual void Shutdown() = 0;
	virtual void Render() = 0;
	virtual void RegisterRenderable(const Renderable& renderable) = 0;
	virtual void RegisterMaterial(Material* material) = 0;
};

#endif
