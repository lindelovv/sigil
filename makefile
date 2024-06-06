
all:
	$(MAKE) c r
release:
	odin build ./ -out:./build/sigil
build b:
	odin build ./ -out:./build/sigil -debug
debug d:
	gdb ./build/sigil
run r:
	./build/sigil
clean cln:
	rm -f ./build/sigil


NAME=default

vertexc scv:
	glslc -c ./res/shaders/$(NAME).vert -o ./res/shaders/$(NAME).vert.spv
fragmentc scf:
	glslc -c ./res/shaders/$(NAME).frag -o ./res/shaders/$(NAME).frag.spv
computec scc:
	glslc -c ./res/shaders/$(NAME).comp -o ./res/shaders/$(NAME).comp.spv
shaderc sc:
	$(MAKE) vertexc && $(MAKE) fragmentc && $(MAKE) fragmentc
