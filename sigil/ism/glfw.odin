
package ism

import sigil "../core"
import "vendor:glfw"
import vk "vendor:vulkan"
import "core:math/linalg/glsl"

import "core:fmt"

glfw :: proc() {
    using sigil
    schedule(init(init_glfw))
    schedule(tick(tick_glfw))
    schedule(exit(exit_glfw))
}

//_____________________________
window        : glfw.WindowHandle
window_extent := [2]i32 { 2560, 1440 } // { 1920, 1080 }
fps           : f32
ms            : f32
time          : f32
delta_time    : f32
@(private="file") prev_delta : f32

//_____________________________
init_glfw :: proc() {
    glfw.InitHint(glfw.PLATFORM, glfw.PLATFORM_X11) // needed for renderdoc on wayland lol
    __ensure(glfw.Init(), "GLFW Init Failed")

    glfw.WindowHint(glfw.CLIENT_API, glfw.NO_API)
    glfw.WindowHint(glfw.DECORATED, glfw.FALSE)
    window = glfw.CreateWindow(window_extent.x, window_extent.y, sigil.TITLE, nil, nil)
    __ensure(window, "GLFW CreateWindow Failed")
    
    glfw.SetFramebufferSizeCallback(window, glfw.FramebufferSizeProc(on_resize))
    glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
    glfw.SetKeyCallback(window, glfw.KeyProc(keyboard_callback))
    glfw.SetMouseButtonCallback(window, glfw.MouseButtonProc(mouse_callback))
    glfw.MakeContextCurrent(window)

    setup_standard_bindings()
}

//_____________________________
on_resize :: proc() {
    w, h := glfw.GetWindowSize(window)
    resize_window = true
}

//_____________________________
tick_glfw :: proc() {
    last_mouse_position = mouse_position
    x, y := glfw.GetCursorPos(window)
    mouse_position = glsl.vec2 { f32(x), f32(y) }
    glfw.PollEvents()
    glfw.SwapBuffers(window)

    time = f32(glfw.GetTime())
    delta_time = time - prev_delta
    prev_delta = time

    fps = (1000 / delta_time) / 1000
    ms = delta_time * 1000

    sigil.request_exit = cast(bool)glfw.WindowShouldClose(window)
}

//_____________________________
exit_glfw :: proc() {
    for i in sigil.query(type_of(vulkan)) {
        fmt.printf("%v", i)
    }
    glfw.DestroyWindow(window)
    glfw.Terminate()
}

//_____________________________
// Input
input_callbacks     := make(map[Key]KeyCallback)
mouse_position      :  glsl.vec2
last_mouse_position :  glsl.vec2

get_mouse_movement :: proc() -> glsl.vec2 {
    return mouse_position - last_mouse_position
}

Key :: i32
KeyCallback :: struct {
    press:   proc(),
    release: proc(),
}

//_____________________________
bind_input :: proc {
    bind_input_keycallback,
    bind_input_press,
    bind_input_press_release,
}

bind_input_keycallback :: proc(key: Key, callbacks: KeyCallback) {
    if _, exists := input_callbacks[key]; !exists {
        input_callbacks[key] = {}
    }
    input_callbacks[key] = callbacks
}

bind_input_press :: proc(key: Key, press: proc()) {
    if _, exists := input_callbacks[key]; !exists {
        input_callbacks[key] = {}
    }
    input_callbacks[key] = KeyCallback { press = press }
}

bind_input_press_release :: proc(key: Key, press: proc(), release: proc()) {
    if _, exists := input_callbacks[key]; !exists {
        input_callbacks[key] = {}
    }
    input_callbacks[key] = KeyCallback { press = press, release = release }
}

//_____________________________
keyboard_callback :: proc(window: glfw.WindowHandle, key: i32, scancode: i32, action: i32, mods: i32) {
    if callback, exists := input_callbacks[key]; exists {
        if action == glfw.PRESS   { if callback.press != nil   { callback.press()   } }
        if action == glfw.RELEASE { if callback.release != nil { callback.release() } }
    }
}

//_____________________________
mouse_callback :: proc(window: glfw.WindowHandle, button: i32, action: i32, mods: i32) {
    if callback, exists := input_callbacks[button]; exists {
        if action == glfw.PRESS   { if callback.press != nil   { callback.press()   } }
        if action == glfw.RELEASE { if callback.release != nil { callback.release() } }
    }
}

//_____________________________
setup_standard_bindings :: proc() {
    bind_input(glfw.KEY_T,      press = proc() { __log("test") })
    bind_input(glfw.KEY_ESCAPE, press = proc() { glfw.SetWindowShouldClose(window, true) })
}

