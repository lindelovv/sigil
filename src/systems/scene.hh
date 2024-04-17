#pragma once

#include "sigil.hh"
#include "vulkan.hh"
#include "glfw.hh"

namespace sigil::scene {

    inline void setup_keybinds() {

        input::bind(GLFW_KEY_W,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.forward = 1; },
                .release = [&]{ renderer::camera.request_movement.forward = 0; }
        }   );
        input::bind(GLFW_KEY_S,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.back = 1; },
                .release = [&]{ renderer::camera.request_movement.back = 0; }
        }   );
        input::bind(GLFW_KEY_A,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.left = 1; },
                .release = [&]{ renderer::camera.request_movement.left = 0; }
        }   );
        input::bind(GLFW_KEY_D,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.right = 1; },
                .release = [&]{ renderer::camera.request_movement.right = 0; }
        }   );
        input::bind(GLFW_KEY_Q,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.down = 1; },
                .release = [&]{ renderer::camera.request_movement.down = 0; }
        }   );
        input::bind(GLFW_KEY_E,
            key_callback {
                .press   = [&]{ renderer::camera.request_movement.up = 1; },
                .release = [&]{ renderer::camera.request_movement.up = 0; }
        }   );
        input::bind(GLFW_KEY_LEFT_SHIFT,
            key_callback {
                .press   = [&]{ renderer::camera.movement_speed = 2.f; },
                .release = [&]{ renderer::camera.movement_speed = 1.f; }
        }   );
        input::bind(GLFW_MOUSE_BUTTON_2,
            key_callback {
                .press   = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    renderer::camera.follow_mouse = true;
                },
                .release = [&]{
                    glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    renderer::camera.follow_mouse = false;
                }
        }   );
    }

    inline void init() {
        setup_keybinds();
    }
    


    inline void tick() {

    }

    inline void terminate() {

    }
} // sigil::scene

struct scene {
    scene() {
        using namespace sigil;
        schedule(eInit, sigil::scene::init);
        schedule(eTick, sigil::scene::tick);
        schedule(eExit, sigil::scene::terminate);
    }
};
