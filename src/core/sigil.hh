#pragma once

#include "types.hh"

#include <initializer_list>
#include <iostream>
#include <string>
#include <format>
#include <vector>
#include <functional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {
    //____________________________________
    // Version info
    namespace version {
        inline uint32_t major = 0;
        inline uint32_t minor = 0;
        inline uint32_t patch = 1;
        static const std::string as_string = std::format("v. {}.{}.{} ", major, minor, patch);
    };

    //____________________________________
    // Runtime id generator, making it easier to add and remove modules.
    class id {
        inline static std::size_t identifier {};
    public:
        template <typename>
        inline static const std::size_t of_type = identifier++;
    };

    namespace /* private */ {
        inline bool should_close = false;
        inline std::vector<void*> modules;
        inline std::vector<std::function<void()>> init_delegates;
        inline std::vector<std::function<void()>> tick_delegates;
        inline std::vector<std::function<void()>> exit_delegates;
        //____________________________________
        // Core & builder
        struct _sigil {
            template <typename T>
            inline _sigil& add_module() {
                ALLOW_UNUSED(id::of_type<T>) // Generate id (and thus index) for module.
                modules.push_back(new T);
                return *this;
            }

            inline void run() {
                for( auto&& init : init_delegates ) {
                    init();
                }
                while( !should_close ) {
                    for( auto&& tick : tick_delegates ) {
                        tick();
                    }
                }
                for( auto&& terminate : exit_delegates ) {
                    terminate();
                }
            }
        } inline core;
    }

    inline _sigil& init() {
        return core;
    }

    inline void request_exit() {
        should_close = true;
    }

    template <typename T>
    inline T* get_module() {
        return static_cast<T*>(modules.at(id::of_type<T>));
    }

    inline void add_init_fn(std::function<void()> fn) {
        init_delegates.push_back(fn);
    }

    inline void add_tick_fn(std::function<void()> fn) {
        tick_delegates.push_back(fn);
    }

    inline void add_exit_fn(std::function<void()> fn) {
        exit_delegates.push_back(fn);
    }
}

