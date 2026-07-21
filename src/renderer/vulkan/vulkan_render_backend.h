#ifndef VULKAN_RENDER_BACKEND_H
#define VULKAN_RENDER_BACKEND_H

#include "renderer/render_backend.h"

#if defined(CENGINE_ENABLE_VULKAN)

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace CEngine::Renderer {

class VulkanRenderBackend final : public IRenderBackend
{
public:
	bool Initialize(GLFWwindow* window, int window_width, int window_height) override;
	void Shutdown() override;
	void Render() override;
	void RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
		uint32_t native_depth_texture, int texture_width, int texture_height) override;
	void RegisterRenderable(const Renderable& renderable) override;
	void RemoveRenderable(RenderableHandle handle) override;
	void UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform,
		const Bounds& world_bounds) override;
	void RegisterMaterial(Material* material) override;
	void RemoveMaterial(Material* material) override;
	bool RegisterLightmap(const Lightmap* lightmap) override;
	void RemoveLightmap(const Lightmap* lightmap) override;

private:
	struct QueueFamilyIndices {
		uint32_t graphics_family = UINT32_MAX;
		uint32_t present_family = UINT32_MAX;

		bool IsComplete() const
		{
			return graphics_family != UINT32_MAX && present_family != UINT32_MAX;
		}
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities {};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	bool CreateInstance();
	bool CreateSurface(GLFWwindow* window);
	bool PickPhysicalDevice();
	bool CreateLogicalDevice();
	bool CreateSwapchain(int window_width, int window_height);
	bool CreateImageViews();
	bool CreateRenderPass();
	bool CreateFramebuffers();
	bool CreateCommandPool();
	bool CreateCommandBuffers();
	bool CreateSyncObjects();

	void DestroySwapchainResources();

	bool IsDeviceSuitable(VkPhysicalDevice candidate) const;
	bool CheckDeviceExtensionSupport(VkPhysicalDevice candidate) const;
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice candidate) const;
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const;
	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& available_present_modes) const;
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int window_width, int window_height) const;

	bool RecordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);

	VkInstance instance = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physical_device = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphics_queue = VK_NULL_HANDLE;
	VkQueue present_queue = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchain_image_format = VK_FORMAT_UNDEFINED;
	VkExtent2D swapchain_extent {};
	VkRenderPass render_pass = VK_NULL_HANDLE;
	VkCommandPool command_pool = VK_NULL_HANDLE;

	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	std::vector<VkFramebuffer> swapchain_framebuffers;
	std::vector<VkCommandBuffer> command_buffers;
	std::vector<VkSemaphore> image_available_semaphores;
	std::vector<VkSemaphore> render_finished_semaphores;
	std::vector<VkFence> in_flight_fences;

	size_t current_frame = 0;
	bool initialized = false;
};


} // namespace CEngine::Renderer
#endif

#endif
