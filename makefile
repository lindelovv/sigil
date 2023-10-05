CFLAGS=-std=c++20 -g -O2 -D_GLFW_WAYLAND -D_GLFW_EGL
LDFLAGS=-lglfw -lvulkan -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=tmp

all:
	g++-13 -Wall -Wpedantic $(CFLAGS) $(EXFLAGS) ./src/*.cc -I*.hh -o ./build/sigil $(LDFLAGS)
and_run:
	$(MAKE) all && $(MAKE) run
and_debug:
	$(MAKE) all && $(MAKE) debug
run:
	./build/sigil
debug:
	gdb ./build/sigil
clean:
	rm -f ./build/sigil
vertex:
	glslc ./shaders/$(NAME).vert -o ./shaders/$(NAME).vert.spv
fragment:
	glslc ./shaders/$(NAME).frag -o ./shaders/$(NAME).frag.spv
shaderc:
	$(MAKE) vertex && $(MAKE) fragment
