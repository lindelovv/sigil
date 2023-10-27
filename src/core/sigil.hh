#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include <any>

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
    // Core & builder
    struct Sigil {
        inline void run() {
            for( auto&& init : init_fns ) {
                init();
            }
            while( !should_close ) {
                float current_frame = glfwGetTime();
                delta_time = current_frame - last_frame;
                last_frame = current_frame;
                for( auto&& tick : tick_fns ) {
                    tick();
                }
            }
            for( auto&& terminate : terminate_fns ) {
                terminate();
            }
        }

        template <typename T>
        inline Sigil& add_module() {
            auto module = new T(*this);
            ALLOW_UNUSED(Id::of_type<T>) // Generate id (and thus index) for module.
            modules.push_back(module);
            return *this;
        }

        template <typename T>
        inline T* get_module() {
            return static_cast<T*>(modules.at(Id::of_type<T>));
        }

        float delta_time;
        float last_frame;
        bool should_close = false;
        std::vector<void*> modules;
        std::vector<std::function<void()>> init_fns;
        std::vector<std::function<void()>> tick_fns;
        std::vector<std::function<void()>> terminate_fns;
    };
}

