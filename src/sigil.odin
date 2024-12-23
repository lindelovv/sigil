//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
package sigil
import "vendor:glfw"
import vk "vendor:vulkan"
import "core:fmt"

//_____________________________
TITLE   :: "__sigil_"
SIGIL_V_STR := "0.0.1"
SIGIL_V_U32 := vk.MAKE_VERSION(0, 0, 1)

//_____________________________
Sigil :: struct { modules: map[typeid]Module }
_sigil : ^Sigil

@(private="file")
delegates := make(map[RunLevel][dynamic]proc())

Module :: struct {
    setup: proc(),
    data:  rawptr,
}

RunLevel :: enum { INIT, TICK, EXIT }

//_____________________________
// Functions

schedule :: proc(runlvl: RunLevel, fn: proc()) {
    if _, exists := &delegates[runlvl]; !exists {
        __log("Creating runlvl array", runlvl)
        delegates[runlvl] = {}
    }
    arr, _ := &delegates[runlvl]
    append(arr, fn)
}

use :: proc(module: Module) { module.setup() }

run :: proc() {
    for fn in delegates[.INIT] do fn()
    main_loop: for !should_close() {
      for fn in delegates[.TICK] do fn()
    }
    for fn in delegates[.EXIT] do fn()
}

//_____________________________
// Logging and error reporting

@(disabled=!ODIN_DEBUG)
__ensure_result :: proc(result: vk.Result, msg: string = "") { if result != .SUCCESS do fmt.printf("__error: %#s\n", result, msg) }
@(disabled=!ODIN_DEBUG)
__ensure_b32 :: proc(result: b32, msg: string = "") { if !result do fmt.printf("__error: %s\n", msg) }
@(disabled=!ODIN_DEBUG)
__ensure_bool :: proc(result: bool, msg: string = "") { if !result do fmt.printf("__error: %s\n", msg) }
@(disabled=!ODIN_DEBUG)
__ensure_ptr :: proc(ptr: rawptr, msg: string = "") { if ptr == nil do fmt.printf("__error: %s\n", msg) }
__ensure :: proc { __ensure_result, __ensure_bool, __ensure_b32, __ensure_ptr, }

@(disabled=!ODIN_DEBUG)
__log :: proc(msg: string, args: ..any) { fmt.printf("__log: %s %w", msg, args); fmt.printf("\n") }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
