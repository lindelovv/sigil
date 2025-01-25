
package ism

import glm "core:math/linalg/glsl"
import "core:math/linalg"
import "core:math"

WORLD_UP :: glm.vec3 { 0, 0, 1 }

Rotation :: struct {
    yaw   : f32,
    pitch : f32,
    roll  : f32,
}

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

CameraController :: struct {
    requested_movement : RequestMovement,
    requested_rotation : RequestRotation,
    movement_speed     : f32,
    mouse_sensitivity  : f32,
    follow_mouse       : bool,
}

Camera :: struct {
    position : glm.vec3,
    rotation : Rotation,
    fov      : f32,
    velocity : glm.vec3,
    forward  : glm.vec3,
    right    : glm.vec3,
    up       : glm.vec3,
    near     : f32,
    far      : f32,
}

init_camera :: proc(cam: ^Camera) {
    cam.forward = get_forward_vector(cam)
    cam.right   = get_right_vector(cam)
    cam.up      = get_up_vector(cam)
}

update_camera :: proc(delta_time: f32, controller: ^CameraController, cam: ^Camera) {
    if controller.requested_movement.forward do cam.velocity += cam.forward
    if controller.requested_movement.back    do cam.velocity -= cam.forward
    if controller.requested_movement.right   do cam.velocity += cam.right
    if controller.requested_movement.left    do cam.velocity -= cam.right
    if controller.requested_movement.up      do cam.velocity -= WORLD_UP
    if controller.requested_movement.down    do cam.velocity += WORLD_UP

    if controller.requested_rotation.right do cam.rotation.yaw   -= 20 * controller.movement_speed * 1.6 * delta_time;
    if controller.requested_rotation.left  do cam.rotation.yaw   += 20 * controller.movement_speed * 1.6 * delta_time;
    if controller.requested_rotation.up    do cam.rotation.pitch -= 20 * controller.movement_speed * 1.6 * delta_time;
    if controller.requested_rotation.down  do cam.rotation.pitch += 20 * controller.movement_speed * 1.6 * delta_time;

    cam.position += cam.velocity * controller.movement_speed * delta_time
    cam.velocity = glm.vec3 { 0, 0, 0 }

    if controller.follow_mouse {
        offset := -get_mouse_movement() * controller.mouse_sensitivity * delta_time
        cam.rotation.yaw += offset.x
        new_pitch := cam.rotation.pitch + -offset.y
        if new_pitch > 89 do cam.rotation.pitch = 89
        else if new_pitch < -89 do cam.rotation.pitch = -89
        else do cam.rotation.pitch = new_pitch
    }
    cam.forward = get_forward_vector(cam)
    cam.right   = get_right_vector(cam)
    cam.up      = get_up_vector(cam)
}

// 1.0f, 0.0f, 0.0f, 0.0f,
// 0.0f, 1.0f, 0.0f, 0.0f,
// 0.0f, 0.0f, 1.0f, 0.0f,
// 0.0f, 0.0f, 0.0f, 1.0f

// t.x.x = a; t.x.y = b; t.x.z = c; t.x.w = d;
// t.y.x = e; t.y.y = f; t.y.z = g; t.y.w = h;
// t.z.x = i; t.z.y = j; t.z.z = k; t.z.w = l;
// t.w.x = m; t.w.y = n; t.w.z = o; t.w.w = p;

get_forward_vector :: #force_inline proc(cam: ^Camera) -> glm.vec3 {
    return glm.normalize(glm.vec3 {
        math.cos(glm.radians(cam.rotation.yaw)) * math.cos(glm.radians(cam.rotation.pitch)),
        math.sin(glm.radians(cam.rotation.yaw)) * math.cos(glm.radians(cam.rotation.pitch)),
        math.sin(glm.radians(cam.rotation.pitch)),
    })
}

get_right_vector :: #force_inline proc(cam: ^Camera) -> glm.vec3 {
    return glm.normalize(glm.cross(cam.forward, WORLD_UP))
}

get_up_vector :: #force_inline proc(cam: ^Camera) -> glm.vec3 {
    return glm.normalize(glm.cross(cam.forward, -cam.right))
}

get_view :: #force_inline proc(cam: Camera) -> glm.mat4 {
    return glm.mat4LookAt(cam.position, (cam.position + cam.forward), cam.up)
}

