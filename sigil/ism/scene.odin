
package ism

import sigil "sigil:core"
import "vendor:glfw"
import glm "core:math/linalg/glsl"
import "core:fmt"
import "core:math"

scene :: proc(e: sigil.entity_t) -> typeid {
    using sigil
    add(e, name("scene_module"))
    schedule(e, init(init_scene))
    schedule(e, tick(tick_scene))
    return none
}

init_scene :: proc() {
    init_camera()
    setup_keybinds()
}

get_ctrl :: #force_inline proc() -> ^camera_controller_t {
    return sigil.get_ref(cam_entity, camera_controller_t)
}

setup_keybinds :: proc() {
    bind_input(glfw.KEY_W,
        press   = proc() { get_ctrl().requested_movement.forward = true  },
        release = proc() { get_ctrl().requested_movement.forward = false },
    )
    bind_input(glfw.KEY_S,
        press   = proc() { get_ctrl().requested_movement.back = true  },
        release = proc() { get_ctrl().requested_movement.back = false },
    )
    bind_input(glfw.KEY_A,
        press   = proc() { get_ctrl().requested_movement.left = true  },
        release = proc() { get_ctrl().requested_movement.left = false },
    )
    bind_input(glfw.KEY_D,
        press   = proc() { get_ctrl().requested_movement.right = true  },
        release = proc() { get_ctrl().requested_movement.right = false },
    )
    bind_input(glfw.KEY_Q,
        press   = proc() { get_ctrl().requested_movement.down = true  },
        release = proc() { get_ctrl().requested_movement.down = false },
    )
    bind_input(glfw.KEY_E,
        press   = proc() { get_ctrl().requested_movement.up = true  },
        release = proc() { get_ctrl().requested_movement.up = false },
    )
    bind_input(glfw.KEY_LEFT_SHIFT,
        press   = proc() { get_ctrl().movement_speed = 2 },
        release = proc() { get_ctrl().movement_speed = 1 },
    )
    bind_input(glfw.KEY_H,
        press   = proc() { get_ctrl().requested_rotation.left = true  },
        release = proc() { get_ctrl().requested_rotation.left = false },
    )
    bind_input(glfw.KEY_J,
        press   = proc() { get_ctrl().requested_rotation.down = true  },
        release = proc() { get_ctrl().requested_rotation.down = false },
    )
    bind_input(glfw.KEY_K,
        press   = proc() { get_ctrl().requested_rotation.up = true  },
        release = proc() { get_ctrl().requested_rotation.up = false },
    )
    bind_input(glfw.KEY_L,
        press   = proc() { get_ctrl().requested_rotation.right = true  },
        release = proc() { get_ctrl().requested_rotation.right = false },
    )
    bind_input(glfw.MOUSE_BUTTON_RIGHT,
        press = proc() {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_DISABLED)
            get_ctrl().follow_mouse = true
        },
        release = proc() {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
            get_ctrl().follow_mouse = false
        }
    )
}

tick_scene :: proc() {
    update_camera(delta_time)

    wave := math.sin(3.0 * math.PI * 0.1 + time) * 0.001
    gpu_scene_data.sun_direction += glm.vec4 { wave, 0, 0, 0 }

    //for mesh in sigil.query(mesh) {
    //    mesh.surfaces[0].transform *= glm.mat4Translate(glm.vec3 { 0, 0, wave })
    //}
}

