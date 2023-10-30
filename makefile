CFLAGS=-std=c++20 -g -O2 -D_GLFW_WAYLAND -D_GLFW_EGL $(DEFINE)
LDFLAGS=-lglfw -lvulkan -limgui -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=simple_shader

compile c:
	g++-13 -Wall -Wpedantic $(CFLAGS) $(EXFLAGS) $(shell find ./src/* -name "*.cc") $(shell find ./src/* -type d | sed s/^/-I/) -o ./build/sigil $(LDFLAGS)
run r:
	./build/sigil
debug d:
	gdb ./build/sigil
clean cln:
	rm -f ./build/sigil
vertexc scv:
	glslc ./shaders/$(NAME).vert -o ./shaders/$(NAME).vert.spv
fragmentc scf:
	glslc ./shaders/$(NAME).frag -o ./shaders/$(NAME).frag.spv
shaderc sc:
	$(MAKE) vertex && $(MAKE) fragment

