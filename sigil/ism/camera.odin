package ism

import sigil "../core"
import glm "core:math/linalg/glsl"
import "lib:jolt"

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

WORLD_UP :: glm.vec3 { 0, 0, 1 }

request_movement_t :: struct {
    forward : bool,
    right   : bool,
    back    : bool,
    left    : bool,
    up      : bool,
    down    : bool,
}
request_rotation_t :: struct {
    up      : bool,
    right   : bool,
    down    : bool,
    left    : bool,
}

camera_controller_t :: struct {
    requested_movement : request_movement_t,
    requested_rotation : request_rotation_t,
    movement_speed     : f32,
    mouse_sensitivity  : f32,
    follow_mouse       : bool,
}

camera_t :: struct {
    yaw       : f32,
    pitch     : f32,
    roll      : f32,
    fov       : f32,
    forward   : glm.vec3,
    right     : glm.vec3,
    up        : glm.vec3,
    near      : f32,
    far       : f32,
}

cam_entity: sigil.entity_t
cam_cube_e: sigil.entity_t

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

init_camera :: proc(world: ^sigil.world_t) {
    cam_entity = sigil.new_entity(world)

    cam, _ := sigil.add_component(world, cam_entity, camera_t {
        fov     = 70.0,
        up      = WORLD_UP,
        near    = 0.1,
        far     = 1000000,
        pitch   = 20,
        yaw     = 225,
        roll    = 0,
    })
    sigil.add_component(world, cam_entity, position_t(glm.vec3{ -11, -11, 13 }))
    sigil.add_component(world, cam_entity, rotation_t(0))
    sigil.add_component(world, cam_entity, velocity_t(0))
    sigil.add_component(world, cam_entity, sigil.name_t("cam"))
    sigil.add_component(world, cam_entity, camera_controller_t {
        movement_speed    = 6,
        mouse_sensitivity = 28,
    })

    //cam_cube_e = sigil.new_entity()
    //cube_mesh: mesh_t
    //cube_data = render_data_t {
    //    first     = 0,
    //    count     = u32(len(cube_indices)),
    //    material  = &pbr,
    //}
    //append(&cube_mesh.surfaces, cube_data)
    //upload_mesh(cube_vertices, cube_indices, &cube_mesh)
    //sigil.add(cam_cube_e, cube_mesh.surfaces[0]) 
    //sigil.add(cam_cube_e, position_t(0)) 
    //sigil.add(cam_cube_e, rotation_t(0)) 

    update_camera_vectors(&cam)
}

update_camera :: proc(world: ^sigil.world_t, delta_time: f32) {
    for &q in sigil.query(world, camera_t, position_t, camera_controller_t, physics_id_t) {
        cam, pos, ctrl, id := &q.x, &q.y, &q.z, u32(q.w)

        velocity := glm.vec3 { 0, 0, 0 }
        if ctrl.requested_movement.forward do velocity -= cam.forward
        if ctrl.requested_movement.back    do velocity += cam.forward
        if ctrl.requested_movement.right   do velocity += cam.right
        if ctrl.requested_movement.left    do velocity -= cam.right
        if ctrl.requested_movement.up      do velocity += WORLD_UP
        if ctrl.requested_movement.down    do velocity -= WORLD_UP

        if ctrl.requested_rotation.right do cam.yaw   += ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.left  do cam.yaw   -= ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.up    do cam.pitch -= ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.down  do cam.pitch += ctrl.mouse_sensitivity * delta_time

        //pos^ += (velocity * ctrl.movement_speed * delta_time).xyz
        velocity *= ctrl.movement_speed
		jolt.BodyInterface_SetLinearVelocity(body_interface, id, &velocity)
		jolt.BodyInterface_GetPosition(body_interface, id, cast(^[3]f32)pos)

        if ctrl.follow_mouse {
            offset := (get_mouse_movement() * ctrl.mouse_sensitivity) * delta_time
            cam.yaw += offset.x
            cam.pitch = cam.pitch + offset.y
        }
        cam.pitch = glm.clamp(cam.pitch, -89, 89)
        update_camera_vectors(cam)
        //sigil.get_ref(cam_cube_e, render_data_t).gpu_data.model = glm.mat4Translate(pos^.xyz + -(cam.forward * 4) - glm.vec3 { 0, 0, -1 })
    }
	//for &q in sigil.query(velocity_t, jolt.BodyID) {
	//	vel, id := &q.x, q.y
    //    // this just makes gravity take effect when not controlling,
    //    // which could be fun for like a drone control or something lol
    //    //if math.round_f32(vel.x) == 0 && math.round_f32(vel.y) == 0 && math.round_f32(vel.z) == 0.0 do continue
    //    jolt.BodyInterface_SetLinearVelocity(body_interface, id, vel)
    //    vel^ -= vel^ / 10
    //}
}

update_camera_vectors :: proc(cam: ^camera_t) {
    yaw   := glm.radians(cam.yaw)
    pitch := glm.radians(cam.pitch)
    cam.forward = {
        glm.cos(yaw) * glm.cos(pitch),
        glm.sin(yaw) * glm.cos(pitch),
        glm.sin(pitch),
    }
    cam.forward = glm.normalize(cam.forward)
    cam.right = glm.normalize(glm.cross(cam.forward, WORLD_UP))
    cam.up = glm.normalize(glm.cross(cam.right, cam.forward))
}

get_camera_view :: #force_inline proc(world: ^sigil.world_t) -> glm.mat4 {
    cam      := sigil.get_ref(world, cam_entity, camera_t)^
    position := sigil.get_ref(world, cam_entity, position_t)^
    //fmt.println(position)

    eye    := glm.vec3(position)
    centre := (glm.vec3(position) + cam.forward)
    f := glm.normalize(centre - eye)
    r := glm.normalize(glm.cross(f, cam.up))
    u := glm.normalize(glm.cross(r, f))

    return glm.mat4 {
         r.x,  r.y,  r.z, -glm.dot(r, eye),
         u.x,  u.y,  u.z, -glm.dot(u, eye),
        -f.x, -f.y, -f.z,  glm.dot(f, eye),
         0,    0,    0,    1,
    }
}

get_camera_projection :: #force_inline proc(world: ^sigil.world_t) -> glm.mat4 {
    cam := sigil.get_ref(world, cam_entity, camera_t)

    a := f32(swapchain.extent.width) / f32(swapchain.extent.height)
    h := glm.tan(glm.radians(cam.fov) * 0.5)
    f := cam.far
    n := cam.near

    return glm.mat4 {
        1/(a*h), 0,     0,        0,
        0,       -1/h,  0,        0,
        0,       0,     f/(f-n), -(f*n)/(f-n),
        0,       0,    1,         0,
    }
}

get_camera_pos :: #force_inline proc(world: ^sigil.world_t) -> glm.vec3 {
    return (glm.vec3)(sigil.get_value(world, cam_entity, position_t))
}

