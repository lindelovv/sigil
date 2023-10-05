#include "window.hh"
#include "renderer.hh"

namespace sigil {

    Window* window;

    void Window::init() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        main_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetFramebufferSizeCallback(main_window, Renderer::resize_callback);

        window->main_window = main_window;
    }

    void Window::terminate() {
        glfwDestroyWindow(main_window);
        glfwTerminate();
    };

    void Window::tick() {
        glfwPollEvents();
    };

    bool Window::should_close() {
        return glfwWindowShouldClose(main_window);
    };
}
