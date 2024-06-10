SHELL:=/usr/bin/bash
CXX=clang++
CXXFLAGS=-Wall -pipe -std=c++17
GLSLC=glslc
GLSLFLAGS=
DEPS=external
LDFLAGS= -lglfw3dll -lvulkan-1
GLFW=glfw-3.4.bin.WIN64
GLFW_LIB=lib-static-ucrt
LDPATHS='-L$(DEPS)/$(GLFW)/$(GLFW_LIB)' '-L$(VULKAN_SDK)/Lib'
INCLUDE_PATHS='-I$(DEPS)/glfw-3.4.bin.WIN64/include' '-I$(VULKAN_SDK)\Include'
SRC=$(wildcard src/*.cpp)
OUTDIR=build
DBG_OBJ=$(SRC:%.cpp=$(OUTDIR)/Debug/obj/%.o)
REL_OBJ=$(SRC:%.cpp=$(OUTDIR)/Release/obj/%.o)
SHADERSRC=$(wildcard *.glsl)
SHADEROBJ=$(SHADERSRC:.glsl=.spv)
DBG_OUT=$(OUTDIR)/Debug/bin/galaxy-jar.exe
REL_OUT=$(OUTDIR)/Release/bin/galaxy-jar.exe

.PHONY: all debug release run_debug run_release clean cleanall check_deps

all: debug

debug:   CXXFLAGS += -g -O0
release: CXXFLAGS += -O2 -DNDEBUG

debug release: $(SHADEROBJ)
debug: $(DBG_OUT)
release: $(REL_OUT)

$(DBG_OBJ): check_deps
	mkdir -p $$(dirname $(DBG_OBJ))
	$(CXX) -c $(INCLUDE_PATHS) $(CXXFLAGS) $(SRC) -o $@

$(REL_OBJ): check_deps
	mkdir -p $$(dirname $(REL_OBJ))
	$(CXX) -c $(INCLUDE_PATHS) $(CXXFLAGS) $(SRC) -o $@

$(REL_OUT): $(REL_OBJ)
	mkdir -p $(OUTDIR)/Release/bin
	cp $(DEPS)/$(GLFW)/$(GLFW_LIB)/glfw3.dll $(OUTDIR)/Release/bin
	$(CXX) -v $(CXXFLAGS) $(REL_OBJ) $(LDPATHS) $(LDFLAGS) -o $@

$(DBG_OUT): $(DBG_OBJ)
	mkdir -p $(OUTDIR)/Debug/bin
	cp $(DEPS)/$(GLFW)/$(GLFW_LIB)/glfw3.dll $(OUTDIR)/Debug/bin
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
