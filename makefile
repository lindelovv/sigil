ifeq ($(OS),Windows_NT)
	EXT=.exe
	DBG=raddbg.exe
else
	EXT=
	DBG=gdb
endif

ODIN_ROOT=$(odin root)
ODIN_JS="$ODIN_ROOT/core/sys/wasm/js/odin.js"
WGPU_JS="$ODIN_ROOT/vendor/wgpu/wgpu.js"
INITIAL_MEMORY_PAGES=2000
MAX_MEMORY_PAGES=65536

SHADER_DIR := sigil/ism/shaders
SPV_TARGETS := $(patsubst ./$(SHADER_DIR)/%.slang,$(SHADER_DIR)/%.spv,\
    $(filter-out $(SHADER_DIR)/%.shared.slang,$(wildcard $(SHADER_DIR)/*.slang)))

$(SHADER_DIR)/%.spv: $(SHADER_DIR)/%.slang
	./lib/slang/bin/slangc -target spirv -profile spirv_1_6 \
	       -fvk-use-entrypoint-name \
	       -fvk-use-gl-layout \
	       -O1 -o $@ $<

all:
	$(MAKE) c r
release: 
	$(SPV_TARGETS)
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -o:speed 
build c:
	odin build ./ -out:./build/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -debug
debug d:
	$(DBG) ./build/sigil$(EXT)
web w:
	odin build ./ -target:js_wasm32 -out:./build/web/sigil$(EXT) -collection:lib=./lib/ -collection:sigil=./sigil/ -o:size \
		-extra-linker-flags:"--export-table --import-memory --initial-memory=131072000 --max-memory=4294967296"
	cp $ODIN_JS web/odin.js
	cp $WGPU_JS web/wgpu.js
run r:
	./build/sigil$(EXT)
clean cln:
	rm -f ./build/sigil$(EXT)

