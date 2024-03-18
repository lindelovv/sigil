CFLAGS=-std=c++20 -g -march=native -O2 -pipe -fuse-ld=mold -D_GLFW_WAYLAND -D_GLFW_EGL $(DEFINE)
LDFLAGS=-lglfw -lvulkan -limgui -lassimp -ldl -lX11 -lXxf86vm -lXrandr -lXi
NAME=default

compile c:
	g++-13 $(CFLAGS) $(EXFLAGS) $(shell find ./src/* -name "*.cc") $(shell find ./src/* -type d | sed s/^/-I/) -o ./build/sigil $(LDFLAGS)
run r:
	./build/sigil
debug d:
	gdb ./build/sigil
clean cln:
	rm -f ./build/sigil
vertexc scv:
	glslc -c ./shaders/$(NAME).vert -o ./shaders/$(NAME).vert.spv
fragmentc scf:
	glslc -c ./shaders/$(NAME).frag -o ./shaders/$(NAME).frag.spv
computec scc:
	glslc -c ./shaders/$(NAME).comp -o ./shaders/$(NAME).comp.spv
shaderc sc:
	$(MAKE) vertexc && $(MAKE) fragmentc && $(MAKE) fragmentc

