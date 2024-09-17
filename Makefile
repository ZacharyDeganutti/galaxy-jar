SHELL:=/usr/bin/bash
CXX=clang++
CXXFLAGS=-Wall -pipe -std=c++20 -Werror=return-type
GLSLC=glslc
GLSLFLAGS=
LIBS_PATH=external
DEPS=$(LIBS_PATH)/VulkanMemoryAllocator-3.1.0 $(LIBS_PATH)/glfw-3.4.bin.WIN64 $(LIBS_PATH)
LDFLAGS= -lglfw3dll -lvulkan-1
GLFW=glfw-3.4.bin.WIN64
GLFW_LIB=lib-static-ucrt
LDPATHS='-L$(LIBS_PATH)/$(GLFW)/$(GLFW_LIB)' '-L$(VULKAN_SDK)/Lib'
INCLUDE_PATHS='-I$(LIBS_PATH)/VulkanMemoryAllocator-3.1.0/include' '-I$(LIBS_PATH)/glfw-3.4.bin.WIN64/include' '-I$(VULKAN_SDK)\Include'
SRC=$(wildcard src/*.cpp)
OUTDIR=build
DBG_OBJ_PATH=$(OUTDIR)/Debug/obj
REL_OBJ_PATH=$(OUTDIR)/Release/obj
DBG_OBJ=$(SRC:%.cpp=$(OUTDIR)/Debug/obj/%.o)
REL_OBJ=$(SRC:%.cpp=$(OUTDIR)/Release/obj/%.o)
SHADERSRC=$(wildcard *.glsl.*)
SHADEROBJ=$(SHADERSRC).spv
DBG_OUT=$(OUTDIR)/Debug/bin/galaxy-jar.exe
REL_OUT=$(OUTDIR)/Release/bin/galaxy-jar.exe

.PHONY: all debug release run_debug run_release clean cleanall check_deps

all: debug

debug:   CXXFLAGS += -g -O0
release: CXXFLAGS += -O2 -DNDEBUG

debug release: $(SHADEROBJ)
debug: $(DBG_OUT)
release: $(REL_OUT)

$(DBG_OBJ): $(DBG_OBJ_PATH)/%.o: %.cpp check_deps
	mkdir -p $$(dirname $(DBG_OBJ))
	$(CXX) -c $(INCLUDE_PATHS) $(CXXFLAGS) $< -o $@

$(REL_OBJ): $(REL_OBJ_PATH)/%.o: %.cpp check_deps
	mkdir -p $$(dirname $(REL_OBJ))
	$(CXX) -c $(INCLUDE_PATHS) $(CXXFLAGS) $< -o $@

$(REL_OUT): $(REL_OBJ)
	mkdir -p $(OUTDIR)/Release/bin
	cp $(LIBS_PATH)/$(GLFW)/$(GLFW_LIB)/glfw3.dll $(OUTDIR)/Release/bin
	$(CXX) -v $(CXXFLAGS) $(REL_OBJ) $(LDPATHS) $(LDFLAGS) -o $@

$(DBG_OUT): $(DBG_OBJ)
	mkdir -p $(OUTDIR)/Debug/bin
	cp $(LIBS_PATH)/$(GLFW)/$(GLFW_LIB)/glfw3.dll $(OUTDIR)/Debug/bin
	$(CXX) -v $(CXXFLAGS) $(DBG_OBJ) $(LDPATHS) $(LDFLAGS) -o $@

%.vert.spv: %.vert.glsl check_deps 
	$(GLSLC) $(GLSLFLAGS) -fshader-stage=vert $< -o $@

%.frag.spv: %.frag.glsl check_deps
	$(GLSLC) $(GLSLFLAGS) -fshader-stage=frag $< -o $@

run_debug: debug
	./$(DBG_OUT)

run_release: release
	./$(REL_OUT)

clean:
	-rm $(SHADEROBJ)
	-rm -r $(OUTDIR)

cleanall: clean
	-rm -r $(DEPS)

check_deps:
	bash check_deps.sh

download: $(DEPS)

$(DEPS):
	bash download_deps.sh
