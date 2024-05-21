
compile c:
	odin build ./src/ -out:./build/sigil
run r:
	odin run ./src/ -out:./build/sigil
debug d:
	gdb ./build/sigil
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
