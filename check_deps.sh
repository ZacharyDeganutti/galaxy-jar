#!/bin/bash

if ! vk_installed=$(which glslc > /dev/null 2>&1); then
    echo "glslc not found, please ensure that the Vulkan SDK is installed on your computer, and your PATH is configured";
    exit 1;
fi

if [ ! -f external/glfw-3.4.bin.WIN64/lib-mingw-w64/glfw3.dll ]; then
    echo "GLFW library not found, run 'make download'";
    exit 1;
fi

exit 0;