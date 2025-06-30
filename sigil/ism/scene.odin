
package ism

import sigil "sigil:core"
import "vendor:glfw"
import glm "core:math/linalg/glsl"
import "core:fmt"
import "core:math"
import "lib:jolt"

scene :: proc(e: sigil.entity_t) -> typeid {
    using sigil
    add(e, name_t("scene_module"))
    schedule(e, init(init_scene))
    schedule(e, tick(tick_scene))
    return none
}

init_scene :: proc() {
    setup_keybinds()

    //_____________________________
    // Upload Mesh
    vertices, indices := parse_gltf("res/models/DamagedHelmet.gltf")
    for x in -2..=2 do for y in -2..=2 do for z in 2..=5 {
        //x, y, z := 3, 3, 3
        m: mesh_t
        upload_mesh(vertices, indices, &m)
        mesh_e := sigil.new_entity()
        //sigil.add(mesh_e, m)
        sigil.add(mesh_e, position_t { f32(x) * 3, f32(y) * 3, f32(z) * 3 })
        //sigil.add(mesh_e, rotation_t(0))
        sigil.add(mesh_e, m.surfaces[0])
        //for &s in m.surfaces {
        //    s.gpu_data.model = glm.mat4Translate({ f32(x) * 3, f32(y) * 3, f32(z) * 3 })
        //    sigil.add(mesh_e, s)
        //}
        //fmt.println(mesh_e)
    }

    e_e := sigil.new_entity()
    cube_data = render_data_t {
        first     = 0,
        count     = u32(len(cube_indices)),
        material  = &pbr,
        gpu_data  = {
            model = glm.mat4Translate({ -1, -1, 1 })
        }
    }
    append(&cube_mesh.surfaces, cube_data)
    upload_mesh(cube_vertices, cube_indices, &cube_mesh)
    sigil.add(e_e, cube_mesh.surfaces[0])

    rect_e := sigil.new_entity()
    rect_data := render_data_t {
        first     = 0,
        count     = u32(len(rect_indices)),
        material  = &pbr,
        gpu_data  = {
            model = glm.mat4Translate(glm.vec3 { 0, 0, 0 }) * glm.mat4Scale(glm.vec3(100)),
        }
    }
    append(&rect_mesh.surfaces, rect_data)
    upload_mesh(rect_vertices, rect_indices, &rect_mesh)
    sigil.add(rect_e, rect_mesh.surfaces[0])

	floor_shape := jolt.BoxShape_Create(&{50, 50, 0.5}, jolt.DEFAULT_CONVEX_RADIUS)

	floor_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)floor_shape,
		&{0, 0, 0},
		nil,
		.JPH_MotionType_Static,
		OBJECT_LAYER_NON_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(floor_settings)

	jolt.BodyCreationSettings_SetRestitution(floor_settings, 0.5)
	jolt.BodyCreationSettings_SetFriction(floor_settings, 0.5)

	floor_id = jolt.BodyInterface_CreateAndAddBody(body_interface, floor_settings, .JPH_Activation_DontActivate)

    //cube_mesh: mesh_t
    //cube_data = render_data_t {
    //    first     = 0,
    //    count     = u32(len(cube_indices)),
    //    material  = &pbr,
    //}
    //append(&cube_mesh.surfaces, cube_data)
    //upload_mesh(cube_vertices, cube_indices, &cube_mesh)
    //for &s in cube_mesh.surfaces do sigil.add(box_e, s)

    cube_e := sigil.new_entity()
    c_data := render_data_t {
        first     = 0,
        count     = u32(len(cube_indices)),
        material  = &pbr,
        gpu_data  = {
            model = glm.mat4Translate(glm.vec3 { 4, 4, 2 }),
        }
    }
    m: mesh_t
    append(&m.surfaces, cube_data)
    upload_mesh(cube_vertices, cube_indices, &m)
    sigil.add(cube_e, m.surfaces[0])

	pos := sigil.add(cube_e, position_t { 4, 4, 4 })
	//sigil.add(cube_e, rotation_t(0))
	//sigil.add(cube_e, velocity_t(0))
	//sigil.add(cube_e, sigil.name("phys cube"))

	box_shape := jolt.SphereShape_Create(0.8)//(&{ 1, 1, 1 }, jolt.DEFAULT_CONVEX_RADIUS)
	box_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)box_shape,
		cast(^[3]f32)&pos,
		nil,
		.JPH_MotionType_Dynamic,
		OBJECT_LAYER_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(box_settings)

    e : sigil.entity_t = 10
    for x in -2..=2 do for y in -2..=2 do for z in 3..=6 {    
        //x, y, z := 0, 0, 5
	    id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
        jolt.BodyInterface_SetPosition(body_interface, id, &{ f32(x) * 3, f32(y) * 3, f32(z) * 3}, .JPH_Activation_Activate)
	    sigil.add(e, physics_id_t(id))
	    sigil.add(e, position_t(0))
	    sigil.add(e, rotation_t(0))
        e += 1
    }
	id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
    jolt.BodyInterface_SetPosition(body_interface, id, cast(^[3]f32)sigil.get_ref(cam_entity, position_t), .JPH_Activation_Activate)
	sigil.add(cam_entity, rotation_t(0))
	sigil.add(cam_entity, physics_id_t(id))
    //fmt.println(cam_entity)

	jolt.PhysicsSystem_OptimizeBroadPhase(physics_system)
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
    bind_input(MOUSE_SCROLL_UP,
        press   = proc() { get_ctrl().movement_speed += 0.3 },
    )
    bind_input(MOUSE_SCROLL_DOWN,
        press   = proc() {
            speed := &get_ctrl().movement_speed
            speed^ = max(speed^ - 0.3, 0.1)
        },
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
    bind_input(glfw.KEY_M,
        press   = proc() { 
            fmt.printfln("%#v", sigil.query(position_t, render_data_t))
        },
    )
}

tick_scene :: proc() {
    update_camera(delta_time)

    wave := math.sin(3.0 * math.PI * 0.1 + time) * 0.001
    gpu_scene_data.sun_direction += glm.vec3 { wave, -wave, wave }
}

