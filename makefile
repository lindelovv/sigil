CFLAGS=-std=c++17 -O2 -D_GLFW_WAYLAND -D_GLFW_EGL
LDFLAGS=-lglfw -lvulkan -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=tmp

all:
	g++-12 $(CFLAGS) ./src/*.cpp -I*.h -o ./build/run $(LDFLAGS)
and_run:
	$(MAKE) all && ./build/run
and_debug:
	$(MAKE) all && $(MAKE) debug
debug:
	gdb ./build/run
clean:
	rm -f ./build/run
vertex:
	glslc ./shaders/$(NAME).vert -o ./shaders/$(NAME).vert.spv
fragment:
	glslc ./shaders/$(NAME).frag -o ./shaders/$(NAME).frag.spv
shaderc:
	$(MAKE) vertex && $(MAKE) fragment
