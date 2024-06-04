package sigil
import "core:fmt"
import vk "vendor:vulkan"

Error :: union {
    vk.Result,
}

check_err_rawptr :: proc(ptr: rawptr, msg: string) {
    if ptr == nil {
        fmt.eprintln("[ERROR]:", msg)
        return
    }
}

check_err_int :: proc(val: int, msg: string) {
    if !bool(val) {
        fmt.eprintln("[ERROR]:", msg)
        return
    }
}

check_err_bool :: proc(val: bool, msg: string) {
    if !bool(val) {
        fmt.eprintln("[ERROR]:", msg)
        return
    }
}

check_err_b32 :: proc(val: b32, msg: string) {
    if !bool(val) {
        fmt.eprintln("[ERROR]:", msg)
        return
    }
}

check_err_union :: proc(val: Error, msg: string) {
    if (val != .SUCCESS) {
        fmt.eprintln("[ERROR]:", msg)
        return
    }
}

check_err :: proc {
    check_err_rawptr,
    check_err_bool,
    check_err_b32,
    check_err_int,
    check_err_union,
}

@(disabled=!ODIN_DEBUG)
dbg_msg :: proc(msg: string) {
    fmt.eprintln("[DEBUG]:", msg)
}

@(disabled=!ODIN_DEBUG)
dbg_msg_1arg :: proc(msg: string, arg: $a) {
    fmt.eprintln("[DEBUG]:", msg, arg)
}

