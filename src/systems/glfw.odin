package systems
import "core:fmt"
import "vendor:glfw"
import gl "vendor:OpenGL"

WIDTH  :: 1920
HEIGHT :: 1080
TITLE  :: "sigil"

glfw_init :: proc() {
    if !bool(glfw.Init()) { fmt.eprintln("GLFW Init failed"); return }
    window := glfw.CreateWindow(WIDTH, HEIGHT, TITLE, nil, nil)

    defer glfw.Terminate()
    defer glfw.DestroyWindow(window)

    if window == nil { fmt.eprintln("GLFW CreateWindow failed"); return }

    glfw.MakeContextCurrent(window)
    gl.load_up_to(4, 5, glfw.gl_set_proc_address)
    
    for !glfw.WindowShouldClose(window) {
        glfw.PollEvents()
        gl.ClearColor(0.5, 0.0, 1.0, 1.0)
        gl.Clear(gl.COLOR_BUFFER_BIT)
        glfw.SwapBuffers(window)
    }
}

glfw_run :: proc() {
    
}
