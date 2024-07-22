package sigil
import "vendor:glfw"
import vk "vendor:vulkan"
import "core:fmt"

//_____________________________
TITLE   :: "__sigil_"
SIGIL_V := vk.MAKE_VERSION(0, 0, 1)

//_____________________________
Sigil :: struct {
    modules: map[typeid]Module
}
_sigil : ^Sigil

Module :: struct {
    setup: proc(),
    data:  rawptr,
}

//_____________________________
eRUNLVL :: enum {
    init,
    tick,
    exit,
}

//_____________________________
@(private="file")
delegates := make(map[eRUNLVL][dynamic]proc())

//_____________________________
use :: proc(module: Module) {
    module.setup()
}

//_____________________________
schedule :: proc(runlvl: eRUNLVL, fn: proc()) {
    if _, exists := &delegates[runlvl]; !exists {
        dbg_msg_1arg("Creating runlvl array", runlvl)
        delegates[runlvl] = {}
    }
    arr, _ := &delegates[runlvl]
    append(arr, fn)
}

//_____________________________
run :: proc() {
    using eRUNLVL
    for fn in delegates[init] {
        fn()
    }
    for !should_close() {
        for fn in delegates[tick] {
            fn()
        }
    }
    for fn in delegates[exit] {
        fn()
    }
}

