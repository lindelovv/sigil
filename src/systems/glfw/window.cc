#include "window.hh"
#include "renderer.hh"

namespace sigil {

    void Window::init() {
        can_tick = true;

        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        main_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetFramebufferSizeCallback(main_window, Renderer::resize_callback);
        if( auto tst = core->get<test>() ) {
            std::cout << tst->s;
        } else {
            std::cout << "nullptr";
        }
    }

    void Window::terminate() {
        glfwDestroyWindow(main_window);
        glfwTerminate();
    };

    void Window::tick() {
        glfwPollEvents();
        if( glfwWindowShouldClose(main_window) ) {
            core->should_close = true;
        }
    };
}
