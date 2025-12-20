package ism

import sigil "sigil:core"
import "vendor:glfw"
import glm "core:math/linalg/glsl"
import "core:fmt"
import "core:math"
import "lib:jolt"

scene :: proc(world: ^sigil.world_t, e: sigil.entity_t) -> typeid {
    using sigil
    add(world, e, name_t("scene_module"))
    add(world, e, init(init_scene))
    add(world, e, tick(tick_scene))
    scene_entity = e
    return none
}

scene_entity : sigil.entity_t
follow       : bool

init_scene :: proc(world: ^sigil.world_t) {
    setup_keybinds()
    scene := parse_gltf_scene(world, "res/models/scene.glb")

	box_shape := jolt.SphereShape_Create(0.8)//(&{ 1, 1, 1 }, jolt.DEFAULT_CONVEX_RADIUS)
	box_settings := jolt.BodyCreationSettings_Create3(
		cast(^jolt.Shape)box_shape,
		cast(^[3]f32)&{0,0,0},
		nil,
		.JPH_MotionType_Dynamic,
		OBJECT_LAYER_MOVING,
	)
	defer jolt.BodyCreationSettings_Destroy(box_settings)

	id := jolt.BodyInterface_CreateAndAddBody(body_interface, box_settings, .JPH_Activation_Activate)
    jolt.BodyInterface_SetObjectLayer(body_interface, id, OBJECT_LAYER_MOVING)
    jolt.BodyInterface_SetPosition(body_interface, id, cast(^[3]f32)sigil.get_ref(world, cam_entity, position_t), .JPH_Activation_Activate)
	sigil.add(world, cam_entity, physics_id_t(id))

	jolt.PhysicsSystem_OptimizeBroadPhase(physics_system)
}

get_ctrl :: #force_inline proc(world: ^sigil.world_t) -> ^camera_controller_t {
    return sigil.get_ref(world, cam_entity, camera_controller_t)
}

setup_keybinds :: proc() {
    bind_input(glfw.KEY_W,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.forward = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.forward = false },
    )
    bind_input(glfw.KEY_S,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.back = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.back = false },
    )
    bind_input(glfw.KEY_A,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.left = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.left = false },
    )
    bind_input(glfw.KEY_D,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.right = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.right = false },
    )
    bind_input(glfw.KEY_Q,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.down = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.down = false },
    )
    bind_input(glfw.KEY_E,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.up = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_movement.up = false },
    )
    bind_input(glfw.KEY_LEFT_SHIFT,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).movement_speed = 2 },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).movement_speed = 1 },
    )
    bind_input(glfw.KEY_H,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.left = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.left = false },
    )
    bind_input(glfw.KEY_J,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.down = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.down = false },
    )
    bind_input(glfw.KEY_K,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.up = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.up = false },
    )
    bind_input(glfw.KEY_L,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.right = true  },
        release = proc(world: ^sigil.world_t) { get_ctrl(world).requested_rotation.right = false },
    )
    bind_input(MOUSE_SCROLL_UP,
        press   = proc(world: ^sigil.world_t) { get_ctrl(world).movement_speed += 0.3 },
    )
    bind_input(MOUSE_SCROLL_DOWN,
        press   = proc(world: ^sigil.world_t) {
            speed := &get_ctrl(world).movement_speed
            speed^ = max(speed^ - 0.3, 0.1)
        },
    )
    bind_input(glfw.MOUSE_BUTTON_RIGHT,
        press = proc(world: ^sigil.world_t) {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_DISABLED)
            get_ctrl(world).follow_mouse = true
        },
        release = proc(world: ^sigil.world_t) {
            glfw.SetInputMode(window, glfw.CURSOR, glfw.CURSOR_NORMAL)
            get_ctrl(world).follow_mouse = false
        }
    )
    bind_input(glfw.KEY_V,//glfw.MOUSE_BUTTON_LEFT,
        press = proc(world: ^sigil.world_t) {
            npq := jolt.PhysicsSystem_GetNarrowPhaseQuery(physics_system)
            cam := sigil.get_value(world, cam_entity, camera_t)
            // this is fucked, tried fucking it more to fix it but no luck it seems
            forward := glm.normalize(cam.forward)
            origin := get_camera_pos(world)
            // todo: figure out why cam is being weird, likely mistake in calculations
            //es := parse_gltf_scene("res/models/DamagedHelmet.glb")
            ////model := glm.quatMulVec3(glm.dot_quat(glm.quat(1), forward), origin)
            //fmt.printfln("new e: %v", es)
            //for e in es {
            //    //sigil.remove_component(e, physics_id_t)
            //    fmt.printfln("point: %v", origin - forward)
            //    t, _ := sigil.add(e, transform_t(glm.mat4Translate(auto_cast (origin - forward) * glm.vec3(10))))
            //    // this scaling down at higher numbers would indicate that everything is in reverse lol hmmm
            //    fmt.printfln("spawn at: %v", t)
            //}
            //fmt.printfln("full list: %#v", sigil.query(render_data_t, transform_t))
            hit: jolt.RayCastResult
            filter := jolt.ObjectLayerFilter_Create(nil)
            dir := origin - forward
            if collided := jolt.NarrowPhaseQuery_CastRay(npq, &origin, &dir, &hit, nil, filter, nil); collided {
                fmt.println("cast hit")
                set := world.sets[physics_id_t]
                fmt.printfln("%v", set)
                owner: sigil.entity_t
                fmt.printfln("bodyID: %v", hit.bodyID)
                for id, i in sigil.query(world, physics_id_t) {
                    fmt.printfln("q id: %v", id)
                    fmt.printfln("q i: %v", i)
                    fmt.printfln("q idc: %v", set.indices[i+1])
                    if u32(id) == hit.bodyID {
                        for idx, e in set.indices {
                            if idx == u32(i) do owner = auto_cast e // todo: reverse lookup, feel like I did this sometime
                        }
                    } }
                fmt.printfln("owner: %v", owner)
                sigil.delete_entity(world, owner)
                //sigil.remove_component(owner, render_data_t)
                //sigil.remove_component(owner, transform_t)
                //sigil.remove_component(owner, physics_id_t)
            }
            else { fmt.println("cast miss") }
            //fmt.printfln("cam: %v", cam_entity)
        },
    )
    bind_input(glfw.KEY_M,
        press   = proc(world: ^sigil.world_t) { 
            fmt.printfln("%#v", sigil.query(world, position_t, render_data_t))
        },
    )
    bind_input(glfw.KEY_F,
        press   = proc(world: ^sigil.world_t) { follow = !follow  },
    )
    bind_input(glfw.KEY_B,
        press   = proc(world: ^sigil.world_t) {
            q1 := sigil.query(world, render_data_t)
            fmt.println("before remove: ")
            fmt.println(q1)

            //sigil.remove_component(n, render_data_t)
            sigil.delete_entity(world, n)

            q2 := sigil.query(world, render_data_t)
            fmt.println("after remove: ")
            fmt.println(q2)

            fmt.println()

            n += 1
        },
    )
    //bind_input(glfw.KEY_V,
    //    press   = proc() {
    //        sigil.delete_entity(auto_cast m)
    //        m += 1
    //    }
    //)
}
n := 9
m := 9

tick_scene :: proc(world: ^sigil.world_t) {
    update_camera(world, delta_time)

    wave := math.sin(3.0 * math.PI * 0.1 + time) * 0.001
    gpu_scene_data.sun_direction += glm.vec3 { wave, -wave, wave }

    if follow {
        target: [3]f32
        cam_id := sigil.get_value(world, cam_entity, physics_id_t)
	    jolt.BodyInterface_GetPosition(body_interface, u32(cam_id), &target)
        for &q in sigil.query(world, transform_t, physics_id_t) {
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

