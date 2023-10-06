#pragma once

#include "util.hh"

#include <concepts>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {    
    struct Component;
    class System;

    class Engine {
        public:
            void run();

            template <std::derived_from<System> SystemType> inline Engine& add_system() {
                auto system = new SystemType;
                expect("Invalid system module passed.",
                    system
                );
                system->link(this);
                systems.push_back(std::unique_ptr<System>(system));
                return *this;
            }

            bool should_exit = false;
            std::vector<std::unique_ptr<System>> systems;
            std::vector<std::shared_ptr<Component>> components;
    };
    extern Engine core;
}


