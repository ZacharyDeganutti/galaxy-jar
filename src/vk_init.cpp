#include "vk_init.hpp"
#include "vk_descriptors.hpp"
#include "vk_buffer.hpp"
#include "glmvk.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <unordered_set>
#include <vulkan/vk_enum_string_helper.h>

// Local definitions and forward declarations
namespace vk_init {
    #ifndef NDEBUG
    const std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };
    #endif

    struct QueueFamilyCollection {
        std::vector<uint32_t> indices;
        std::vector<VkQueueFamilyProperties> properties;
    };

    typedef struct GpuAndQueueInfo {
        VkPhysicalDevice gpu;
        QueueFamilyCollection graphics;
        QueueFamilyCollection presentation;
    } GpuInfo;

    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

    SwapchainSupportDetails query_swapchain_support(const VkPhysicalDevice device, const VkSurfaceKHR surface);
    bool are_device_extensions_supported(const VkPhysicalDevice device, const std::vector<const char*>& required_extensions);
    bool are_vulkan_1_3_features_supported(const VkPhysicalDevice device);
    QueueFamilyCollection find_queue_families(const VkPhysicalDevice gpu);
    QueueFamilyCollection filter(const QueueFamilyCollection& families, const std::function<bool(size_t)> criteria);
    QueueFamilyCollection filter_for_feature_compatability(const QueueFamilyCollection& families, const VkQueueFlagBits queue_feature_flags);
    QueueFamilyCollection filter_for_presentation_compatibility(const VkPhysicalDevice gpu, const VkSurfaceKHR surface, const QueueFamilyCollection& families);
    VkInstance init_instance(const int extension_count, const char* const* extension_names, vk_types::CleanupProcedures& cleanup_procedures);
    GpuAndQueueInfo init_physical_device(const VkInstance instance, const std::vector<const char*>& required_extensions, const VkSurfaceKHR surface);
    VkDevice init_logical_device(const GpuAndQueueInfo& gpu_info, const std::vector<const char*>& required_extensions, vk_types::CleanupProcedures& cleanup_procedures);
    VkSurfaceKHR init_surface(const VkInstance instance, GLFWwindow* window, vk_types::CleanupProcedures& cleanup_procedures);
    VkSurfaceFormatKHR choose_swapchain_surface_format(const std::vector<VkSurfaceFormatKHR>& format_list);
    VkPresentModeKHR choose_swapchain_present_mode(const std::vector<VkPresentModeKHR>& mode_list);
    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, const uint32_t width, const uint32_t height);
    VkImageView init_image_view(const VkDevice device, const VkImage image, const VkFormat format, vk_types::CleanupProcedures& cleanup_procedures);
    vk_types::AllocatedImage init_allocated_image(const VkDevice device, const VmaAllocator allocator, const VkFormat format, const VkImageUsageFlags usage_flags, const VkExtent2D extent, vk_types::CleanupProcedures& cleanup_procedures);
    vk_types::Swapchain init_swapchain(const VkDevice device, const GpuAndQueueInfo& gpu_info, const VkSurfaceKHR surface, const uint32_t width, const uint32_t height, vk_types::CleanupProcedures& cleanup_procedures);
    std::vector<vk_types::Command> init_command(const VkDevice device, const GpuAndQueueInfo& gpu, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures);
    std::vector<vk_types::Synchronization> init_synchronization(const VkDevice device, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures);
    VmaAllocator init_allocator(const VkInstance instance, const VkDevice device, const GpuAndQueueInfo& gpu, vk_types::CleanupProcedures& cleanup_procedures);
    VkShaderModule init_shader_module(const VkDevice device, const char *file_path, vk_types::CleanupProcedures& cleanup_procedures);
    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, vk_types::CleanupProcedures& cleanup_procedures);
    vk_types::Pipeline init_compute_pipeline(const VkDevice device, const VkPipelineLayout compute_pipeline_layout, const VkShaderModule shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures);
    vk_types::Pipeline init_graphics_pipeline(const VkDevice device, const VkPipelineLayout pipeline_layout, const VkFormat& target_format, const VkFormat& depth_format, const VkShaderModule vert_shader_module, const VkShaderModule frag_shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures);
}

namespace vk_init {
    VkShaderModule init_shader_module(const VkDevice device, const char *file_path, vk_types::CleanupProcedures& cleanup_procedures) {
        // open the file. With cursor at the end
        std::ifstream file(file_path, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            printf("Unable to open shader file: %s\n", file_path);
            exit(EXIT_FAILURE);
        }

        // find what the size of the file is by looking up the location of the cursor
        // because the cursor is at the end, it gives the size directly in bytes
        size_t file_size = static_cast<size_t>(file.tellg());

        // spirv expects the buffer to be uint32, so make sure to reserve a int
        // vector big enough for the entire file
        std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

        // put file cursor at beginning
        file.seekg(0);

        // load the entire file into the buffer
        file.read(reinterpret_cast<char *>(buffer.data()), file_size);

        // now that the file is loaded into the buffer, we can close it
        file.close();

        // create a new shader module, using the buffer we loaded
        VkShaderModuleCreateInfo shader_info = {};
        shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_info.pNext = nullptr;

        // codeSize has to be in bytes, so multiply the ints in the buffer by size of
        // int to know the real size of the buffer
        shader_info.codeSize = buffer.size() * sizeof(uint32_t);
        shader_info.pCode = buffer.data();

        VkShaderModule shader_module = {};
        if (vkCreateShaderModule(device, &shader_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            printf("Unable to generate shader module from file: %s\n", file_path);
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, shader_module]() {
            vkDestroyShaderModule(device, shader_module, nullptr);
        });
        return shader_module;
    }

    SwapchainSupportDetails query_swapchain_support(const VkPhysicalDevice device, const VkSurfaceKHR surface) {
        SwapchainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
        if (format_count != 0) {
            details.formats.resize(format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, details.formats.data());
        }

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
        if (present_mode_count != 0) {
            details.present_modes.resize(present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, details.present_modes.data());
        }
        return details;
    }

    bool are_device_extensions_supported(const VkPhysicalDevice device, const std::vector<const char*>& required_extensions) {
        uint32_t extension_count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());
        
        std::unordered_set<std::string> extensions_left_to_satisfy(required_extensions.begin(), required_extensions.end());

        for (const auto& extension : available_extensions) {
            extensions_left_to_satisfy.erase(std::string(extension.extensionName));
        }

        return extensions_left_to_satisfy.empty();
    }

    bool are_vulkan_1_3_features_supported(const VkPhysicalDevice device) {
        VkPhysicalDeviceVulkan13Features features13prefill = {};
        features13prefill.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features13prefill;
        vkGetPhysicalDeviceFeatures2(device, &features2);
        VkBaseOutStructure* next = reinterpret_cast<VkBaseOutStructure*>(features2.pNext);
        while (next != nullptr) {
            if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                VkPhysicalDeviceVulkan13Features* features13 = reinterpret_cast<VkPhysicalDeviceVulkan13Features*>(next);
                if (features13->synchronization2 && features13->dynamicRendering) {
                    return true;
                }
                else {
                    return false;
                }
            }
            else {
                next = reinterpret_cast<VkBaseOutStructure*>(next->pNext);
            }
        }
        return false;
    }

    QueueFamilyCollection find_queue_families(const VkPhysicalDevice gpu) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_families.data());

        std::vector<uint32_t> indices;
        for (uint32_t queue_family_index = 0; queue_family_index < queue_families.size(); ++queue_family_index) {
            indices.push_back(queue_family_index);
        }
        QueueFamilyCollection family_info = {
            indices,
            queue_families
        };
        return family_info;
    }

    QueueFamilyCollection filter(const QueueFamilyCollection& families, const std::function<bool(size_t)> criteria) {
        std::vector<uint32_t> filtered_indices;
        std::vector<VkQueueFamilyProperties> filtered_properties;
        for (size_t index = 0; index < families.indices.size(); ++index) {
            if (criteria(index)) {
                filtered_indices.push_back(families.indices[index]);
                filtered_properties.push_back(families.properties[index]);
            }
        }
        return QueueFamilyCollection {
            filtered_indices,
            filtered_properties
        };
    }

    QueueFamilyCollection filter_for_feature_compatability(const QueueFamilyCollection& families, const VkQueueFlagBits queue_feature_flags) {
        return filter(families, [&](size_t index) {
            VkQueueFamilyProperties queue_family = families.properties[index];
            return (queue_family.queueFlags & queue_feature_flags) > 0;
        });
    }

    QueueFamilyCollection filter_for_presentation_compatibility(const VkPhysicalDevice gpu, const VkSurfaceKHR surface, const QueueFamilyCollection& families) {
        return filter(families, [&](size_t index) {
            VkBool32 present_support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, index, surface, &present_support);
            return present_support;
        });
    }

    VkInstance init_instance(const int extension_count, const char* const* extension_names, vk_types::CleanupProcedures& cleanup_procedures) {
        // Create our instance
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Galaxy Jar";
        app_info.applicationVersion = VK_API_VERSION_1_3;
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_API_VERSION_1_3;
        app_info.apiVersion= VK_API_VERSION_1_3;

        #ifndef NDEBUG
        // Configure validation layers if compiled in debug mode
        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
        
        std::unordered_map<std::string, uint32_t> layer_map;
        for (auto layer : available_layers) {
            layer_map.emplace(std::string(layer.layerName), 1);
        }

        // Bail if the requested validation layers are not available
        for (auto enabled_layer : validation_layers) {
            if (layer_map.find(std::string(enabled_layer)) == layer_map.end()) {
                printf("Validation layer %s requested but not available!\n", enabled_layer);
                exit(EXIT_FAILURE);
            }
        }
        printf("\nValidation layers ON\n");
        #endif

        VkInstanceCreateInfo instance_create_info{};
        instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instance_create_info.pApplicationInfo = &app_info;
        instance_create_info.enabledExtensionCount = extension_count;
        instance_create_info.ppEnabledExtensionNames = extension_names;
        #ifndef NDEBUG
        instance_create_info.ppEnabledLayerNames = validation_layers.data();
        instance_create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        #else
        instance_create_info.ppEnabledLayerNames = nullptr;
        instance_create_info.enabledLayerCount = 0;
        #endif

        VkInstance instance{};
        VkResult instance_creation_result = vkCreateInstance(&instance_create_info, nullptr, &instance);
        if (instance_creation_result != VK_SUCCESS) {
            printf("VkInstance creation failed: %s\n", string_VkResult(instance_creation_result));
            exit(EXIT_FAILURE);
        }
        cleanup_procedures.add([instance]() {
            vkDestroyInstance(instance, nullptr);
        });
        return instance;
    }

    GpuAndQueueInfo init_physical_device(const VkInstance instance, const std::vector<const char*>& required_extensions, const VkSurfaceKHR surface) {
        uint32_t device_count = 0;
        if (instance == nullptr) {
            printf("No valid VkInstance.\n");
            exit(EXIT_FAILURE);
        }
        vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
        if (device_count == 0) {
            printf("No Vulkan capable devices found.\n");
            exit(EXIT_FAILURE);
        }

        std::vector<VkPhysicalDevice> devices(device_count);
        vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

        // dumb heuristic for auto selecting the 'best' gpu based on heap size and device type
        std::vector<uint64_t> rankings(device_count, 0);
        for (uint32_t device_index = 0; device_index < device_count; ++device_index) {
            VkPhysicalDevice device = devices[device_index];
            VkPhysicalDeviceProperties props = VkPhysicalDeviceProperties{};
            vkGetPhysicalDeviceProperties(device, &props);

            // prioritize GPUs by skewing the ranking, for discrete ones over integrated ones in particular
            if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                rankings[device_index] |= (1ULL << 63); 
            }
            else if (props.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
                rankings[device_index] |= (1ULL << 62);
            }

            VkPhysicalDeviceMemoryProperties memory_props = VkPhysicalDeviceMemoryProperties{};
            vkGetPhysicalDeviceMemoryProperties(device, &memory_props);

            VkMemoryHeap* heaps_pointer = memory_props.memoryHeaps;
            std::vector<VkMemoryHeap> heaps = std::vector<VkMemoryHeap>(heaps_pointer, heaps_pointer + memory_props.memoryHeapCount);

            VkDeviceSize size = 0;
            for (const auto& heap : heaps) {
                if (heap.flags & VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    size += heap.size;
                }
            }

            rankings[device_index] += size;

            // Check if the given device supports the extensions that we need
            bool supports_extensions = are_device_extensions_supported(device, required_extensions);
            bool supports_features = are_vulkan_1_3_features_supported(device);
            // We also need to know if a graphics capable queue family exists
            QueueFamilyCollection queue_families = find_queue_families(device);
            QueueFamilyCollection supporting_graphics = filter_for_feature_compatability(queue_families, VK_QUEUE_GRAPHICS_BIT);
            QueueFamilyCollection supporting_presentation = filter_for_presentation_compatibility(device, surface, queue_families);

            // If the device can't do what the program needs, disqualify it completely
            if ((!supports_extensions) || 
                (!supports_features) ||
                (supporting_graphics.indices.size() == 0) || 
                (supporting_presentation.indices.size() == 0)) 
            {
                rankings[device_index] = 0;
            }
            // Some additional checks against specific extensions. Only bother if extension support is already deemed adequate
            if (supports_extensions && (std::find(required_extensions.begin(), required_extensions.end(), VK_KHR_SWAPCHAIN_EXTENSION_NAME) != required_extensions.end())) {
                // Make sure swapchain support is up to snuff if we need it
                SwapchainSupportDetails swapchain_support = query_swapchain_support(device, surface);
                if (swapchain_support.formats.empty() || swapchain_support.present_modes.empty()) {
                    rankings[device_index] = 0;
                }
            }
        }
        // The biggest heap size of the highest priority device type that hasn't been disqualified wins
        auto high_score_location = std::max_element(rankings.begin(), rankings.end());
        if((*high_score_location) == 0) {
            printf("Could not find a suitable VkPhysicalDevice.\n");
            exit(EXIT_FAILURE);
        }
        size_t winner = std::distance(rankings.begin(), high_score_location); 

        VkPhysicalDevice best_device = devices[winner];
        // Be lazy and redo a little work
        QueueFamilyCollection queue_families = find_queue_families(best_device);
        QueueFamilyCollection supporting_graphics = filter_for_feature_compatability(queue_families, VK_QUEUE_GRAPHICS_BIT);
        QueueFamilyCollection supporting_presentation = filter_for_presentation_compatibility(best_device, surface, queue_families);

        return GpuAndQueueInfo {
            best_device,
            supporting_graphics,
            supporting_presentation
        };
    }

    VkDevice init_logical_device(const GpuAndQueueInfo& gpu_info, const std::vector<const char*>& required_extensions, vk_types::CleanupProcedures& cleanup_procedures) {
        if (gpu_info.gpu == nullptr) {
            printf("No valid VkPhysicalDevice.\n");
            exit(EXIT_FAILURE);
        }
        auto graphics_queue_family_indices = gpu_info.graphics.indices;
        if (graphics_queue_family_indices.size() == 0) {
            printf("Could not find device with suitable queue family\n");
            exit(EXIT_FAILURE);
        }
        auto presentation_queue_family_indices = gpu_info.presentation.indices;
        if (presentation_queue_family_indices.size() == 0) {
            printf("Could not find device with suitable queue family\n");
            exit(EXIT_FAILURE);
        }

        std::unordered_set<uint32_t> queue_indices = {
            graphics_queue_family_indices[0], 
            presentation_queue_family_indices[0],
        };
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {};
        float queue_priority = 1.0f; 
        for (auto queue_index : queue_indices) {
            VkDeviceQueueCreateInfo device_queue_create_info{};
            device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_queue_create_info.queueFamilyIndex = graphics_queue_family_indices[0];
            device_queue_create_info.queueCount = 1;
            device_queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(device_queue_create_info);
        }

        // Do the janky Vulkan 1.2 + 1.3 features enabling song and dance
        VkPhysicalDeviceFeatures device_features{};
        VkPhysicalDeviceVulkan13Features features13 = {};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        features13.pNext = nullptr;

        VkPhysicalDeviceVulkan12Features features12 = {};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.pNext = &features13;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12;
        features2.features = device_features;

        // Sum it all up in the device-wide create info
        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext = &features2;
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.queueCreateInfoCount = queue_create_infos.size();
        device_create_info.pEnabledFeatures = nullptr;
        device_create_info.enabledExtensionCount = required_extensions.size();
        device_create_info.ppEnabledExtensionNames = required_extensions.data();
        #ifndef NDEBUG
        device_create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        device_create_info.ppEnabledLayerNames = validation_layers.data();
        #else
        device_create_info.enabledLayerCount = 0;
        #endif

        VkDevice device{};
        VkResult device_creation_result = vkCreateDevice(gpu_info.gpu, &device_create_info, nullptr, &device);
        if (device_creation_result != VK_SUCCESS) {
            printf("Unable to create logical device\n");
            exit(EXIT_FAILURE);
        }
        cleanup_procedures.add([device]() {
            vkDestroyDevice(device, nullptr);
        });
        return device;
    }

    VkSurfaceKHR init_surface(const VkInstance instance, GLFWwindow* window, vk_types::CleanupProcedures& cleanup_procedures) {
        if (instance == nullptr) {
            printf("No valid VkInstance.\n");
            exit(EXIT_FAILURE);
        }
        if (window == nullptr) {
            printf("No valid window.\n");
            exit(EXIT_FAILURE);
        }

        VkSurfaceKHR surface{};
        VkResult surface_creation_result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if (surface_creation_result != VK_SUCCESS) {
            printf("Unable to create window surface\n");
            exit(EXIT_FAILURE);
        }
        cleanup_procedures.add([instance, surface]() {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        });
        return surface;
    }

    VkSurfaceFormatKHR choose_swapchain_surface_format(const std::vector<VkSurfaceFormatKHR>& format_list) {
        // Prefer SRGB
        for (const auto& format : format_list) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
        // Barring availability of that, just grab whatever's in front
        return format_list[0];
    }

    VkPresentModeKHR choose_swapchain_present_mode(const std::vector<VkPresentModeKHR>& mode_list) {
        // Prefer mailbox
        for (const auto& mode : mode_list) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }
        // But FIFO is guaranteed to be available when that isn't
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, const uint32_t width, const uint32_t height) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actual_extent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actual_extent;
        }
    }

    VkImageView init_image_view(const VkDevice device, const VkImage image, const VkFormat format, vk_types::CleanupProcedures& cleanup_procedures) {
        VkImageView image_view = {};

        VkImageViewCreateInfo image_view_create_info{};
        image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_create_info.image = image;
        image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_create_info.format = format;
        image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_create_info.subresourceRange.aspectMask = (format >= VK_FORMAT_D16_UNORM) && (format <= VK_FORMAT_D32_SFLOAT_S8_UINT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_create_info.subresourceRange.baseMipLevel = 0;
        image_view_create_info.subresourceRange.levelCount = 1;
        image_view_create_info.subresourceRange.baseArrayLayer = 0;
        image_view_create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &image_view_create_info, nullptr, &image_view) != VK_SUCCESS) {
            printf("Unable to create image view");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, image_view]() {
            vkDestroyImageView(device, image_view, nullptr);
        });

        return image_view;
    }
    
    vk_types::AllocatedImage init_allocated_image(const VkDevice device, const VmaAllocator allocator, const VkFormat format, const VkImageUsageFlags usage_flags, const VkExtent2D extent, vk_types::CleanupProcedures& cleanup_procedures) {
        // Setup image specification
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.pNext = nullptr;

        image_info.imageType = VK_IMAGE_TYPE_2D;

        image_info.format = format;
        image_info.extent = {extent.width, extent.height, 1};

        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;

        image_info.samples = VK_SAMPLE_COUNT_1_BIT;

        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = usage_flags;

        // Setup allocation specification
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImage image = {};
        VmaAllocation allocation = {};
        vmaCreateImage(allocator, &image_info, &alloc_info, &image, &allocation, nullptr);

        // Specify cleanup for the VkImage here because the image is created prior to its VkImageView and we want cleanup in the right order
        // since init_image_view handles its own cleanup
        cleanup_procedures.add([allocator, image, allocation]() {
            vmaDestroyImage(allocator, image, allocation);
        });

        VkImageView view = init_image_view(device, image, format, cleanup_procedures);
        vk_types::AllocatedImage allocated_image = {};
        allocated_image.allocation = allocation;
        allocated_image.image = image;
        allocated_image.image_extent = extent;
        allocated_image.image_format = format;
        allocated_image.image_view = view;

        return allocated_image;
    }

    vk_types::Swapchain init_swapchain(const VkDevice device, const GpuAndQueueInfo& gpu_info, const VkSurfaceKHR surface, const uint32_t width, const uint32_t height, vk_types::CleanupProcedures& cleanup_procedures) {
        SwapchainSupportDetails swapchain_support = query_swapchain_support(gpu_info.gpu, surface);

        VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(swapchain_support.formats);
        VkPresentModeKHR present_mode = choose_swapchain_present_mode(swapchain_support.present_modes);
        VkExtent2D extent = choose_swapchain_extent(swapchain_support.capabilities, width, height);

        // Choosing the bare minimum may induce driver overhead. Add 1
        uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

        // Double check that adding this one does not exceed the max image count. 0 is a special value indicating no limit
        if ((swapchain_support.capabilities.maxImageCount > 0) && (image_count > swapchain_support.capabilities.maxImageCount)) {
            image_count = swapchain_support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR swapchain_create_info{};
        swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface = surface;
        swapchain_create_info.minImageCount = image_count;
        swapchain_create_info.imageFormat = surface_format.format;
        swapchain_create_info.imageColorSpace = surface_format.colorSpace;
        swapchain_create_info.imageExtent = extent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // Specify behavior when the graphics queue family is not also the presentation queue family
        uint32_t separate_queue_indices[] = {gpu_info.graphics.indices[0], gpu_info.presentation.indices[0]};
        if (gpu_info.graphics.indices[0] != gpu_info.presentation.indices[0]) {
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            swapchain_create_info.queueFamilyIndexCount = 2;
            swapchain_create_info.pQueueFamilyIndices = separate_queue_indices;
        } else {
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // Don't care
            swapchain_create_info.queueFamilyIndexCount = 0;
            swapchain_create_info.pQueueFamilyIndices = nullptr;
        }

        swapchain_create_info.preTransform = swapchain_support.capabilities.currentTransform;
        swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_create_info.presentMode = present_mode;
        swapchain_create_info.clipped = VK_TRUE;
        swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

        // Construct a handle to a swapchain built from our create info
        VkSwapchainKHR swapchain_handle;
        if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain_handle) != VK_SUCCESS) {
            printf("Unable to create swapchain!\n");
            exit(EXIT_FAILURE);
        }

        // Get the swapchain image references
        std::vector<VkImage> swapchain_images;
        uint32_t queried_image_count = 0;
        vkGetSwapchainImagesKHR(device, swapchain_handle, &queried_image_count, nullptr);
        swapchain_images.resize(queried_image_count);
        vkGetSwapchainImagesKHR(device, swapchain_handle, &queried_image_count, swapchain_images.data());
        
        // Construct image views for the images
        // std::vector<VkImageView> swapchain_views = init_image_views(device, swapchain_images, surface_format.format);
        std::vector<VkImageView> swapchain_views(swapchain_images.size());
        for (auto swapchain_image : swapchain_images) {
            VkImageView view = init_image_view(device, swapchain_image, surface_format.format, cleanup_procedures);
            swapchain_views.push_back(view);
        }

        // Tie all the swapchain bits and bobs together for later use
        vk_types::Swapchain swapchain{};
        swapchain.handle = swapchain_handle;
        swapchain.format = surface_format.format;
        swapchain.extent = extent;
        swapchain.images = swapchain_images;
        swapchain.views  = swapchain_views;

        cleanup_procedures.add([device, swapchain]() {
            vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        });

        return swapchain;
    }

    std::vector<vk_types::Command> init_command(const VkDevice device, const GpuAndQueueInfo& gpu, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures) {
        std::vector<vk_types::Command> per_frame_command_data(buffer_count, vk_types::Command{});
        VkCommandPoolCreateInfo command_pool_info = {};
        command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_info.pNext = nullptr;
        command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_info.queueFamilyIndex = gpu.graphics.indices[0];

        for (size_t i = 0; i < buffer_count; ++i) {
            VkResult command_pool_result = vkCreateCommandPool(device, &command_pool_info, nullptr, &(per_frame_command_data[i].pool));
            if (command_pool_result != VK_SUCCESS) {
                printf("Unable to create command pool for frame %zu\n", i);
                exit(EXIT_FAILURE);
            }
            VkCommandBufferAllocateInfo command_buffer_alloc_info = {};
            command_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            command_buffer_alloc_info.pNext = nullptr;
            command_buffer_alloc_info.commandPool = per_frame_command_data[i].pool;
            command_buffer_alloc_info.commandBufferCount = 1;
            command_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            VkResult command_buffer_result = vkAllocateCommandBuffers(device, &command_buffer_alloc_info, &(per_frame_command_data[i].buffer_primary));
            if (command_buffer_result != VK_SUCCESS) {
                printf("Unable to create command buffer(s) for frame %zu\n", i);
                exit(EXIT_FAILURE);
            }
        }

        cleanup_procedures.add([device, per_frame_command_data]() {
            for (auto command : per_frame_command_data) {
                vkDestroyCommandPool(device, command.pool, nullptr);
            }
        });
        return per_frame_command_data;
    }

    VkFence init_fence(const VkDevice device, vk_types::CleanupProcedures& cleanup_procedures) {
        VkFenceCreateInfo fence_info = {};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence fence = {};
        VkResult render_fence_result = vkCreateFence(device, &fence_info, nullptr, &fence);
        if (render_fence_result != VK_SUCCESS) {
            printf("Unable to create fence\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, fence](){
            vkDestroyFence(device, fence, nullptr);
        });

        return fence;
    }

    std::vector<vk_types::Synchronization> init_synchronization(const VkDevice device, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures) {
        std::vector<vk_types::Synchronization> per_frame_synchronization_structures(buffer_count, vk_types::Synchronization{});

        VkSemaphoreCreateInfo semaphore_info = {};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = 0;

        for (size_t i = 0; i < buffer_count; ++i) {
            per_frame_synchronization_structures[i].render_fence = init_fence(device, cleanup_procedures);

            VkResult swapchain_semaphore_result =
                vkCreateSemaphore(device, &semaphore_info, nullptr, &(per_frame_synchronization_structures[i].swapchain_semaphore));
            if (swapchain_semaphore_result != VK_SUCCESS) {
                printf("Unable to create swapchain semaphore for frame %zu\n", i);
                exit(EXIT_FAILURE);
            }

            VkResult render_semaphore_result =
                vkCreateSemaphore(device, &semaphore_info, nullptr, &(per_frame_synchronization_structures[i].render_semaphore));
            if (swapchain_semaphore_result != VK_SUCCESS) {
                printf("Unable to create render semaphore for frame %zu\n", i);
                exit(EXIT_FAILURE);
            }
        }
        cleanup_procedures.add([device, per_frame_synchronization_structures](){
            for (auto synchronization : per_frame_synchronization_structures) {
                vkDestroySemaphore(device, synchronization.render_semaphore, nullptr);
                vkDestroySemaphore(device, synchronization.swapchain_semaphore, nullptr);
            }
        });
        return per_frame_synchronization_structures;
    }

    VmaAllocator init_allocator(const VkInstance instance, const VkDevice device, const GpuAndQueueInfo& gpu, vk_types::CleanupProcedures& cleanup_procedures) {
        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.physicalDevice = gpu.gpu;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        VmaAllocator allocator = {};
        vmaCreateAllocator(&allocatorInfo, &allocator);

        cleanup_procedures.add([allocator]() {
            vmaDestroyAllocator(allocator);
        });

        return allocator;
    }

    VkPipelineLayout init_pipeline_layout(const VkDevice device, const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts, vk_types::CleanupProcedures& cleanup_procedures) {
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pNext = nullptr;
        layout_info.pSetLayouts = descriptor_set_layouts.data();
        layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());

        VkPipelineLayout pipeline_layout = {};
        if(vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
            printf("Unable to create compute pipeline layout\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, pipeline_layout]() {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        });

        return pipeline_layout;
    }

    VkPipelineShaderStageCreateInfo make_shader_stage_info(VkShaderStageFlagBits stage_flags, VkShaderModule module) {
        // Then create the pipeline (this is somewhat specific to pipeline type)
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.pNext = nullptr;
        stage_info.stage = stage_flags;
        stage_info.module = module;
        stage_info.pName = "main";

        return stage_info;
    }

    vk_types::Pipeline init_compute_pipeline(const VkDevice device, const VkPipelineLayout compute_pipeline_layout, const VkShaderModule shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures) {
        VkPipelineShaderStageCreateInfo stage_info = make_shader_stage_info(VK_SHADER_STAGE_COMPUTE_BIT, shader_module);

        VkComputePipelineCreateInfo compute_pipeline_info{};
        compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        compute_pipeline_info.pNext = nullptr;
        compute_pipeline_info.layout = compute_pipeline_layout;
        compute_pipeline_info.stage = stage_info;

        VkPipeline compute_pipeline = {};
        if(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_pipeline_info, nullptr, &compute_pipeline) != VK_SUCCESS) {
            printf("Unable to create compute pipeline\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, compute_pipeline]() {
            vkDestroyPipeline(device, compute_pipeline, nullptr);
        });

        vk_types::Pipeline pipeline = {};
        pipeline.bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        pipeline.handle = compute_pipeline;
        pipeline.layout = compute_pipeline_layout;
        pipeline.descriptors = descriptor_set;

        return pipeline;
    }

    vk_types::Pipeline init_graphics_pipeline(const VkDevice device, const VkPipelineLayout pipeline_layout, const VkFormat& target_format, const VkFormat& depth_format, const VkShaderModule vert_shader_module, const VkShaderModule frag_shader_module, const VkDescriptorSet descriptor_set, vk_types::CleanupProcedures& cleanup_procedures) {
        
        /// Basic viewport setup
        VkPipelineViewportStateCreateInfo viewport_info = {};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.pNext = nullptr;
        viewport_info.viewportCount = 1;
        viewport_info.scissorCount = 1;

        /// Basic blending info, no blending for now
        VkPipelineColorBlendAttachmentState blend_attachment_state = {};
        blend_attachment_state.blendEnable = VK_FALSE;
        blend_attachment_state.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend_info = {};
        blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_info.pNext = nullptr;
        blend_info.logicOpEnable = VK_FALSE;
        blend_info.logicOp = VK_LOGIC_OP_COPY;
        blend_info.attachmentCount = 1;
        blend_info.pAttachments = &blend_attachment_state;

        /// VertexInputStateCreateInfo is to be configured for non-interleaved positions, normals, and texture coordinates.
        const std::array<VkVertexInputBindingDescription, 3> bindings = {{
            {
                0,                          // binding
                sizeof(glm::vec3),          // stride
                VK_VERTEX_INPUT_RATE_VERTEX // inputRate
            },
            {
                1,                          // binding
                sizeof(glm::vec3),          // stride
                VK_VERTEX_INPUT_RATE_VERTEX // inputRate
            },
            {
                2,
                sizeof(glm::vec2),
                VK_VERTEX_INPUT_RATE_VERTEX
            }
        }};

        const std::array<VkVertexInputAttributeDescription, 3> attributes {{
            {
                0,                          // location
                bindings[0].binding,        // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                1,                          // location
                bindings[1].binding,        // binding
                VK_FORMAT_R32G32B32_SFLOAT, // format
                0                           // offset
            },
            {
                2,                          // location
                bindings[2].binding,        // binding
                VK_FORMAT_R32G32_SFLOAT,    // format
                0                           // offset
            }
        }};
        VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {};
        vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_info.pNext = nullptr;
        vertex_input_state_info.pVertexAttributeDescriptions = attributes.data();
        vertex_input_state_info.vertexAttributeDescriptionCount = attributes.size();
        vertex_input_state_info.pVertexBindingDescriptions = bindings.data();
        vertex_input_state_info.vertexBindingDescriptionCount = bindings.size();

        /// Input Assembly configured to draw triangles
        VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
        input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_info.pNext = nullptr;
        input_assembly_info.primitiveRestartEnable = VK_FALSE;
        // Simple but wasteful list of separate triangles
        input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /// Rasterization configuration
        VkPipelineRasterizationStateCreateInfo rasterization_info = {};
        rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_info.pNext = nullptr;
        // Set up polygons
        rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_info.lineWidth = 1.0f;
        // Culling and winding order
        rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;

        /// Multisampling configuration (AA, etc)
        VkPipelineMultisampleStateCreateInfo multisampling_info = {};
        multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling_info.pNext = nullptr;
        multisampling_info.sampleShadingEnable = VK_FALSE;
        multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling_info.minSampleShading = 1.0f;
        multisampling_info.pSampleMask = nullptr;
        multisampling_info.alphaToCoverageEnable = VK_FALSE;
        multisampling_info.alphaToOneEnable = VK_FALSE;

        /// Set up rendering
        VkPipelineRenderingCreateInfo rendering_info = {};
        rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_info.pNext = nullptr;
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachmentFormats = &target_format;
        rendering_info.depthAttachmentFormat = depth_format;

        /// Configure depth
        VkPipelineDepthStencilStateCreateInfo depth_info = {};
        depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth_info.pNext = nullptr;
        depth_info.depthTestEnable = VK_TRUE;
        depth_info.depthWriteEnable = VK_TRUE;
        depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
        depth_info.depthBoundsTestEnable = VK_FALSE;
        depth_info.stencilTestEnable = VK_FALSE;
        depth_info.front = {};
        depth_info.back = {};
        depth_info.minDepthBounds = 0.0f;
        depth_info.maxDepthBounds = 1.0f;

        /// Dynamic state info
        std::vector<VkDynamicState> state = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_info = {};
        dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_info.pNext = nullptr;
        dynamic_info.pDynamicStates = state.data();
        dynamic_info.dynamicStateCount = static_cast<uint32_t>(state.size());

        std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos = {
            make_shader_stage_info(VK_SHADER_STAGE_VERTEX_BIT, vert_shader_module),
            make_shader_stage_info(VK_SHADER_STAGE_FRAGMENT_BIT, frag_shader_module),
        };

        /// Smoosh it all into the pipeline definition, unused stages like tesselation left as 0 initialized nullptr
        VkGraphicsPipelineCreateInfo pipeline_info = {};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = &rendering_info;
        pipeline_info.stageCount = static_cast<uint32_t>(shader_stage_infos.size());
        pipeline_info.pStages = shader_stage_infos.data();
        pipeline_info.pVertexInputState = &vertex_input_state_info;
        pipeline_info.pInputAssemblyState = &input_assembly_info;
        pipeline_info.pViewportState = &viewport_info;
        pipeline_info.pRasterizationState = &rasterization_info;
        pipeline_info.pMultisampleState = &multisampling_info;
        pipeline_info.pColorBlendState = &blend_info;
        pipeline_info.pDepthStencilState = &depth_info;
        pipeline_info.pDynamicState = &dynamic_info;
        pipeline_info.layout = pipeline_layout;

        VkPipeline graphics_pipeline = {};
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &graphics_pipeline) != VK_SUCCESS) {
            printf("Unable to create graphics pipeline\n");
            exit(EXIT_FAILURE);
        }

        cleanup_procedures.add([device, graphics_pipeline](){
            vkDestroyPipeline(device, graphics_pipeline, nullptr);
        });

        vk_types::Pipeline graphics_pipeline_bundle = {};
        graphics_pipeline_bundle.descriptors = descriptor_set;
        graphics_pipeline_bundle.handle = graphics_pipeline;
        graphics_pipeline_bundle.layout = pipeline_layout;
        graphics_pipeline_bundle.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

        return graphics_pipeline_bundle;
    }

    vk_types::Context init(const std::vector<const char*>& required_device_extensions, const std::vector<const char*>& glfw_extensions, GLFWwindow* window) {
        // Setup tracker for resources that need to be cleaned up
        vk_types::CleanupProcedures cleanup_procedures{};
        VkInstance vulkan_instance = init_instance(glfw_extensions.size(), glfw_extensions.data(), cleanup_procedures);
        VkSurfaceKHR vulkan_surface = init_surface(vulkan_instance, window, cleanup_procedures);
        GpuAndQueueInfo vulkan_gpu = init_physical_device(vulkan_instance, required_device_extensions, vulkan_surface);
        VkDevice vulkan_device = init_logical_device(vulkan_gpu, required_device_extensions, cleanup_procedures);
        
        int w;
        int h;
        glfwGetFramebufferSize(window, &w, &h);
        uint32_t width = static_cast<uint32_t>(w);
        uint32_t height = static_cast<uint32_t>(h);

        vk_types::Swapchain swapchain = init_swapchain(vulkan_device, vulkan_gpu, vulkan_surface, width, height, cleanup_procedures);
        
        // Get the queue handles
        vk_types::Queues queues = {};
        vkGetDeviceQueue(vulkan_device, vulkan_gpu.graphics.indices[0], 0, &(queues.graphics));
        vkGetDeviceQueue(vulkan_device, vulkan_gpu.presentation.indices[0], 0, &(queues.presentation));

        // Double buffering for our command buffers and synchronization structures
        const uint8_t DOUBLE_BUFFER = 2;
        const std::vector<vk_types::Command> command = init_command(vulkan_device, vulkan_gpu, DOUBLE_BUFFER, cleanup_procedures);
        const vk_types::Command command_immediate = init_command(vulkan_device, vulkan_gpu, 1, cleanup_procedures)[0];
        const std::vector<vk_types::Synchronization> synchronization = init_synchronization(vulkan_device, DOUBLE_BUFFER, cleanup_procedures);
        const VkFence fence_immediate = init_fence(vulkan_device, cleanup_procedures);

        const VmaAllocator allocator = init_allocator(vulkan_instance, vulkan_device, vulkan_gpu, cleanup_procedures);

        // Provide an image that will be the intermediate draw target
        VkFormat draw_target_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkImageUsageFlags draw_target_flags = 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT 
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT 
            | VK_IMAGE_USAGE_STORAGE_BIT 
            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        VkExtent2D draw_target_extent = {
            swapchain.extent.width,
            swapchain.extent.height
        };

        vk_types::AllocatedImage draw_target = init_allocated_image(vulkan_device, allocator, draw_target_format, draw_target_flags, draw_target_extent, cleanup_procedures);

        // Allocate a depth target as well
        // TODO: make more robust format checks
        VkFormat depth_buffer_format = VK_FORMAT_D16_UNORM;
        VkImageUsageFlags depth_buffer_flags = 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkExtent2D depth_buffer_extent = {
            swapchain.extent.width,
            swapchain.extent.height
        };

        vk_types::AllocatedImage depth_buffer = init_allocated_image(vulkan_device, allocator, depth_buffer_format, depth_buffer_flags, depth_buffer_extent, cleanup_procedures);

        // Setup descriptors for compute pipeline
        vk_descriptors::DescriptorAllocator descriptor_allocator = {};
        std::vector<VkDescriptorType> compute_descriptor_types = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
        VkDescriptorSetLayout compute_descriptor_layout = vk_descriptors::init_descriptor_layout(vulkan_device, VK_SHADER_STAGE_COMPUTE_BIT, compute_descriptor_types, cleanup_procedures);
        VkDescriptorSet compute_descriptor_set = vk_descriptors::init_image_descriptors(vulkan_device, draw_target.image_view, compute_descriptor_layout, descriptor_allocator, cleanup_procedures);

        // Assemble the compute pipeline
        VkShaderModule compute_shader = init_shader_module(vulkan_device, "../../../src/shaders/gradient.glsl.comp.spv", cleanup_procedures);
        std::vector<VkDescriptorSetLayout> compute_descriptor_set_layouts = { compute_descriptor_layout };
        VkPipelineLayout compute_pipeline_layout = init_pipeline_layout(vulkan_device, compute_descriptor_set_layouts, cleanup_procedures);
        vk_types::Pipeline compute_pipeline = init_compute_pipeline(vulkan_device, compute_pipeline_layout, compute_shader, compute_descriptor_set, cleanup_procedures);

        // Assemble the graphics pipeline
        VkShaderModule vert_shader = init_shader_module(vulkan_device, "../../../src/shaders/colored_triangle.glsl.vert.spv", cleanup_procedures);
        VkShaderModule frag_shader = init_shader_module(vulkan_device, "../../../src/shaders/colored_triangle.glsl.frag.spv", cleanup_procedures);
        
        VkPipelineLayout graphics_pipeline_layout = init_pipeline_layout(vulkan_device, {}, cleanup_procedures);
        vk_types::Pipeline graphics_pipeline = init_graphics_pipeline(vulkan_device, graphics_pipeline_layout, draw_target.image_format, depth_buffer.image_format, vert_shader, frag_shader, {}, cleanup_procedures);
        return vk_types::Context {
            cleanup_procedures,
            vulkan_instance,
            vulkan_gpu.gpu,
            vulkan_device,
            vulkan_surface,
            swapchain,
            queues,
            command,
            command_immediate,
            synchronization,
            fence_immediate,
            allocator,
            draw_target,
            depth_buffer,
            compute_pipeline,
            graphics_pipeline,
            DOUBLE_BUFFER
        };
    }
}