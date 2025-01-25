
all:
	$(MAKE) c r
release:
	odin build ./ -out:./build/sigil
build c:
	LD_LIBRARY_PATH=./build/ odin build ./ -out:./build/sigil -collection:lib=./lib/ -collection:sigil=./sigil/ -debug -extra-linker-flags:-lstdc++ -show-system-calls
debug d:
	LD_LIBRARY_PATH=./build/ gdb ./build/sigil
run r:
	LD_LIBRARY_PATH=./build/ ./build/sigil
clean cln:
	rm -f ./build/sigil


NAME=default

vertexc scv:
	glslc -c ./sigil/ism/shaders/$(NAME).vert -o ./res/shaders/$(NAME).vert.spv
fragmentc scf:
	glslc -c ./sigil/ism/shaders/$(NAME).frag -o ./res/shaders/$(NAME).frag.spv
computec scc:
	glslc -c ./sigil/ism/shaders/$(NAME).comp -o ./res/shaders/$(NAME).comp.spv
shaderc sc:
	$(MAKE) vertexc && $(MAKE) fragmentc && $(MAKE) fragmentc
