#pragma once

#include "renderer.hh"

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {
    
    class Window {
        public:
            Window(int w, int h, std::string t) : 
                width(w), height(h), title(t)
            {}

            ~Window() {
                glfwDestroyWindow(ptr);
                glfwTerminate();
            }

            inline void init() {
                glfwInit();
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

                ptr = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
                glfwSetFramebufferSizeCallback(ptr, Renderer::resize_callback);
            }

            inline bool should_close() { return glfwWindowShouldClose(ptr); };

            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            GLFWwindow* ptr;

        private:
            const int   width;
            const int  height;
            std::string title;
    };
}
