package sigil
import "vendor:glfw"

__glfw :: Module {
    setup = proc() {
        using eRUNLVL
        schedule(init, init_glfw)
        schedule(tick, tick_glfw)
        schedule(exit, exit_glfw)
    }
}

//_____________________________
window  : glfw.WindowHandle

//_____________________________
should_close :: proc() -> b32 {
    return glfw.WindowShouldClose(window)
}

//_____________________________
init_glfw :: proc() {
    check_err(glfw.Init(), "GLFW Init Failed")

    //glfw.WindowHint(glfw.CLIENT_API, glfw.NO_API)
    glfw.WindowHint(glfw.DECORATED, glfw.FALSE)
    window = glfw.CreateWindow(i32(window_extent.width), i32(window_extent.height), TITLE, nil, nil)
    check_err(window, "GLFW CreateWindow Failed")
    glfw.SetFramebufferSizeCallback(window, glfw.FramebufferSizeProc(on_resize))
    glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
    glfw.SetKeyCallback(window, glfw.KeyProc(keyboard_callback))
    glfw.SetMouseButtonCallback(window, glfw.MouseButtonProc(mouse_callback))

    setup_standard_bindings()

    glfw.MakeContextCurrent(window)
}

on_resize :: proc() {
    dbg_msg("resize")
}

//_____________________________
tick_glfw :: proc() {
    glfw.PollEvents()
    glfw.SwapBuffers(window)
}

//_____________________________
exit_glfw :: proc() {
    glfw.DestroyWindow(window)
    glfw.Terminate()
}

//_____________________________
input_callbacks     := make(map[Key]KeyCallback)
mouse_position      :  vec2
last_mouse_position :  vec2

Key :: i32
KeyCallback :: struct {
    press:   proc(),
    release: proc(),
}

//_____________________________
bind_input :: proc(key: Key, callbacks: KeyCallback) {
    if _, exists := input_callbacks[key]; !exists {
        input_callbacks[key] = {}
    }
    input_callbacks[key] = callbacks
}

//_____________________________
keyboard_callback :: proc(window: glfw.WindowHandle, key: i32, scancode: i32, action: i32, mods: i32) {
    if callback, exists := input_callbacks[key]; exists {
        if action == glfw.PRESS   { if callback.press != nil   { callback.press()   } }
        if action == glfw.RELEASE { if callback.release != nil { callback.release() } }
    }
}

//_____________________________
mouse_callback :: proc(window: glfw.WindowHandle, key: i32, scancode: i32, action: i32, mods: i32) {
    if callback, exists := input_callbacks[key]; exists {
        if action == glfw.PRESS   { if callback.press != nil   { callback.press()   } }
        if action == glfw.RELEASE { if callback.release != nil { callback.release() } }
    }
}

//_____________________________
setup_standard_bindings :: proc() {
    bind_input(glfw.KEY_T,      KeyCallback { press = proc() { dbg_msg("test") } })
    bind_input(glfw.KEY_ESCAPE, KeyCallback { press = proc() { glfw.SetWindowShouldClose(window, true) } })
}

