
package ism

import sigil "sigil:core"
import "vendor:glfw"
import glm "core:math/linalg/glsl"
import "core:fmt"
import "core:math"

scene :: proc() {
    using sigil
    schedule(init(init_scene))
    schedule(tick(tick_scene))
}

controller  := camera_controller {
    movement_speed    = 1,
    mouse_sensitivity = 28,
}

init_scene :: proc() {
    init_camera()
    setup_keybinds()
}

setup_keybinds :: proc() {
    bind_input(glfw.KEY_W,
        press   = proc() { controller.requested_movement.forward = true  },
        release = proc() { controller.requested_movement.forward = false },
    )
    bind_input(glfw.KEY_S,
        press   = proc() { controller.requested_movement.back = true  },
        release = proc() { controller.requested_movement.back = false },
    )
    bind_input(glfw.KEY_A,
        press   = proc() { controller.requested_movement.left = true  },
        release = proc() { controller.requested_movement.left = false },
    )
    bind_input(glfw.KEY_D,
        press   = proc() { controller.requested_movement.right = true  },
        release = proc() { controller.requested_movement.right = false },
    )
    bind_input(glfw.KEY_Q,
        press   = proc() { controller.requested_movement.down = true  },
        release = proc() { controller.requested_movement.down = false },
    )
    bind_input(glfw.KEY_E,
        press   = proc() { controller.requested_movement.up = true  },
        release = proc() { controller.requested_movement.up = false },
    )
    bind_input(glfw.KEY_LEFT_SHIFT,
        press   = proc() { controller.movement_speed = 2 },
        release = proc() { controller.movement_speed = 1 },
    )
    bind_input(glfw.KEY_H,
        press   = proc() { controller.requested_rotation.left = true  },
        release = proc() { controller.requested_rotation.left = false },
    )
    bind_input(glfw.KEY_J,
        press   = proc() { controller.requested_rotation.down = true  },
        release = proc() { controller.requested_rotation.down = false },
    )
    bind_input(glfw.KEY_K,
        press   = proc() { controller.requested_rotation.up = true  },
        release = proc() { controller.requested_rotation.up = false },
    )
    bind_input(glfw.KEY_L,
        press   = proc() { controller.requested_rotation.right = true  },
        release = proc() { controller.requested_rotation.right = false },
    )
    bind_input(glfw.MOUSE_BUTTON_RIGHT,
        press = proc() {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_DISABLED)
            controller.follow_mouse = true
        },
        release = proc() {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
            controller.follow_mouse = false
        }
    )
}

tick_scene :: proc() {
    update_camera(delta_time)

    //for mesh in sigil.query(Mesh) {
    //    for &data in mesh.surfaces {
    //        data.pos -= glm.vec3 { 0, 0, 0.0001 }
    //    }
    //}
    wave := math.sin(3.0 * math.PI * 0.1 + time) * 0.001
    gpu_scene_data.sun_direction += glm.vec4 { wave, 0, 0, 0 }
}

