
package ism

import vk "vendor:vulkan"
import glm "core:math/linalg/glsl"
import "core:slice"

mesh :: struct {
    surfaces : [dynamic]render_data,
    index    : AllocatedBuffer,
    vertex   : AllocatedBuffer,
    address  : vk.DeviceAddress,
}

render_data :: struct {
    count       : u32,
    first       : u32,
    idx_buffer  : vk.Buffer,
    material    : ^material_t,
    transform   : glm.mat4,
    address     : vk.DeviceAddress,
}

cube_data : render_data
cube_mesh : mesh
rect_mesh : mesh

// rect
rect_vertices := []Vertex {
    { position = {  0.5, -0.5, 0 }, color = { 0, 0, 1, 1 } },
    { position = {  0.5,  0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, 0 }, color = { 1, 0, 0, 1 } },
    { position = { -0.5,  0.5, 0 }, color = { 0, 1, 0, 1 } },
}
rect_indices := []u32 { 3, 1, 2, 2, 1, 0 }

// cube
cube_vertices := []Vertex {
    { position = {  0.5, -0.5, 0 },  color = { 0, 0, 1, 1 } }, { position = {  0.5,  0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, 0 },  color = { 1, 0, 0, 1 } }, { position = { -0.5,  0.5, 0 }, color = { 0, 1, 0, 1 } },

    { position = {  0.5, -0.5, -1 }, color = { 0, 0, 1, 1 } }, { position = {  0.5,  0.5, -1 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, -1 }, color = { 1, 0, 0, 1 } }, { position = { -0.5,  0.5, -1 }, color = { 0, 1, 0, 1 } },

    { position = { 0.5, -0.5, 0 },   color = { 0, 0, 1, 1 } }, { position = { 0.5,  0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { 0.5, -0.5, -1 },  color = { 1, 0, 0, 1 } }, { position = { 0.5,  0.5, -1 }, color = { 0, 1, 0, 1 } },

    { position = { -0.5, -0.5, 0 },  color = { 0, 0, 1, 1 } }, { position = { -0.5, 0.5, 0 }, color = { 1, 0, 1, 1 } },
    { position = { -0.5, -0.5, -1 }, color = { 1, 0, 0, 1 } }, { position = { -0.5, 0.5, -1 }, color = { 0, 1, 0, 1 } },

    { position = { -0.5, 0.5, -1 },  color = { 1, 0, 0, 1 } }, { position = { 0.5, 0.5, -1 }, color = { 0, 1, 0, 1 } },
    { position = { -0.5, 0.5, 0 },   color = { 1, 0, 0, 1 } }, { position = { 0.5, 0.5, 0 }, color = { 0, 1, 0, 1 } },

    { position = { 0.5, -0.5, 0 },   color = { 1, 0, 0, 1 } }, { position = { -0.5, -0.5, 0 }, color = { 0, 1, 0, 1 } },
    { position = { 0.5, -0.5, -1 },  color = { 1, 0, 0, 1 } }, { position = { -0.5, -0.5, -1 }, color = { 0, 1, 0, 1 } },
}
cube_indices := []u32 {
    1,  0,      2,  2,      3,  1,
    5,  7,      6,  6,      4,  5,
    10, 8,      9,  9,      11, 10,
    14, 15,     13, 13,     12, 14,
    17, 19,     18, 18,     16, 17,
    21, 20,     22, 22,     23, 21,
}

