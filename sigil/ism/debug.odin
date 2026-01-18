package ism

import "core:fmt"
import vk "vendor:vulkan"
import "vendor:cgltf"
import "lib:slang"
import "lib:jolt"
import "vendor:miniaudio"

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(disabled=!ODIN_DEBUG) __log :: proc(msg: string, args: ..any) { fmt.printf("__log: %s %w", msg, args); fmt.printf("\n") }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

__ensure_slang_result :: #force_inline proc(res: slang.Result, msg: string = "") {
    when ODIN_DEBUG do if res != slang.OK do fmt.printf("__error: %#w %v\n", res, msg)
}
__ensure_vkresult :: #force_inline proc(res: vk.Result, msg: string = "") {
    when ODIN_DEBUG do if res != .SUCCESS do fmt.printf("__error: %#s\n", res, msg)
}
__ensure_b32 :: #force_inline proc(res: b32, msg: string = "") {
    when ODIN_DEBUG do if !res do fmt.printf("__error: %s\n", msg)
}
__ensure_bool :: #force_inline proc(res: bool, msg: string = "") {
    when ODIN_DEBUG do if !res do fmt.printf("__error: %s\n", msg)
}
__ensure_ptr :: #force_inline proc(res: rawptr, msg: string = "") {
    when ODIN_DEBUG do if res == nil do fmt.printf("__error: %s\n", msg)
}
__ensure_gltfres :: #force_inline proc(res: cgltf.result, msg: string = "") {
    when ODIN_DEBUG do if res != .success do fmt.printf("__error: %s\n", msg)
}
__ensure_jolt :: #force_inline proc(res: jolt.PhysicsUpdateError, msg: string = "") {
    when ODIN_DEBUG do if res != .JPH_PhysicsUpdateError_None do fmt.printf("__error: %s\n", msg)
}
__ensure_miniaudio :: #force_inline proc(res: miniaudio.result, msg: string = "") {
    when ODIN_DEBUG do if res != .SUCCESS do fmt.printf("__error: %s\n", msg)
}
__ensure :: proc { __ensure_slang_result, __ensure_vkresult, __ensure_bool, __ensure_b32, __ensure_ptr, __ensure_gltfres, __ensure_jolt, __ensure_miniaudio }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

