#include "vk_layer.hpp"
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include <unordered_map>
#include <iterator>
#include <algorithm>

namespace vk_layer {
    namespace {
        std::vector<uint32_t> find_queue_families(VkPhysicalDevice gpu, VkQueueFlagBits queue_feature_flags) {
            uint32_t queue_family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, nullptr);

            std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_family_count, queue_families.data());

            std::vector<uint32_t> viable_indices;
            for (uint32_t queue_family_index = 0; queue_family_index < queue_families.size(); ++queue_family_index) {
                VkQueueFamilyProperties queue_family = queue_families[queue_family_index];
                if ((queue_family.queueFlags & queue_feature_flags) > 0) {
                    viable_indices.push_back(queue_family_index);
                }
            }
            return viable_indices;
        }
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
        const std::vector<const char*> validation_layers = {
            "VK_LAYER_KHRONOS_validation"
        };

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

    VkPhysicalDevice init_physical_device(VkInstance instance) {
        uint32_t device_count = 0;
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
            auto queue_family_indices = find_queue_families(device, VK_QUEUE_GRAPHICS_BIT);

            // If the device can't do what the program needs, disqualify it completely
            if (queue_family_indices.size() == 0) {
                rankings[device_index] = 0;
            }
        }
        // The biggest heap size of the highest priority device type that hasn't been disqualified wins
        size_t winner = std::distance(rankings.begin(), std::max_element(rankings.begin(), rankings.end())); 

        return devices[winner];
    }

    VkDevice init_logical_device(VkPhysicalDevice gpu) {

    }

    void cleanup(VkInstance instance) {
        vkDestroyInstance(instance, nullptr);
    }
}
