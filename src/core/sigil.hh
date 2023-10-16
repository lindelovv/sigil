#pragma once

#include <concepts>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define __unused(...) (void)(__VA_ARGS__);

namespace sigil {

    class id {
        inline static std::size_t identifier {};
    public:
        template <typename>
        inline static const std::size_t of_type = identifier++;
    };

    class system_t {
        public:
            virtual ~system_t()      {};

            virtual void init()      {};
            virtual void terminate() {};
            virtual void tick()      {};

            class sigil_t* world;
    };

    class sigil_t {
        public:
            inline void run() {
                for( auto& system : systems ) {
                    system->init();
                }
                while( !should_close ) {
                    for( auto& system : systems ) {
                        system->tick();
                    }
                }
                for( auto& system : systems ) {
                    system->terminate();
                }
            }

            template <std::derived_from<system_t> T>
            inline sigil_t& add_system() {
                auto system = new T;
                system->world = this;
                //std::cout << "id: " << id::of_type<T> << "\n";
                __unused(id::of_type<T>); // generate id (and thus index) for system
                systems.push_back(std::unique_ptr<system_t>(system));
                return *this;
            }

            template <std::derived_from<system_t> T>
            inline T* get_system() {
                return dynamic_cast<T*>(systems.at(id::of_type<T>).get());
            }

            bool should_close = false;
            std::vector<std::unique_ptr<system_t>> systems;
    };

    inline sigil_t& init() {
        auto core = new sigil_t();
        return *core;
    }
}

