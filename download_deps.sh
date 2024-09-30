#!/bin/bash
DEPS="external"
mkdir -p $DEPS
cd $DEPS

# Get GLFW 3.4 from github and extract it
curl -LO "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
unzip -o glfw-3.4.bin.WIN64.zip > /dev/null

# Get VMA 3.1.0 from github and extract it
curl -LO "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.1.0.zip"
unzip -o v3.1.0.zip > /dev/null

# Get tinyobjloader 1.0.6
curl -LO "https://github.com/tinyobjloader/tinyobjloader/releases/download/v1.0.6/tiny_obj_loader.h"
mkdir tinyobjloader
mv tiny_obj_loader.h tinyobjloader


# Get GLM 1.0.1 from github and extract it
curl -LO "https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip"
unzip -o glm-1.0.1-light.zip > /dev/null
