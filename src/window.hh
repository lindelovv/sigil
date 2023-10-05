#pragma once

#include "engine.hh"
#include "system.hh"

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {
    
    class Window : public System {
        public:
            Window() : width(1920), height(1080), title("sigil") {}

            Window(Window& window) : width(window.width), height(window.height), title(window.title) {}

            Window(int w, int h, std::string t) : width(w), height(h), title(t) {}

            virtual void init() override;
            inline virtual void link(Engine* engine) override { core = engine; }
            virtual void terminate() override;
            inline virtual bool can_tick() override { return true; };
            virtual void tick() override;

            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            GLFWwindow* main_window;

        private:
            const int   width;
            const int  height;
            std::string title;
            Engine*      core;
    };
}

