#pragma once

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {
    
    class Window {
        public:
            Window(int w, int h, std::string t);
            ~Window();
            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            void init();
            bool should_close() { return glfwWindowShouldClose(ptr); };
            GLFWwindow* ptr;

        private:
            const int   width;
            const int  height;
            std::string title;
    };
}
