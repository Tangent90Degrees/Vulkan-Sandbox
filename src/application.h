//
// Created by Victor Liu on 11/21/26.
//

#ifndef VULKAN_APPLICATION_H
#define VULKAN_APPLICATION_H

#include <SDL3/SDL.h>
#include <shaderc/shaderc.hpp>
#include "vk_mem_alloc.h"


class Application {
    constexpr static uint32_t VULKAN_VERSION       = VK_API_VERSION_1_4;
    constexpr static uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    constexpr static VkFormat SWAPCHAIN_FORMAT     = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr static VkFormat DEPTH_FORMAT         = VK_FORMAT_D32_SFLOAT;

public:
    Application();
    ~Application();

    bool initialize();
    void run();
    void terminate();

private:
    SDL_Window *window_ = nullptr;

    int window_width_;
    int window_height_;

    bool running_ = false;

    uint64_t frame_count_    = 0;
    uint64_t timeline_value_ = MAX_FRAMES_IN_FLIGHT - 1;

    // Vulkan core
    VkInstance       Vulkan_instance_ = nullptr;
    VkPhysicalDevice physical_device_ = nullptr;
    VkDevice         device_          = nullptr;
    VkSurfaceKHR     surface_         = nullptr;
    VmaAllocator     VMA_allocator_   = nullptr;

    // queue related
    uint32_t gfx_queue_fam_idx_ = UINT32_MAX;
    VkQueue  gfx_queue_         = nullptr;

    // swapchain related
    VkSwapchainKHR           swapchain_ = nullptr;
    std::vector<VkImage>     swapchain_imgs_;
    std::vector<VkImageView> swapchain_img_views_;
    std::vector<VkSemaphore> render_complete_semaphores_;

    bool     require_swapchain_recreate_ = false;
    uint32_t swapchain_width_            = 0;
    uint32_t swapchain_height_           = 0;

    VkImage       depth_img_            = nullptr;
    VkImageView   depth_img_view_       = nullptr;
    VmaAllocation depth_img_allocation_ = nullptr;

    bool initialize_Vulkan();
    bool create_Vulkan_instance();
    bool create_surface();

    [[nodiscard]] VkPhysicalDevice find_physical_device() const;

    bool create_device(VkPhysicalDevice physical_device);
    bool initialize_VMA();
    bool create_swapchain(uint32_t window_width, uint32_t window_height);
    void destroy_swapchain();
    bool find_graphics_queue();
    bool create_shaders();

    static void show_error_message(const char *message);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
        VkDebugUtilsMessageTypeFlagsEXT             message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *                                      user_data
    );
};


#endif // VULKAN_APPLICATION_H
