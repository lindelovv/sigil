#include "window.h"
#include <GLFW/glfw3.h>

namespace sigil {
    Window::Window(int w, int h, std::string t) : width(w), height(h), title(t) {
        init_window();
    }

    Window::~Window() {
        glfwDestroyWindow(window_ptr);
        glfwTerminate();
    }

    void Window::init_window() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window_ptr = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    }
}
