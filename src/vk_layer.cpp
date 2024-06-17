#include "vk_layer.hpp"
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <iterator>
#include <algorithm>

namespace vk_layer {
    namespace {
        #ifndef NDEBUG
        const std::vector<const char*> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };
        #endif

        typedef struct QueueFamilyCollection {
            std::vector<uint32_t> indices;
            std::vector<VkQueueFamilyProperties> properties;
        } QueueFamilyCollection;

        QueueFamilyCollection find_queue_families(VkPhysicalDevice gpu) {
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

        QueueFamilyCollection filter(QueueFamilyCollection families, std::function<bool(size_t)> criteria) {
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

        QueueFamilyCollection filter_for_feature_compatability(QueueFamilyCollection families, VkQueueFlagBits queue_feature_flags) {
           return filter(families, [&](size_t index) {
                VkQueueFamilyProperties queue_family = families.properties[index];
                return (queue_family.queueFlags & queue_feature_flags) > 0;
           });
        }

        QueueFamilyCollection filter_for_presentation_compatibility(VkPhysicalDevice gpu, VkSurfaceKHR surface, QueueFamilyCollection families) {
            return filter(families, [&](size_t index) {
                VkBool32 present_support = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(gpu, index, surface, &present_support);
                return present_support;
            });
        }

        VkInstance init_instance(int extension_count, const char** extension_names) {
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

        typedef struct GpuAndQueues {
            VkPhysicalDevice gpu;
            QueueFamilyCollection graphics;
            QueueFamilyCollection presentation;
        } GpuInfo;

        GpuAndQueues init_physical_device(VkInstance instance, VkSurfaceKHR surface) {
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

                // Check if the given device supports the features that we need
                // For now, check if a graphics capable queue family exists
                QueueFamilyCollection queue_families = find_queue_families(device);
                QueueFamilyCollection supporting_graphics = filter_for_feature_compatability(queue_families, VK_QUEUE_GRAPHICS_BIT);
                QueueFamilyCollection supporting_presentation = filter_for_presentation_compatibility(device, surface, queue_families);

                // If the device can't do what the program needs, disqualify it completely
                if ((supporting_graphics.indices.size() == 0) || supporting_presentation.indices.size() == 0) {
                    rankings[device_index] = 0;
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

            return GpuAndQueues {
                best_device,
                supporting_graphics,
                supporting_presentation
            };
        }

        VkDevice init_logical_device(GpuAndQueues& gpu_info) {
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

            float queue_priority = 1.0f;
            VkDeviceQueueCreateInfo device_graphics_queue_create_info{};
            device_graphics_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_graphics_queue_create_info.queueFamilyIndex = graphics_queue_family_indices[0];
            device_graphics_queue_create_info.queueCount = 1;
            device_graphics_queue_create_info.pQueuePriorities = &queue_priority;

            VkDeviceQueueCreateInfo device_presentation_queue_create_info{};
            device_presentation_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            device_presentation_queue_create_info.queueFamilyIndex = presentation_queue_family_indices[0];
            device_presentation_queue_create_info.queueCount = 1;
            device_presentation_queue_create_info.pQueuePriorities = &queue_priority;

            // If the default queue family supports both graphics and presentation, just collapse those together
            std::unordered_set<uint32_t> used_queue_indices = {};
            std::vector<VkDeviceQueueCreateInfo> queue_create_infos = {};
            auto insert_check = used_queue_indices.insert(device_graphics_queue_create_info.queueFamilyIndex);
            queue_create_infos.push_back(device_graphics_queue_create_info);
            insert_check = used_queue_indices.insert(device_presentation_queue_create_info.queueFamilyIndex);
            // Only insert if the presentation queue family is different from the graphics queue family
            if (insert_check.second) {
                queue_create_infos.push_back(device_presentation_queue_create_info);
            }

            VkPhysicalDeviceFeatures device_features{};

            VkDeviceCreateInfo device_create_info{};
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pQueueCreateInfos = queue_create_infos.data();
            device_create_info.queueCreateInfoCount = queue_create_infos.size();
            device_create_info.pEnabledFeatures = &device_features;
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

        VkSurfaceKHR init_surface(VkInstance instance, GLFWwindow* window) {
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
    }

    Resources init(int extension_count, const char** extension_names, GLFWwindow* window) {
        VkInstance vulkan_instance = vk_layer::init_instance(extension_count, extension_names);
        VkSurfaceKHR vulkan_surface = vk_layer::init_surface(vulkan_instance, window);
        vk_layer::GpuAndQueues vulkan_gpu = vk_layer::init_physical_device(vulkan_instance, vulkan_surface);
        VkDevice vulkan_device = vk_layer::init_logical_device(vulkan_gpu);

        return Resources {
            vulkan_instance,
            vulkan_gpu.gpu,
            vulkan_device,
            vulkan_surface,
        };
    }

    void cleanup(Resources& resources) {
        vkDestroySurfaceKHR(resources.instance, resources.surface, nullptr);
        vkDestroyDevice(resources.device, nullptr);
        vkDestroyInstance(resources.instance, nullptr);
    }
}
