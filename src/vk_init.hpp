#ifndef VK_INIT_H_
#define VK_INIT_H_

#include "vk_mem_alloc.h"
#include "vk_types.hpp"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <deque>
#include <functional>

namespace vk_init {

    vk_types::Context init(const std::vector<const char*>& required_device_extensions, const std::vector<const char*>& glfw_extensions, GLFWwindow* window);
}

#endif