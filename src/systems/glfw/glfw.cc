#include "glfw.hh"
#include "renderer.hh"

namespace sigil {

    //    WINDOWING    //

    void Windowing::init() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        main_window = new Window;
    }

    void Windowing::terminate() {
        glfwDestroyWindow(main_window->instance);
        glfwTerminate();
    };

    void Windowing::tick() {
        glfwPollEvents();
        if( glfwWindowShouldClose(main_window->instance) ) {
            world->should_close = true;
        }
    };

    //    INPUT    //

    std::unordered_map<KeyInfo, std::function<void()>> Input::callbacks;

    void Input::init() {
        window = world->get_system<Windowing>()->main_window->instance;
        setup_standard_bindings();
        glfwSetKeyCallback(window, callback);
    }

    void Input::callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if( callbacks.contains({ key, action, mods }) ) {
            callbacks.at({ key, action, mods })();
        }
    }

    void Input::setup_standard_bindings() {
        bind({ GLFW_KEY_T,      GLFW_PRESS, 0 }, []    { std::cout << "test\n";                  });
        bind({ GLFW_KEY_Q,      GLFW_PRESS, 0 }, [this]{ glfwSetWindowShouldClose(window, true); });
        bind({ GLFW_KEY_ESCAPE, GLFW_PRESS, 0 }, [this]{ glfwSetWindowShouldClose(window, true); });
    }
}
