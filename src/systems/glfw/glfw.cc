#include "glfw.hh"
#include "renderer.hh"
#include <GLFW/glfw3.h>
#include <vector>

namespace sigil {

    std::unordered_map<KeyInfo, std::function<void()>>  Glfw::callbacks;

    void Glfw::init() {
        can_tick = true;

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetFramebufferSizeCallback(window, Renderer::resize_callback);

        setup_standard_input_bindings();
        glfwSetKeyCallback(window, callback);
    }

    void Glfw::terminate() {
        glfwDestroyWindow(window);
        glfwTerminate();
    };

    void Glfw::tick() {
        if( glfwWindowShouldClose(window) ) {
            core->should_exit = true;
        }
        glfwPollEvents();
    };

    void Glfw::bind_input(KeyInfo key_info, std::function<void()> callback_fn) {
        callbacks.insert({ key_info, callback_fn });
    }

    void Glfw::callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if( callbacks.contains({ (uint16_t)key, (uint16_t)action, (uint16_t)mods }) ) {
            callbacks.at({ (uint16_t)key, (uint16_t)action, (uint16_t)mods })();
        }
    }

    void Glfw::setup_standard_input_bindings() {
        bind_input({ GLFW_KEY_T,      GLFW_PRESS, 0 }, []     () { std::cout << "test\n";                  });
        bind_input({ GLFW_KEY_Q,      GLFW_PRESS, 0 }, [this] () { glfwSetWindowShouldClose(window, true); });
        bind_input({ GLFW_KEY_ESCAPE, GLFW_PRESS, 0 }, [this] () { glfwSetWindowShouldClose(window, true); });
    }
}
