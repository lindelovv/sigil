#include "window.hh"
#include "engine.hh"
#include "renderer.hh"

#include <GLFW/glfw3.h>

namespace sigil {
    
    Window::Window(int w, int h, std::string t) : 
        width(w), height(h), title(t)
    {}

    Window::~Window() {
        glfwDestroyWindow(ptr);
        glfwTerminate();
    }

    void Window::init() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        ptr = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetFramebufferSizeCallback(ptr, Renderer::resize_callback);
    }
}
