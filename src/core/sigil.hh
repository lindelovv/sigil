#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {

    //____________________________________
    // Runtime id generator, making it easier to add and remove modules.
    class Id {
        inline static std::size_t identifier {};
    public:
        template <typename>
        inline static const std::size_t of_type = identifier++;
    };

    //____________________________________
    // Module base. 
    //
    // @TODO: Research/test if there is a better way to call each system each tick. 
    //        Alternativly, even better, if there is an elegant way to not need inheritance.
    struct Module {
        virtual ~Module()        {}

        virtual void init()      {}
        virtual void terminate() {}
        virtual void tick()      {}

        struct Sigil* sigil;
    };

    //____________________________________
    // Core & builder
    struct Sigil {
        inline void run() {
            for( auto& module : modules ) {
                module->init();
            }
            while( !should_close ) {
                float current_frame = glfwGetTime();
                delta_time = current_frame - last_frame;
                last_frame = current_frame;
                for( auto& module : modules ) {
                    module->tick();
                }
            }
            for( auto& module : modules ) {
                module->terminate();
            }
        }

        template <std::derived_from<Module> T>
        inline Sigil& add_module() {
            auto module = new T;
            module->sigil = this;
            ALLOW_UNUSED(Id::of_type<T>) // Generate id (and thus index) for module.
            modules.push_back(std::shared_ptr<Module>{ module });
            return *this;
        }

        template <std::derived_from<Module> T>
        inline T* get_module() {
            return dynamic_cast<T*>(modules.at(Id::of_type<T>).get());
        }

        float delta_time;
        float last_frame;
        bool should_close = false;
        std::vector<std::shared_ptr<Module>> modules;
    };
}

