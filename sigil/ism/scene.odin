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

follow : bool

init_scene :: proc() {
    setup_keybinds()

    //_____________________________
    // Upload Mesh
    //helmet := parse_gltf("res/models/DamagedHelmet.gltf")
    //for x in -2..=2 do for y in -2..=2 do for z in 2..=5 {
    //    //x, y, z := 3, 3, 3
    //    mesh_e := sigil.new_entity()
    //    r_data := upload_mesh(helmet[0].vertices, helmet[0].indices)
    //    sigil.add(mesh_e, position_t { f32(x) * 3, f32(y) * 3, f32(z) * 3 })
    //    sigil.add(mesh_e, transform_t(0))
    //    sigil.add(mesh_e, r_data)
    //}
    scene := parse_gltf_scene("res/models/scene.glb")

    //e_e := sigil.new_entity()
    //cube_data = upload_mesh(cube_vertices, cube_indices)
    //sigil.add(e_e, cube_data)
    //_, e_e_trans_idx := sigil.add(e_e, transform_t(glm.mat4Translate({ -1, -1, 1 })))

    //rect_e := sigil.new_entity()
    //rect_data := upload_mesh(rect_vertices, rect_indices)
    //sigil.add(rect_e, rect_data)
    //_, rect_e_trans_idx := sigil.add(rect_e, transform_t(glm.mat4Translate(glm.vec3 { 0, 0, 0 }) * glm.mat4Scale(glm.vec3(100))))

	//floor_shape := jolt.BoxShape_Create(&{10, 10, 0.5}, jolt.DEFAULT_CONVEX_RADIUS)

	//floor_settings := jolt.BodyCreationSettings_Create3(
	//	cast(^jolt.Shape)floor_shape,
	//	&{0, 0, -1},
	//	nil,
	//	.JPH_MotionType_Static,
	//	OBJECT_LAYER_NON_MOVING,
	//)
	//defer jolt.BodyCreationSettings_Destroy(floor_settings)

	//jolt.BodyCreationSettings_SetRestitution(floor_settings, 0.5)
	//jolt.BodyCreationSettings_SetFriction(floor_settings, 0.5)

	//floor_id = jolt.BodyInterface_CreateAndAddBody(body_interface, floor_settings, .JPH_Activation_DontActivate)

    //cube_e := sigil.new_entity()
    //c_data := upload_mesh(cube_vertices, cube_indices)
    //sigil.add(cube_e, c_data)
    //sigil.add(cube_e, transform_t(0))
	//pos, _ := sigil.add(cube_e, position_t { 4, 4, 4 })
	//sigil.add(cube_e, rotation_t(0))
	//sigil.add(cube_e, velocity_t(0))
	//sigil.add(cube_e, sigil.name("phys cube"))

	box_shape := jolt.SphereShape_Create(0.8)//(&{ 1, 1, 1 }, jolt.DEFAULT_CONVEX_RADIUS)
	box_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)box_shape,
		cast(^[3]f32)&{0,0,0},
		nil,
		.JPH_MotionType_Dynamic,
		OBJECT_LAYER_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(box_settings)

    //e : sigil.entity_t = 9
    //for x in -2..=2 do for y in -2..=2 do for z in 3..=6 {    
    //    //x, y, z := 0, 0, 5
	//    id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
    //    jolt.BodyInterface_SetPosition(body_interface, id, &{ f32(x) * 3, f32(y) * 3, f32(z) * 3}, .JPH_Activation_Activate)
	//    sigil.add(e, physics_id_t(id))
	//    sigil.add(e, position_t(0))
	//    sigil.add(e, rotation_t(0))
    //    e += 1
    //}
	id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
    jolt.BodyInterface_SetPosition(body_interface, id, cast(^[3]f32)sigil.get_ref(cam_entity, position_t), .JPH_Activation_Activate)
	//sigil.add(cam_entity, rotation_t(0))
	sigil.add(cam_entity, physics_id_t(id))

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

    bind_input(glfw.KEY_F,
        press   = proc() { follow = !follow  },
    )

    bind_input(glfw.KEY_B,
        press   = proc() {
            //fmt.println(sigil.get_component_slice(&sigil.core.groups[sigil.types_hash(render_data_t, transform_t)], transform_t))
            //fmt.println(sigil.core.groups[sigil.types_hash(render_data_t, transform_t)])
            sigil.remove_component(auto_cast n, render_data_t)
            n += 1
            //fmt.println()
            //g := &sigil.core.groups[sigil.types_hash(render_data_t, transform_t)]
            //fmt.println(g)
            //fmt.println()
            //fmt.println(sigil.get_component_slice(&sigil.core.groups[sigil.types_hash(render_data_t, transform_t)], transform_t))
            //fmt.println()
            //slice1 := sigil.get_component_slice(g, render_data_t)
            //slice2 := sigil.get_component_slice(g, transform_t)
            //fmt.printfln("=-== %#v", slice1)
            //fmt.printfln("mmmmmm %#v", slice2)
        },
    )
}
n := 9

tick_scene :: proc() {
    update_camera(delta_time)

    wave := math.sin(3.0 * math.PI * 0.1 + time) * 0.001
    gpu_scene_data.sun_direction += glm.vec3 { wave, -wave, wave }

    if follow {
        target: [3]f32
        cam_id := sigil.get_value(cam_entity, physics_id_t)
	    jolt.BodyInterface_GetPosition(body_interface, u32(cam_id), &target)
        for &q in sigil.query(transform_t, physics_id_t) {
            transform, id := &q.x, u32(q.y)

            pos: [3]f32
	        jolt.BodyInterface_GetPosition(body_interface, id, &pos)
            z := pos.z
            movement := move_towards(pos, target, 0.01)
            //movement.z = pos.z
	    	jolt.BodyInterface_SetPosition(body_interface, id, &movement, .JPH_Activation_Activate)

            src_rot: glm.quat
            jolt.BodyInterface_GetRotation(body_interface, id, &src_rot)
            target_dir := glm.normalize(target - pos)
            rot := rotate_towards(src_rot, target_dir, glm.radians(f32(1.0)))
            //offset_rot := glm.quatAxisAngle(glm.normalize(glm.vec3{0, 1, 0}), 90.0)
            //rot = rot * offset_rot
            jolt.BodyInterface_SetRotation(body_interface, id, &rot, .JPH_Activation_Activate)

	    	//jolt.BodyInterface_SetLinearAndAngularVelocity(body_interface, id, &target_dir, &target_dir)
        }
    }
}

move_towards :: proc(src, dst: glm.vec3, mdd: f32) -> glm.vec3 {
    x, y, z: f32 = dst.x - src.x, dst.y - src.y, dst.z - src.z
    sqdist: f32 = x*x + y*y + z*z
    if sqdist == 0 || (mdd >= 0 && sqdist <= mdd*mdd) do return dst
    dist: f32 = math.sqrt(sqdist)
    return glm.vec3 { src.x + x / dist * mdd, src.y + y / dist * mdd, src.z + z / dist * mdd }
}

rotate_towards :: proc(q: glm.quat, target: glm.vec3, angle_step: f32) -> glm.quat {
    forward := glm.quatMulVec3(q, target)
    axis := glm.cross(forward, target)
    angle := glm.acos(glm.dot(forward, glm.normalize(target)))

    if (glm.length(axis) > 0.0001) {
        axis = glm.normalize(axis);
        inc := glm.quatAxisAngle(axis, glm.min(angle_step, angle))
        return inc * q
    } else {
        return q
    }
}

