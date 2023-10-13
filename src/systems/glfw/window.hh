#pragma once

#include "sigil.hh"

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {
    
    class Window : public system_t {
        public:
            Window() : width(1920), height(1080), title("sigil") {}

            Window(Window& window) : width(window.width), height(window.height), title(window.title) {}

            Window(int w, int h, std::string t) : width(w), height(h), title(t) {}

            virtual void init() override;
            virtual void terminate() override;
            virtual void tick() override;

            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            GLFWwindow* main_window;

        private:
            const int   width;
            const int  height;
            std::string title;
    };
}

