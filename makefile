CFLAGS=-std=c++20 -g -march=native -O2 -pipe -fuse-ld=mold -D_GLFW_WAYLAND -D_GLFW_EGL $(DEFINE)
LDFLAGS=-lglfw -lvulkan -lassimp -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=default

#g++-13 $(CFLAGS) $(EXFLAGS) $(shell find ./src/* -name "*.cc") $(shell find ./src/* -type d | sed s/^/-I/) -I./submodules/imgui -I./submodules/VulkanMemoryAllocator-Hpp/include/ -I./submodules/VulkanMemoryAllocator-Hpp/VulkanMemoryAllocator/include -o ./build/sigil $(LDFLAGS)

compile c:
	ninja -C build
run r:
	./build/sigil
debug d:
	gdb ./build/sigil
clean cln:
	rm -f ./build/sigil
vertexc scv:
	glslc -c ./src/shaders/$(NAME).vert -o ./res/shaders/$(NAME).vert.spv
fragmentc scf:
	glslc -c ./src/shaders/$(NAME).frag -o ./res/shaders/$(NAME).frag.spv
computec scc:
	glslc -c ./src/shaders/$(NAME).comp -o ./res/shaders/$(NAME).comp.spv
shaderc sc:
	$(MAKE) vertexc && $(MAKE) fragmentc && $(MAKE) fragmentc

