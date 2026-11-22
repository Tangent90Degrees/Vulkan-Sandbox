//
// Created by Victor Liu on 11/21/26.
//

#include <iostream>
#include <SDL3/SDL_vulkan.h>


#define VOLK_IMPLEMENTATION
#include "volk.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#define VMA_IMPLEMENTATION
#include "application.h"


Application::Application()
    : window_width_(1280), window_height_(720) {
}

bool Application::initialize() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        window_ = SDL_CreateWindow(
            "Vulkan Learning",
            window_width_, window_height_,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (!window_) {
            show_error_message("Error creating window");
            return false;
        }

        if (!initialize_Vulkan()) {
            show_error_message("Unable to initialize Vulkan");
            return false;
        }
    } else {
        show_error_message("Unable to initialize SDL3");
        return false;
    }

    return true;
}

void Application::terminate() {
    // cleanup swapchain
    destroy_swapchain();

    // VMA
    if (VMA_allocator_) {
        vmaDestroyAllocator(VMA_allocator_);
    }

    // cleanup Vulkan
    if (surface_) vkDestroySurfaceKHR(Vulkan_instance_, surface_, nullptr);
    if (device_) vkDestroyDevice(device_, nullptr);

    // cleanup instance
    if (Vulkan_instance_) vkDestroyInstance(Vulkan_instance_, nullptr);
    volkFinalize();

    // cleanup SDL
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Application::initialize_Vulkan() {
    if (!create_Vulkan_instance()) {
        show_error_message("Unable to create a Vulkan instance");
        return false;
    }

    if (!create_surface()) {
        show_error_message("Unable to create a window surface");
        return false;
    }

    if (physical_device_ = find_physical_device(); !physical_device_) {
        show_error_message("Unable to find an appropriate physical device");
        return false;
    }

    if (!find_graphics_queue()) {
        show_error_message("Unable to find a compatible graphic queue");
        return false;
    }

    if (!create_device(physical_device_)) {
        show_error_message("Unable to create the logical GPU device");
        return false;
    }

    if (!initialize_VMA()) {
        show_error_message("Unable to create a Vulkan Memory Allocator");
        return false;
    }

    if (!create_swapchain(window_width_, window_height_)) {
        show_error_message("Unable to create a swapchain");
        return false;
    }

    if (!create_shaders()) {
        show_error_message("Error creating shader modules");
        return false;
    }
}

bool Application::create_Vulkan_instance() {
    // Initialize Volk and load Vk function pointers
    if (volkInitialize() != VK_SUCCESS) {
        show_error_message("Error initializing Volk");
        return false;
    }

    // Create the Vulkan application instance
    VkApplicationInfo app_info {
        .sType            = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "My First Triangle",
        .apiVersion       = VULKAN_VERSION
    };

    uint32_t           ext_count  = 0;
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    std::vector requested_extensions { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    for (size_t i = 0; i < ext_count; ++i) {
        requested_extensions.push_back(extensions[i]);
    }

    std::vector requested_layers { "VK_LAYER_KHRONOS_validation" };

    VkDebugUtilsMessengerCreateInfoEXT debug_info {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext           = nullptr,
        .flags           = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData       = nullptr
    };

    const VkInstanceCreateInfo instance_create_info {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = &debug_info,
        .flags                   = 0,
        .pApplicationInfo        = &app_info,
        .enabledLayerCount       = static_cast<uint32_t>(requested_layers.size()),
        .ppEnabledLayerNames     = requested_layers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requested_extensions.size()),
        .ppEnabledExtensionNames = requested_extensions.data()
    };

    if (vkCreateInstance(&instance_create_info, nullptr, &Vulkan_instance_) != VK_SUCCESS) {
        return false;
    }

    volkLoadInstance(Vulkan_instance_);
    return true;
}


bool Application::create_surface() {
    if (!SDL_Vulkan_CreateSurface(window_, Vulkan_instance_, nullptr, &surface_)) {
        return false;
    }
    return true;
}


VkPhysicalDevice Application::find_physical_device() const {
    uint32_t physical_device_count = 0;
    vkEnumeratePhysicalDevices(Vulkan_instance_, &physical_device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices { physical_device_count };
    vkEnumeratePhysicalDevices(
        Vulkan_instance_, &physical_device_count,
        physical_devices.data()
    );

    VkPhysicalDevice physical_device = nullptr;
    if (physical_device_count) {
        physical_device = physical_devices[0];

        for (auto &p : physical_devices) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(p, &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device = p;
                break;
            }
        }
    }

    // ensure the desired swapchain format is supported
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface_, &format_count, nullptr
    );
    std::vector<VkSurfaceFormatKHR> surface_formats { format_count };
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface_, &format_count, surface_formats.data()
    );

    const bool format_supported = std::ranges::any_of(
        surface_formats,
        [](const auto &s) { return s.format == SWAPCHAIN_FORMAT; }
    );

    if (!format_supported) {
        show_error_message("Requested swapchain format is not supported by the surface");
        return nullptr;
    }

    return physical_device;
}


bool Application::create_device(VkPhysicalDevice physical_device) {
    // query supported features
    VkPhysicalDeviceVulkan14Features supported_features_1_4 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };
    VkPhysicalDeviceVulkan13Features supported_features_1_3 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &supported_features_1_4
    };
    VkPhysicalDeviceVulkan12Features supported_features_1_2 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &supported_features_1_3
    };
    VkPhysicalDeviceFeatures2 supported_features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &supported_features_1_2
    };
    vkGetPhysicalDeviceFeatures2(physical_device, &supported_features);

    // check if what we need is supported
    if (
        !supported_features_1_3.dynamicRendering ||
        !supported_features_1_3.synchronization2 ||
        !supported_features_1_2.timelineSemaphore
    ) {
        show_error_message("Physical device does not meet the feature requirements");
        return false;
    }

    // product a separate features struct chain for device creation
    VkPhysicalDeviceVulkan14Features features_1_4 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = nullptr
    };
    VkPhysicalDeviceVulkan13Features features_1_3 {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext            = &features_1_4,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    VkPhysicalDeviceVulkan12Features features_1_2 {
        .sType             = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext             = &features_1_3,
        .timelineSemaphore = VK_TRUE
    };
    VkPhysicalDeviceFeatures2 features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_1_2
    };

    // request the queues we will be using
    std::vector             queue_priorities { 1.0f };
    VkDeviceQueueCreateInfo gfx_queue_info {
        .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = gfx_queue_fam_idx_,
        .queueCount       = 1,
        .pQueuePriorities = queue_priorities.data()
    };

    // device specific extensions
    const std::vector device_extensions { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo device_create_info {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext                   = &features,
        .queueCreateInfoCount    = 1,
        .pQueueCreateInfos       = &gfx_queue_info,
        .enabledExtensionCount   = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
        .pEnabledFeatures        = nullptr // features struct chain is set pNext
    };

    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }

    // grab the VkQueue object finally
    vkGetDeviceQueue(device_, gfx_queue_fam_idx_, 0, &gfx_queue_);
    if (!gfx_queue_) {
        show_error_message("Could not get the graphics queue");
        return false;
    }
    return true;
}


bool Application::initialize_VMA() {
    VmaVulkanFunctions     VMA_func_info;
    VmaAllocatorCreateInfo VMA_alloc_info {
        .flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice   = physical_device_,
        .device           = device_,
        .pVulkanFunctions = &VMA_func_info,
        .instance         = Vulkan_instance_,
        .vulkanApiVersion = VULKAN_VERSION
    };

    // VMA can import directly from Volk
    vmaImportVulkanFunctionsFromVolk(&VMA_alloc_info, &VMA_func_info);

    if (vmaCreateAllocator(&VMA_alloc_info, &VMA_allocator_) != VK_SUCCESS) {
        return false;
    }
    return true;
}

bool Application::create_swapchain(const uint32_t window_width, const uint32_t window_height) {
    // track swapchain size separate from window size
    swapchain_width_  = window_width;
    swapchain_height_ = window_height;

    // ensure we request an appropriate number of images
    VkSurfaceCapabilitiesKHR surface_caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &surface_caps)
        != VK_SUCCESS) {
        show_error_message("Could not get surface capabilities");
        return false;
    }

    uint32_t requested_img_count = std::max(2U, surface_caps.minImageCount);
    if (surface_caps.maxImageCount > 0) {
        requested_img_count = std::min(requested_img_count, surface_caps.maxImageCount);
    }

    const VkSwapchainCreateInfoKHR swapchain_create_info {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = surface_,
        .minImageCount    = requested_img_count,
        .imageFormat      = SWAPCHAIN_FORMAT,
        .imageColorSpace  = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent      = { .width = swapchain_width_, .height = swapchain_height_ },
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform     = surface_caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = VK_PRESENT_MODE_FIFO_KHR
    };

    if (vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr, &swapchain_)
        != VK_SUCCESS) {
        show_error_message("Could not create swapchain");
        return false;
    }

    // ask for the swapchain images
    uint32_t img_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &img_count, nullptr);
    swapchain_imgs_.resize(img_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &img_count, swapchain_imgs_.data());

    // create the swapchain image views
    swapchain_img_views_.resize(img_count);
    for (size_t i = 0; i < img_count; ++i) {
        VkImageViewCreateInfo img_view_info {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image    = swapchain_imgs_[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = SWAPCHAIN_FORMAT,
            .subresourceRange {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };

        if (vkCreateImageView(device_, &img_view_info, nullptr, &swapchain_img_views_[i])
            != VK_SUCCESS) {
            show_error_message("Could not create swapchain image view");
            return false;
        }
    }

    // semaphores used to signal render completion
    render_complete_semaphores_.resize(img_count);
    for (auto &semaphore : render_complete_semaphores_) {
        VkSemaphoreCreateInfo semaphore_info {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
            show_error_message("Could not create the render-complete semaphore");
            return false;
        }
    }

    // create depth image
    const VkImageCreateInfo depth_create_info {
        .sType         = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = DEPTH_FORMAT,
        .extent        = { .width = swapchain_width_, .height = swapchain_height_ },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    constexpr VmaAllocationCreateInfo alloc_info {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    if (vmaCreateImage(VMA_allocator_, &depth_create_info, &alloc_info, &depth_img_,
                       &depth_img_allocation_, nullptr) != VK_SUCCESS) {
        show_error_message("Error allocating depth image");
        return false;
    }

    return true;
}


void Application::destroy_swapchain() {
    for (const auto swapchain_img_view : swapchain_img_views_) {
        vkDestroyImageView(device_, swapchain_img_view, nullptr);
    }
    swapchain_img_views_.clear();

    // destroy render complete semaphores
    for (const auto &semaphore : render_complete_semaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    render_complete_semaphores_.clear();

    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = nullptr;
    }

    // destroy the depth buffer along with the swapchain
    if (depth_img_view_) {
        vkDestroyImageView(device_, depth_img_view_, nullptr);
        vmaDestroyImage(VMA_allocator_, depth_img_, depth_img_allocation_);
        depth_img_view_ = nullptr;
    }
}


bool Application::find_graphics_queue() {
    // eventually we will have more complex queue lookup for presentation. etc.
    uint32_t queue_fam_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(
        physical_device_, &queue_fam_count,
        nullptr
    );
    std::vector<VkQueueFamilyProperties2> queue_fam_props {
        queue_fam_count,
        { .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 }
    };
    vkGetPhysicalDeviceQueueFamilyProperties2(
        physical_device_, &queue_fam_count,
        queue_fam_props.data()
    );

    for (size_t i = 0; i < queue_fam_props.size(); ++i) {
        // ensure it has presentation support
        VkBool32 has_present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device_, i, surface_, &has_present_support
        );

        // ensure this is a GRAPHICS queue with presentation support
        if (
            const auto &props = queue_fam_props[i];
            props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && has_present_support
        ) {
            gfx_queue_fam_idx_ = i;
            return true;
        }
    }
    return false;
}


bool Application::create_shaders() {
}


void Application::show_error_message(const char *message) {
    std::cerr << message << std::endl;
}

VkBool32 Application::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
    VkDebugUtilsMessageTypeFlagsEXT             message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *                                      user_data
) {
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation Layer: " << callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}
