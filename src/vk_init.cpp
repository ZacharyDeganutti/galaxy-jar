#include "vk_image.hpp"
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
    bool are_vulkan_1_2_features_supported(const VkPhysicalDevice device);
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
    vk_types::Swapchain init_swapchain(const VkDevice device, const GpuAndQueueInfo& gpu_info, const VkSurfaceKHR surface, const uint32_t width, const uint32_t height, vk_types::CleanupProcedures& cleanup_procedures);
    std::vector<vk_types::Command> init_command(const VkDevice device, const GpuAndQueueInfo& gpu, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures);
    std::vector<vk_types::Synchronization> init_synchronization(const VkDevice device, const uint8_t buffer_count, vk_types::CleanupProcedures& cleanup_procedures);
    VmaAllocator init_allocator(const VkInstance instance, const VkDevice device, const GpuAndQueueInfo& gpu, vk_types::CleanupProcedures& cleanup_procedures);
}

namespace vk_init {

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

    bool are_vulkan_1_2_features_supported(const VkPhysicalDevice device) {
        VkPhysicalDeviceVulkan12Features features12prefill = {};
        features12prefill.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12prefill;
        vkGetPhysicalDeviceFeatures2(device, &features2);
        VkBaseOutStructure* next = reinterpret_cast<VkBaseOutStructure*>(features2.pNext);
        while (next != nullptr) {
            if (next->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
                VkPhysicalDeviceVulkan12Features* features12 = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(next);
                if (features12->descriptorIndexing && 
                    features12->descriptorBindingPartiallyBound && 
                    features12->bufferDeviceAddress && 
                    features12->runtimeDescriptorArray &&
                    features12->shaderStorageImageArrayNonUniformIndexing &&
                    features12->shaderSampledImageArrayNonUniformIndexing) 
                {
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
            bool supports_features = are_vulkan_1_2_features_supported(device) &&
                                     are_vulkan_1_3_features_supported(device);

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
        device_features.samplerAnisotropy = VK_TRUE;
        VkPhysicalDeviceVulkan13Features features13 = {};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        features13.pNext = nullptr;

        VkPhysicalDeviceVulkan12Features features12 = {};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.runtimeDescriptorArray = VK_TRUE;
        features12.descriptorBindingPartiallyBound = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;
        features12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
        features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
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
        const uint32_t NO_MIPMAPS = 1;
        std::vector<VkImageView> swapchain_views(swapchain_images.size());
        for (auto swapchain_image : swapchain_images) {
            VkImageView view = vk_image::init_image_view(device, swapchain_image, vk_image::Representation::Flat, surface_format.format, NO_MIPMAPS, cleanup_procedures);
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

        vk_descriptors::DescriptorAllocator descriptor_allocator = {};

        constexpr size_t POOL_SIZES = 1000;
        vk_descriptors::MegaDescriptorSet mega_descriptor_set = vk_descriptors::init_mega_descriptor_set(vulkan_device, descriptor_allocator, POOL_SIZES, cleanup_procedures);
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
            mega_descriptor_set,
            DOUBLE_BUFFER
        };
    }
}