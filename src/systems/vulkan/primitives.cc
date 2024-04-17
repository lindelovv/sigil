#include "vulkan.hh"

namespace sigil::renderer::primitives {

    Plane::Plane(u32 div, f32 width) {
        f32 triangle_side = width / div;
        for( u32 row = 0; row < div + 1; row++ ) {
            for( u32 col = 0; col < div + 1; col++ ) {
                auto pos = glm::vec3(col * triangle_side, row * -triangle_side, 0);
                vertices.push_back(
                    Vertex {
                        .position = pos,
                        .uv_x = (f32)col / div,
                        .uv_y = (f32)row / div,
                        .color = {
                            1.f,
                            1.f,
                            1.f,
                            1.f,
                        },
                    }
                );
            }
        }
        surface.start_index = (u32)indices.size();
        for( u32 row = 0; row < div; row++ ) {
            for( u32 col = 0; col < div; col++ ) {
                u32 index = row * (div + 1) + col;
                indices.push_back(index);
                indices.push_back(index + (div + 1) + 1);
                indices.push_back(index + (div + 1));
                indices.push_back(index);
                indices.push_back(index + 1);
                indices.push_back(index + (div + 1) + 1);
            }
        }
        surface.count = (u32)indices.size();
    }
    //rect_vertices = {
    //    Vertex {
    //        .position = { .5, -.5, 0 },
    //        .color    = { .5, .5, .5, 1 },
    //    },
    //    Vertex {
    //        .position = { .5, .5, 0 },
    //        .color    = { .5, .5, .5, 1 },
    //    },
    //    Vertex {
    //        .position = { -.5, -.5, 0 },
    //        .color    = { .5, .5, .5, 1 },
    //    },
    //    Vertex {
    //        .position = { -.5, .5, 0 },
    //        .color    = { .5, .5, .5, 1 },
    //    },
    //};
    //rect_indices = {
    //    0, 1, 2,
    //    2, 1, 3,
    //};

} // sigil::renderer::primitives

