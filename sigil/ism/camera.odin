
package ism

import sigil "../core"
import glm "core:math/linalg/glsl"
import "core:math/linalg"
import "core:math"
import "core:fmt"

WORLD_UP :: glm.vec3 { 0, 0, 1 }

RequestMovement :: struct {
    forward : bool,
    right   : bool,
    back    : bool,
    left    : bool,
    up      : bool,
    down    : bool,
}
RequestRotation :: struct {
    up      : bool,
    right   : bool,
    down    : bool,
    left    : bool,
}          

camera_controller :: struct {
    requested_movement : RequestMovement,
    requested_rotation : RequestRotation,
    movement_speed     : f32,
    mouse_sensitivity  : f32,
    follow_mouse       : bool,
}

transform :: glm.mat4
position :: distinct [3]f32
velocity :: glm.vec3
camera :: struct {
    yaw       : f32,
    pitch     : f32,
    roll      : f32,
    fov       : f32,
    velocity  : glm.vec3,
    forward   : glm.vec3,
    right     : glm.vec3,
    up        : glm.vec3,
    near      : f32,
    far       : f32,
}
cam_entity: sigil.entity_t

init_camera :: proc() {
    cam_entity = sigil.new_entity()

    cam := sigil.add_component(cam_entity, camera {
        fov     = 70.0,
        up      = WORLD_UP,
        near    = 0.1,
        far     = 1000000,
        pitch   = 0,
        yaw     = 140,
        roll    = 0,
    })
    sigil.add_component(cam_entity, position(glm.vec3{ -2.5, 2.5, 1.5 }))
    sigil.add_component(cam_entity, glm.vec3{ -2.5, 2.5, 1.5 })
    sigil.add_component(cam_entity, &controller)

    update_camera_vectors(&cam)
}

update_camera :: proc(delta_time: f32) {
    for &sys in sigil.query(camera, position, ^camera_controller) {
        cam := &sys._0
        pos := &sys._1
        ctrl := sys._2

        if ctrl.requested_movement.forward do cam.velocity -= cam.forward
        if ctrl.requested_movement.back    do cam.velocity += cam.forward
        if ctrl.requested_movement.right   do cam.velocity += cam.right
        if ctrl.requested_movement.left    do cam.velocity -= cam.right
        if ctrl.requested_movement.up      do cam.velocity += WORLD_UP
        if ctrl.requested_movement.down    do cam.velocity -= WORLD_UP

        if ctrl.requested_rotation.right do cam.yaw   += ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.left  do cam.yaw   -= ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.up    do cam.pitch -= ctrl.mouse_sensitivity * delta_time
        if ctrl.requested_rotation.down  do cam.pitch += ctrl.mouse_sensitivity * delta_time

        pos^ += (cam.velocity * ctrl.movement_speed * delta_time).xyz
        cam.velocity = glm.vec3 { 0, 0, 0 }

        if ctrl.follow_mouse {
            offset := (get_mouse_movement() * ctrl.mouse_sensitivity) * delta_time
            cam.yaw += offset.x
            cam.pitch = cam.pitch + offset.y
        }
        cam.pitch = glm.clamp(cam.pitch, -89, 89)
        update_camera_vectors(cam)
        cube_mesh.surfaces[0].transform = glm.mat4Translate(pos^.xyz + -(cam.forward * 4) - glm.vec3 { 0, 0, -1 })
    }
}

update_camera_vectors :: proc(cam: ^camera) {
    yaw := glm.radians(cam.yaw)
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

get_camera_view :: #force_inline proc() -> glm.mat4 {
    cam, position := expand_values(sigil.query(camera, position)[0])

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

get_camera_projection :: #force_inline proc() -> glm.mat4 {
    cam := sigil.query(camera)[0]

    a := f32(swap_extent.width) / f32(swap_extent.height)
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

get_camera_pos :: #force_inline proc() -> glm.vec3 {
    return (glm.vec3)(sigil.query(camera, position)[0]._1)
}

