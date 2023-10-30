#pragma once

#include <string>
#include <format>
#include <vector>
#include <functional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {

    struct Version {
        static const uint8_t major = 0;
        static const uint8_t minor = 0;
        static const uint8_t patch = 1;
        static const inline std::string to_string() {
            return std::format("sigil   v. {}.{}.{} ", major, minor, patch);
        }
    } const inline version;

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
            for( auto&& init : init_delegates ) {
                init();
            }
            while( !should_close ) {
                float current_time = glfwGetTime();
                delta_time = current_time - last_frame;
                last_frame = current_time;
                for( auto&& tick : tick_delegates ) {
                    tick();
                }
            }
            for( auto&& terminate : exit_delegates ) {
                terminate();
            }
        }

        template <typename T>
        inline Sigil& add_module() {
            ALLOW_UNUSED(Id::of_type<T>) // Generate id (and thus index) for module.
            modules.push_back(new T(*this));
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
        std::vector<std::function<void()>> init_delegates;
        std::vector<std::function<void()>> tick_delegates;
        std::vector<std::function<void()>> exit_delegates;
    };
}

