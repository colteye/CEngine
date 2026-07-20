#include "vulkan_render_backend.h"

#if defined(CENGINE_ENABLE_VULKAN)

#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <set>
#include <string>

namespace {
constexpr int kMaxFramesInFlight = 2;

const std::vector<const char*> kDeviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

bool VkSucceeded(VkResult result, const char* message)
{
	if (result != VK_SUCCESS)
	{
		std::cout << message << " VkResult=" << result << "\n";
		return false;
	}
	return true;
}
} // namespace

bool VulkanRenderBackend::Initialize(GLFWwindow* window, int window_width, int window_height)
{
	if (window == nullptr)
	{
		std::cout << "Vulkan backend requires a GLFW window.\n";
		return false;
	}

	if (!CreateInstance() ||
		!CreateSurface(window) ||
		!PickPhysicalDevice() ||
		!CreateLogicalDevice() ||
		!CreateSwapchain(window_width, window_height) ||
		!CreateImageViews() ||
		!CreateRenderPass() ||
		!CreateFramebuffers() ||
		!CreateCommandPool() ||
		!CreateCommandBuffers() ||
		!CreateSyncObjects())
	{
		Shutdown();
		return false;
	}

	initialized = true;
	return true;
}

void VulkanRenderBackend::Shutdown()
{
	if (device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(device);
	}

	for (VkFence fence : in_flight_fences)
	{
		vkDestroyFence(device, fence, nullptr);
	}
	in_flight_fences.clear();

	for (VkSemaphore semaphore : render_finished_semaphores)
	{
		vkDestroySemaphore(device, semaphore, nullptr);
	}
	render_finished_semaphores.clear();

	for (VkSemaphore semaphore : image_available_semaphores)
	{
		vkDestroySemaphore(device, semaphore, nullptr);
	}
	image_available_semaphores.clear();

	if (command_pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}
	command_buffers.clear();

	DestroySwapchainResources();

	if (device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}

	physical_device = VK_NULL_HANDLE;
	graphics_queue = VK_NULL_HANDLE;
	present_queue = VK_NULL_HANDLE;
	current_frame = 0;
	initialized = false;
}

void VulkanRenderBackend::Render()
{
	if (!initialized)
	{
		return;
	}

	vkWaitForFences(device, 1, &in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

	uint32_t image_index = 0;
	VkResult acquire_result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
		image_available_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
	if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return;
	}
	if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR)
	{
		std::cout << "Failed to acquire Vulkan swapchain image. VkResult=" << acquire_result << "\n";
		return;
	}

	vkResetFences(device, 1, &in_flight_fences[current_frame]);

	VkCommandBuffer command_buffer = command_buffers[image_index];
	vkResetCommandBuffer(command_buffer, 0);
	if (!RecordCommandBuffer(command_buffer, image_index))
	{
		return;
	}

	VkSemaphore wait_semaphores[] = { image_available_semaphores[current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signal_semaphores[] = { render_finished_semaphores[current_frame] };

	VkSubmitInfo submit_info {};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	if (!VkSucceeded(vkQueueSubmit(graphics_queue, 1, &submit_info, in_flight_fences[current_frame]),
		"Failed to submit Vulkan draw command buffer."))
	{
		return;
	}

	VkPresentInfoKHR present_info {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = { swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;
	present_info.pImageIndices = &image_index;

	VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
	if (present_result != VK_SUCCESS && present_result != VK_SUBOPTIMAL_KHR && present_result != VK_ERROR_OUT_OF_DATE_KHR)
	{
		std::cout << "Failed to present Vulkan swapchain image. VkResult=" << present_result << "\n";
	}

	current_frame = (current_frame + 1) % kMaxFramesInFlight;
}

void VulkanRenderBackend::RegisterRenderable(const Renderable& /*renderable*/)
{
	static bool warned = false;
	if (!warned)
	{
		std::cout << "Vulkan backend is active; mesh upload/rendering is not implemented yet.\n";
		warned = true;
	}
}

void VulkanRenderBackend::UpdateRenderableTransform(RenderableHandle /*handle*/, const glm::mat4& /*transform*/,
	const Bounds& /*world_bounds*/)
{
}

void VulkanRenderBackend::RenderDepthOnly(const glm::mat4& /*view*/, const glm::mat4& /*projection*/,
	uint32_t /*native_depth_texture*/, int /*texture_width*/, int /*texture_height*/)
{
	static bool warned = false;
	if (!warned)
	{
		std::cout << "Vulkan backend is active; depth-only rendering is not implemented yet.\n";
		warned = true;
	}
}

void VulkanRenderBackend::RegisterMaterial(Material* /*material*/)
{
	static bool warned = false;
	if (!warned)
	{
		std::cout << "Vulkan backend is active; material texture upload is not implemented yet.\n";
		warned = true;
	}
}

bool VulkanRenderBackend::CreateInstance()
{
	if (glfwVulkanSupported() != GLFW_TRUE)
	{
		std::cout << "GLFW reports that Vulkan is not supported on this system.\n";
		return false;
	}

	VkApplicationInfo app_info {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "CEngine";
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "CEngine";
	app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;

	uint32_t glfw_extension_count = 0;
	const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
	if (glfw_extensions == nullptr || glfw_extension_count == 0)
	{
		std::cout << "GLFW did not provide Vulkan instance extensions.\n";
		return false;
	}

	VkInstanceCreateInfo create_info {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = glfw_extension_count;
	create_info.ppEnabledExtensionNames = glfw_extensions;
	create_info.enabledLayerCount = 0;

	return VkSucceeded(vkCreateInstance(&create_info, nullptr, &instance), "Failed to create Vulkan instance.");
}

bool VulkanRenderBackend::CreateSurface(GLFWwindow* window)
{
	return VkSucceeded(glfwCreateWindowSurface(instance, window, nullptr, &surface),
		"Failed to create Vulkan window surface.");
}

bool VulkanRenderBackend::PickPhysicalDevice()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if (device_count == 0)
	{
		std::cout << "No Vulkan-capable physical devices found.\n";
		return false;
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	for (VkPhysicalDevice candidate : devices)
	{
		if (IsDeviceSuitable(candidate))
		{
			physical_device = candidate;
			return true;
		}
	}

	std::cout << "No suitable Vulkan physical device found.\n";
	return false;
}

bool VulkanRenderBackend::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies(physical_device);

	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<uint32_t> unique_queue_families = { indices.graphics_family, indices.present_family };
	float queue_priority = 1.0f;

	for (uint32_t queue_family : unique_queue_families)
	{
		VkDeviceQueueCreateInfo queue_create_info {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	VkPhysicalDeviceFeatures device_features {};

	VkDeviceCreateInfo create_info {};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.pEnabledFeatures = &device_features;
	create_info.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
	create_info.ppEnabledExtensionNames = kDeviceExtensions.data();
	create_info.enabledLayerCount = 0;

	if (!VkSucceeded(vkCreateDevice(physical_device, &create_info, nullptr, &device),
		"Failed to create Vulkan logical device."))
	{
		return false;
	}

	vkGetDeviceQueue(device, indices.graphics_family, 0, &graphics_queue);
	vkGetDeviceQueue(device, indices.present_family, 0, &present_queue);
	return true;
}

bool VulkanRenderBackend::CreateSwapchain(int window_width, int window_height)
{
	SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(physical_device);
	VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swapchain_support.formats);
	VkPresentModeKHR present_mode = ChooseSwapPresentMode(swapchain_support.present_modes);
	VkExtent2D extent = ChooseSwapExtent(swapchain_support.capabilities, window_width, window_height);

	uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
	if (swapchain_support.capabilities.maxImageCount > 0 &&
		image_count > swapchain_support.capabilities.maxImageCount)
	{
		image_count = swapchain_support.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR create_info {};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices indices = FindQueueFamilies(physical_device);
	uint32_t queue_family_indices[] = { indices.graphics_family, indices.present_family };

	if (indices.graphics_family != indices.present_family)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	create_info.preTransform = swapchain_support.capabilities.currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (!VkSucceeded(vkCreateSwapchainKHR(device, &create_info, nullptr, &swapchain),
		"Failed to create Vulkan swapchain."))
	{
		return false;
	}

	vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
	swapchain_images.resize(image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

	swapchain_image_format = surface_format.format;
	swapchain_extent = extent;
	return true;
}

bool VulkanRenderBackend::CreateImageViews()
{
	swapchain_image_views.resize(swapchain_images.size());

	for (size_t index = 0; index < swapchain_images.size(); ++index)
	{
		VkImageViewCreateInfo create_info {};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = swapchain_images[index];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = swapchain_image_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (!VkSucceeded(vkCreateImageView(device, &create_info, nullptr, &swapchain_image_views[index]),
			"Failed to create Vulkan image view."))
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderBackend::CreateRenderPass()
{
	VkAttachmentDescription color_attachment {};
	color_attachment.format = swapchain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref {};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_info {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	render_pass_info.dependencyCount = 1;
	render_pass_info.pDependencies = &dependency;

	return VkSucceeded(vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass),
		"Failed to create Vulkan render pass.");
}

bool VulkanRenderBackend::CreateFramebuffers()
{
	swapchain_framebuffers.resize(swapchain_image_views.size());

	for (size_t index = 0; index < swapchain_image_views.size(); ++index)
	{
		VkImageView attachments[] = { swapchain_image_views[index] };

		VkFramebufferCreateInfo framebuffer_info {};
		framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebuffer_info.renderPass = render_pass;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = attachments;
		framebuffer_info.width = swapchain_extent.width;
		framebuffer_info.height = swapchain_extent.height;
		framebuffer_info.layers = 1;

		if (!VkSucceeded(vkCreateFramebuffer(device, &framebuffer_info, nullptr, &swapchain_framebuffers[index]),
			"Failed to create Vulkan framebuffer."))
		{
			return false;
		}
	}

	return true;
}

bool VulkanRenderBackend::CreateCommandPool()
{
	QueueFamilyIndices queue_family_indices = FindQueueFamilies(physical_device);

	VkCommandPoolCreateInfo pool_info {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue_family_indices.graphics_family;

	return VkSucceeded(vkCreateCommandPool(device, &pool_info, nullptr, &command_pool),
		"Failed to create Vulkan command pool.");
}

bool VulkanRenderBackend::CreateCommandBuffers()
{
	command_buffers.resize(swapchain_framebuffers.size());

	VkCommandBufferAllocateInfo alloc_info {};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());

	return VkSucceeded(vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()),
		"Failed to allocate Vulkan command buffers.");
}

bool VulkanRenderBackend::CreateSyncObjects()
{
	image_available_semaphores.resize(kMaxFramesInFlight);
	render_finished_semaphores.resize(kMaxFramesInFlight);
	in_flight_fences.resize(kMaxFramesInFlight);

	VkSemaphoreCreateInfo semaphore_info {};
	semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fence_info {};
	fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (int index = 0; index < kMaxFramesInFlight; ++index)
	{
		if (!VkSucceeded(vkCreateSemaphore(device, &semaphore_info, nullptr, &image_available_semaphores[index]),
				"Failed to create Vulkan image-available semaphore.") ||
			!VkSucceeded(vkCreateSemaphore(device, &semaphore_info, nullptr, &render_finished_semaphores[index]),
				"Failed to create Vulkan render-finished semaphore.") ||
			!VkSucceeded(vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[index]),
				"Failed to create Vulkan frame fence."))
		{
			return false;
		}
	}

	return true;
}

void VulkanRenderBackend::DestroySwapchainResources()
{
	for (VkFramebuffer framebuffer : swapchain_framebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	swapchain_framebuffers.clear();

	if (render_pass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}

	for (VkImageView image_view : swapchain_image_views)
	{
		vkDestroyImageView(device, image_view, nullptr);
	}
	swapchain_image_views.clear();
	swapchain_images.clear();

	if (swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

bool VulkanRenderBackend::IsDeviceSuitable(VkPhysicalDevice candidate) const
{
	QueueFamilyIndices indices = FindQueueFamilies(candidate);
	bool extensions_supported = CheckDeviceExtensionSupport(candidate);

	bool swapchain_adequate = false;
	if (extensions_supported)
	{
		SwapChainSupportDetails swapchain_support = QuerySwapChainSupport(candidate);
		swapchain_adequate = !swapchain_support.formats.empty() && !swapchain_support.present_modes.empty();
	}

	return indices.IsComplete() && extensions_supported && swapchain_adequate;
}

bool VulkanRenderBackend::CheckDeviceExtensionSupport(VkPhysicalDevice candidate) const
{
	uint32_t extension_count = 0;
	vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr);

	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(kDeviceExtensions.begin(), kDeviceExtensions.end());
	for (const VkExtensionProperties& extension : available_extensions)
	{
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

VulkanRenderBackend::QueueFamilyIndices VulkanRenderBackend::FindQueueFamilies(VkPhysicalDevice candidate) const
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, nullptr);

	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queue_family_count, queue_families.data());

	for (uint32_t index = 0; index < queue_family_count; ++index)
	{
		if ((queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			indices.graphics_family = index;
		}

		VkBool32 present_support = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &present_support);
		if (present_support == VK_TRUE)
		{
			indices.present_family = index;
		}

		if (indices.IsComplete())
		{
			break;
		}
	}

	return indices;
}

VulkanRenderBackend::SwapChainSupportDetails VulkanRenderBackend::QuerySwapChainSupport(VkPhysicalDevice candidate) const
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(candidate, surface, &details.capabilities);

	uint32_t format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, nullptr);
	if (format_count != 0)
	{
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &format_count, details.formats.data());
	}

	uint32_t present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &present_mode_count, nullptr);
	if (present_mode_count != 0)
	{
		details.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface, &present_mode_count, details.present_modes.data());
	}

	return details;
}

VkSurfaceFormatKHR VulkanRenderBackend::ChooseSwapSurfaceFormat(
	const std::vector<VkSurfaceFormatKHR>& available_formats) const
{
	for (const VkSurfaceFormatKHR& available_format : available_formats)
	{
		if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
			available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return available_format;
		}
	}

	return available_formats[0];
}

VkPresentModeKHR VulkanRenderBackend::ChooseSwapPresentMode(
	const std::vector<VkPresentModeKHR>& available_present_modes) const
{
	for (const VkPresentModeKHR& available_present_mode : available_present_modes)
	{
		if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return available_present_mode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderBackend::ChooseSwapExtent(
	const VkSurfaceCapabilitiesKHR& capabilities, int window_width, int window_height) const
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}

	VkExtent2D actual_extent = {
		static_cast<uint32_t>(window_width),
		static_cast<uint32_t>(window_height)
	};

	actual_extent.width = std::clamp(actual_extent.width,
		capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actual_extent.height = std::clamp(actual_extent.height,
		capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actual_extent;
}

bool VulkanRenderBackend::RecordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index)
{
	VkCommandBufferBeginInfo begin_info {};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (!VkSucceeded(vkBeginCommandBuffer(command_buffer, &begin_info),
		"Failed to begin recording Vulkan command buffer."))
	{
		return false;
	}

	VkClearValue clear_color = {{{0.03f, 0.035f, 0.04f, 1.0f}}};

	VkRenderPassBeginInfo render_pass_info {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_info.renderPass = render_pass;
	render_pass_info.framebuffer = swapchain_framebuffers[image_index];
	render_pass_info.renderArea.offset = {0, 0};
	render_pass_info.renderArea.extent = swapchain_extent;
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(command_buffer);

	return VkSucceeded(vkEndCommandBuffer(command_buffer),
		"Failed to record Vulkan command buffer.");
}

#endif
