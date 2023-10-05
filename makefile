CFLAGS=-std=c++20 -g -O2 -D_GLFW_WAYLAND -D_GLFW_EGL
LDFLAGS=-lglfw -lvulkan -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=simple_shader

compile c:
	g++-13 -Wall -Wpedantic $(CFLAGS) $(EXFLAGS) ./src/*.cc -I*.hh -o ./build/sigil $(LDFLAGS)
run r:
	./build/sigil
debug d:
	gdb ./build/sigil
clean cln:
	rm -f ./build/sigil
vertexc:
	glslc ./shaders/$(NAME).vert -o ./shaders/$(NAME).vert.spv
fragmentc:
	glslc ./shaders/$(NAME).frag -o ./shaders/$(NAME).frag.spv
shaderc sc:
	$(MAKE) vertex && $(MAKE) fragment
