#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {

    class Id {
        inline static std::size_t identifier {};
    public:
        template <typename>
        inline static const std::size_t of_type = identifier++;
    };

    struct Module {
        virtual ~Module()      {};

        virtual void init()      {};
        virtual void terminate() {};
        virtual void tick()      {};
    };

    struct Sigil {
        inline void run() {
            for( auto& module : modules ) {
                module->init();
            }
            while( !should_close ) {
                for( auto& module : modules ) {
                    module->tick();
                }
            }
            for( auto& module : modules ) {
                module->terminate();
            }
        }

        template <std::derived_from<Module> T>
        inline auto& add_module() {
            auto module = new T;
            ALLOW_UNUSED(Id::of_type<T>); // generate id (and thus index) for module
            modules.push_back(std::unique_ptr<Module>(module));
            return *this;
        }

        bool should_close = false;
        std::vector<std::unique_ptr<Module>> modules;
    } static inline core;

    template <std::derived_from<Module> T>
    inline T* get_module() {
        return dynamic_cast<T*>(core.modules.at(Id::of_type<T>).get());
    }

    inline auto& init() {
        return core;
    }

    inline void request_exit() {
        core.should_close = true;
    }
}

