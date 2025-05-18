ifeq ($(OS),Windows_NT)
	EXT=.exe
else
	EXT=
endif

all:
	$(MAKE) c r
release:
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/
build c:
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -debug -show-system-calls
debug d:
	gdb ./build/sigil$(EXT)
run r:
	./build/sigil$(EXT)
clean cln:
	rm -f ./build/sigil$(EXT)


NAME=default

vertexc scv:
	glslc -c ./sigil/ism/shaders/$(NAME).vert -o ./res/shaders/$(NAME).vert.spv
fragmentc scf:
	glslc -c ./sigil/ism/shaders/$(NAME).frag -o ./res/shaders/$(NAME).frag.spv
computec scc:
	glslc -c ./sigil/ism/shaders/$(NAME).comp -o ./res/shaders/$(NAME).comp.spv
shaderc sc:
	$(MAKE) vertexc && $(MAKE) fragmentc && $(MAKE) fragmentc
