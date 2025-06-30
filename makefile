ifeq ($(OS),Windows_NT)
	EXT=.exe
else
	EXT=
endif

SHADER_DIR := sigil/ism/shaders
SPV_TARGETS := $(patsubst $(SHADER_DIR)/%.slang,$(SHADER_DIR)/%.spv,\
    $(filter-out $(SHADER_DIR)/%.shared.slang,$(wildcard $(SHADER_DIR)/*.slang)))

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.slang
	slangc -target spirv -profile spirv_1_5 \
	       -fvk-use-entrypoint-name \
	       -fvk-use-gl-layout \
	       -O1 -o $@ $<

all:
	$(MAKE) c r
release: $(SPV_TARGETS)
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -o:speed 
build c:
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -debug -show-system-calls
debug d:
	gdb ./build/sigil$(EXT)
run r:
	./build/sigil$(EXT)
clean cln:
	rm -f ./build/sigil$(EXT)

