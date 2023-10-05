#include "window.hh"
#include "renderer.hh"

namespace sigil {

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
        if( glfwWindowShouldClose(main_window) ) {
            core->should_close = true;
        }
        glfwPollEvents();
    };
}
