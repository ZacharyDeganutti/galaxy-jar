#!/bin/bash
DEPS="external"
mkdir -p $DEPS
cd $DEPS

# Get GLFW 3.4 from github and extract it
curl -LO "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
unzip glfw-3.4.bin.WIN64.zip > /dev/null
