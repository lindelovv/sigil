#pragma once

#include "system.hh"

#include <concepts>
#include <memory>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


namespace sigil {    
    class System;

    class Engine {
        public:
            void run();

            template <std::derived_from<System> SystemType>
            inline Engine& add_system() {
                auto system = new SystemType;
                system->link(this);
                systems.push_back(std::unique_ptr<System>(system));
                return *this;
            }

            std::vector<std::unique_ptr<System>> systems;
            bool should_close = false;
    };

    inline Engine& init() {
        auto core = new Engine();
        return *core;
    }
}

