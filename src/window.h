#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace sigil {
    class Window {
        public:
            Window(int w, int h, std::string t);
            ~Window();
            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            bool should_close() { return glfwWindowShouldClose(window_ptr); };
        private:
            void init_window();
            const int width;
            const int height;
            std::string title;
            GLFWwindow* window_ptr;
    };
}
