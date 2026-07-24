//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/vulkan/vulkan_render_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef VULKAN_RENDER_BACKEND_H
#define VULKAN_RENDER_BACKEND_H

#include "renderer/render_backend.h"

#ifdef CENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe VulkanRenderBackend.
 */
class VulkanRenderBackend final : public IRenderBackend
{
  public:
    /**
     * @brief TODO: Describe Initialize.
     *
     * @param rendering TODO: Describe this parameter.
     * @param window TODO: Describe this parameter.
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Initialize(RenderSystem &rendering, Window::WindowSystem &window, int window_width,
                    int window_height) override;
    /**
     * @brief TODO: Describe Shutdown.
     */
    void Shutdown() override;
    /**
     * @brief TODO: Describe Resize.
     *
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Resize(int window_width, int window_height) override;
    /**
     * @brief TODO: Describe Render.
     */
    void Render() override;
    /**
     * @brief TODO: Describe RegisterMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param mesh_instance TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                              const Bounds &world_bounds) override;
    /**
     * @brief TODO: Describe RemoveMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     */
    void RemoveMeshInstance(std::uint32_t slot) override;
    /**
     * @brief TODO: Describe UpdateMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param transform TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @param flags TODO: Describe this parameter.
     */
    void UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                            std::uint32_t flags) override;
    bool UpdateSkinningPalette(std::uint32_t, std::span<const glm::mat4>) override
    {
        return false;
    }

  private:
    /**
     * @brief TODO: Describe QueueFamilyIndices.
     */
    struct QueueFamilyIndices
    {
        uint32_t graphics_family = UINT32_MAX;
        uint32_t present_family = UINT32_MAX;

        /**
         * @brief TODO: Describe IsComplete.
         *
         * @return TODO: Describe the return value.
         */
        bool IsComplete() const
        {
            return graphics_family != UINT32_MAX && present_family != UINT32_MAX;
        }
    };

    /**
     * @brief TODO: Describe SwapChainSupportDetails.
     */
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    /**
     * @brief TODO: Describe CreateInstance.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateInstance();
    /**
     * @brief TODO: Describe CreateSurface.
     *
     * @param window TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool CreateSurface(Window::WindowSystem &window);
    /**
     * @brief TODO: Describe PickPhysicalDevice.
     *
     * @return TODO: Describe the return value.
     */
    bool PickPhysicalDevice();
    /**
     * @brief TODO: Describe CreateLogicalDevice.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateLogicalDevice();
    /**
     * @brief TODO: Describe CreateSwapchain.
     *
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool CreateSwapchain(int window_width, int window_height);
    /**
     * @brief TODO: Describe CreateImageViews.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateImageViews();
    /**
     * @brief TODO: Describe CreateRenderPass.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateRenderPass();
    /**
     * @brief TODO: Describe CreateFramebuffers.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateFramebuffers();
    /**
     * @brief TODO: Describe CreateCommandPool.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateCommandPool();
    /**
     * @brief TODO: Describe CreateCommandBuffers.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateCommandBuffers();
    /**
     * @brief TODO: Describe CreateSyncObjects.
     *
     * @return TODO: Describe the return value.
     */
    bool CreateSyncObjects();

    /**
     * @brief TODO: Describe DestroySwapchainResources.
     */
    void DestroySwapchainResources();

    /**
     * @brief TODO: Describe IsDeviceSuitable.
     *
     * @param candidate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool IsDeviceSuitable(VkPhysicalDevice candidate) const;
    /**
     * @brief TODO: Describe CheckDeviceExtensionSupport.
     *
     * @param candidate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool CheckDeviceExtensionSupport(VkPhysicalDevice candidate) const;
    /**
     * @brief TODO: Describe FindQueueFamilies.
     *
     * @param candidate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice candidate) const;
    /**
     * @brief TODO: Describe QuerySwapChainSupport.
     *
     * @param candidate TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice candidate) const;
    /**
     * @brief TODO: Describe ChooseSwapSurfaceFormat.
     *
     * @param available_formats TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &available_formats) const;
    /**
     * @brief TODO: Describe ChooseSwapPresentMode.
     *
     * @param available_present_modes TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &available_present_modes) const;
    /**
     * @brief TODO: Describe ChooseSwapExtent.
     *
     * @param capabilities TODO: Describe this parameter.
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, int window_width,
                                int window_height) const;

    /**
     * @brief TODO: Describe RecordCommandBuffer.
     *
     * @param command_buffer TODO: Describe this parameter.
     * @param image_index TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool RecordCommandBuffer(VkCommandBuffer command_buffer, uint32_t image_index);

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_image_format = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchain_extent{};
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
