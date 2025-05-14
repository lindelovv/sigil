
package ism

import "core:fmt"
import vk "vendor:vulkan"
import "vendor:cgltf"
import "lib:slang"
import "lib:jolt"

@(disabled=!ODIN_DEBUG) __log :: proc(msg: string, args: ..any) { fmt.printf("__log: %s %w", msg, args); fmt.printf("\n") }

@(disabled=!ODIN_DEBUG) __ensure_sresult :: proc(res: slang.Result, msg: string = "")  { if res != slang.OK do fmt.printf("__error: %#s\n", res, msg) }
@(disabled=!ODIN_DEBUG) __ensure_vkresult :: proc(res: vk.Result, msg: string = "")    { if res != .SUCCESS do fmt.printf("__error: %#s\n", res, msg) }
@(disabled=!ODIN_DEBUG) __ensure_b32      :: proc(res: b32, msg: string = "")          { if !res            do fmt.printf("__error: %s\n", msg)       }
@(disabled=!ODIN_DEBUG) __ensure_bool     :: proc(res: bool, msg: string = "")         { if !res            do fmt.printf("__error: %s\n", msg)       }
@(disabled=!ODIN_DEBUG) __ensure_ptr      :: proc(res: rawptr, msg: string = "")       { if res == nil      do fmt.printf("__error: %s\n", msg)       }
@(disabled=!ODIN_DEBUG) __ensure_gltfres  :: proc(res: cgltf.result, msg: string = "") { if res != .success do fmt.printf("__error: %s\n", msg)       }
@(disabled=!ODIN_DEBUG) __ensure_jolt     :: proc(res: jolt.PhysicsUpdateError, msg: string = "")  { if res != .None do fmt.printf("__error: %s\n", msg)       }
__ensure :: proc { __ensure_sresult, __ensure_vkresult, __ensure_bool, __ensure_b32, __ensure_ptr, __ensure_gltfres, __ensure_jolt }

