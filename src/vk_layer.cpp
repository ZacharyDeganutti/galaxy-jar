#include "vk_layer.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <iterator>
#include <algorithm>

namespace vk_layer {
    namespace {
        #ifndef NDEBUG
        const std::vector<const char*> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };
        #endif

        struct QueueFamilyCollection {
            std::vector<uint32_t> indices;
            std::vector<VkQueueFamilyProperties> properties;
        };

        struct SwapchainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> present_modes;
        };

        SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
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

        QueueFamilyCollection filter(const QueueFamilyCollection& families, std::function<bool(size_t)> criteria) {
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

        VkInstance init_instance(const int extension_count, const char* const* extension_names) {
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
            return instance;
        }

        typedef struct GpuAndQueueInfo {
            VkPhysicalDevice gpu;
            QueueFamilyCollection graphics;
            QueueFamilyCollection presentation;
        } GpuInfo;

        GpuAndQueueInfo init_physical_device(const VkInstance instance, const std::vector<const char*>& required_extensions, VkSurfaceKHR surface) {
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
                // We also need to know if a graphics capable queue family exists
                QueueFamilyCollection queue_families = find_queue_families(device);
                QueueFamilyCollection supporting_graphics = filter_for_feature_compatability(queue_families, VK_QUEUE_GRAPHICS_BIT);
                QueueFamilyCollection supporting_presentation = filter_for_presentation_compatibility(device, surface, queue_families);

                // If the device can't do what the program needs, disqualify it completely
                if ((!supports_extensions) || 
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

        VkDevice init_logical_device(const GpuAndQueueInfo& gpu_info, const std::vector<const char*>& required_extensions) {
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

            VkPhysicalDeviceFeatures device_features{};

            VkDeviceCreateInfo device_create_info{};
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pQueueCreateInfos = queue_create_infos.data();
            device_create_info.queueCreateInfoCount = queue_create_infos.size();
            device_create_info.pEnabledFeatures = &device_features;
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

            return device;
        }

        VkSurfaceKHR init_surface(const VkInstance instance, GLFWwindow* window) {
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

            return surface;
        }

        VkSurfaceFormatKHR choose_swapchain_surface_format(const std::vector<VkSurfaceFormatKHR>& format_list) {
            // Prefer SRGB
            for (const auto& format : format_list) {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
            // Barring availability of that, just grab whatever's in front
}           return format_list[0];
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

        VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
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

        std::vector<VkImageView> init_image_views(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage> images, VkFormat format) {
            std::vector<VkImageView> swapchain_views(images.size());
            for (size_t i = 0; i < images.size(); ++i) {
                VkImageViewCreateInfo image_view_create_info{};
                image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                image_view_create_info.image = images[i];
                image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                image_view_create_info.format = format;
                image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_view_create_info.subresourceRange.baseMipLevel = 0;
                image_view_create_info.subresourceRange.levelCount = 1;
                image_view_create_info.subresourceRange.baseArrayLayer = 0;
                image_view_create_info.subresourceRange.layerCount = 1;

                if (vkCreateImageView(device, &image_view_create_info, nullptr, &swapchain_views[i]) != VK_SUCCESS) {
                    printf("Unable to create image view for image %d\n", i);
                    exit(EXIT_FAILURE);
                }
            }

            return swapchain_views;
        }

        Swapchain init_swapchain(const VkDevice device, const GpuAndQueueInfo& gpu_info, const VkSurfaceKHR surface, const uint32_t width, const uint32_t height) {
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
            swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
            std::vector<VkImageView> swapchain_views = init_image_views(device, swapchain_handle, swapchain_images, surface_format.format);
            // Tie all the swapchain bits and bobs together for later use
            Swapchain swapchain{};
            swapchain.handle = swapchain_handle;
            swapchain.format = surface_format.format;
            swapchain.extent = extent;
            swapchain.images = swapchain_images;
            swapchain.views  = swapchain_views;
            return swapchain;
        }
    }

    Resources init(const std::vector<const char*>& required_device_extensions, const std::vector<const char*>& glfw_extensions, GLFWwindow* window) {
        VkInstance vulkan_instance = vk_layer::init_instance(glfw_extensions.size(), glfw_extensions.data());
        VkSurfaceKHR vulkan_surface = vk_layer::init_surface(vulkan_instance, window);
        vk_layer::GpuAndQueueInfo vulkan_gpu = vk_layer::init_physical_device(vulkan_instance, required_device_extensions, vulkan_surface);
        VkDevice vulkan_device = vk_layer::init_logical_device(vulkan_gpu, required_device_extensions);
        
        int w;
        int h;
        glfwGetFramebufferSize(window, &w, &h);
        uint32_t width = static_cast<uint32_t>(w);
        uint32_t height = static_cast<uint32_t>(h);

        Swapchain swapchain = vk_layer::init_swapchain(vulkan_device, vulkan_gpu, vulkan_surface, width, height);

        return Resources {
            vulkan_instance,
            vulkan_gpu.gpu,
            vulkan_device,
            vulkan_surface,
            swapchain,
        };
    }

    void cleanup(Resources& resources) {
        for (auto image_view : resources.swapchain.views) {
            vkDestroyImageView(resources.device, image_view, nullptr);
        }
        vkDestroySwapchainKHR(resources.device, resources.swapchain.handle, nullptr);
        vkDestroySurfaceKHR(resources.instance, resources.surface, nullptr);
        vkDestroyDevice(resources.device, nullptr);
        vkDestroyInstance(resources.instance, nullptr);
    }
}
