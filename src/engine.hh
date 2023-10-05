#pragma once

#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace sigil {    
    class System;

    class Engine {
        public:
            void run();

        template <typename S>
        inline Engine& add_system() {
            auto system = new S();
            systems.push_back(system);
            return *this;
        }

        private:
            std::vector<System*> systems;
            bool should_close = false;
    };
    extern Engine core;
}

