#include "input.hh"
#include "engine.hh"
#include <GLFW/glfw3.h>
#include <functional>
#include <map>
#include <any>

namespace sigil {

    std::unordered_map<KeyInfo, std::function<void()>> Input::callbacks;

    void Input::init() {
        setup_standard_bindings(core.window.ptr);
        glfwSetKeyCallback(core.window.ptr, callback);
    }

    void Input::callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if( callbacks.contains({ key, action, mods }) ) {
            callbacks.at({ key, action, mods })();
        } // else { std::cout << "non bound input.\n"; }
    }

    void Input::setup_standard_bindings(GLFWwindow* window) {
        bind({ GLFW_KEY_T,      GLFW_PRESS, 0 }, [=, this]{     std::cout << "test\n";                  });
        bind({ GLFW_KEY_Q,      GLFW_PRESS, 0 }, [=, this]{     glfwSetWindowShouldClose(window, true); });
        bind({ GLFW_KEY_ESCAPE, GLFW_PRESS, 0 }, [=, this]{     glfwSetWindowShouldClose(window, true); });
    }
}

