#include "glfw.hh"
#include "renderer.hh"

namespace sigil {

    //    INPUT    //

    void Window::init() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        main_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetFramebufferSizeCallback(main_window, Renderer::resize_callback);
    }

    void Window::terminate() {
        glfwDestroyWindow(main_window);
        glfwTerminate();
    };

    void Window::tick() {
        glfwPollEvents();
        if( glfwWindowShouldClose(main_window) ) {
            world->should_close = true;
        }
    };

    //    INPUT    //

    std::unordered_map<KeyInfo, std::function<void()>> Input::callbacks;

    void Input::init() {
        window = world->get_system<Window>();
        setup_standard_bindings();
        glfwSetKeyCallback(window->main_window, callback);
    }

    void Input::callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if( callbacks.contains({ key, action, mods }) ) {
            callbacks.at({ key, action, mods })();
        }
    }

    void Input::setup_standard_bindings() {
        bind({ GLFW_KEY_T,      GLFW_PRESS, 0 }, []    { std::cout << "test\n";                               });
        bind({ GLFW_KEY_Q,      GLFW_PRESS, 0 }, [this]{ glfwSetWindowShouldClose(window->main_window, true); });
        bind({ GLFW_KEY_ESCAPE, GLFW_PRESS, 0 }, [this]{ glfwSetWindowShouldClose(window->main_window, true); });
    }
}
