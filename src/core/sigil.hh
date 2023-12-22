#pragma once

#include "types.hh"

#include <initializer_list>
#include <iostream>
#include <string>
#include <format>
#include <vector>
#include <functional>
#include <bitset>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {
    //____________________________________
    // Version info
    namespace version {
        inline uint8_t major = 0;
        inline uint8_t minor = 0;
        inline uint8_t patch = 1;
        static const std::string as_string = std::format("v. {}.{}.{} ", major, minor, patch);
    };

    //____________________________________
    // Runtime id generator.
    class id {
        inline static std::size_t identifier {};
    public:
        template <typename>
        inline static const std::size_t of_type = identifier++;
    };

    namespace /* private */ {

        enum run_level {
            init,
            tick,
            exit,
        };
        inline std::unordered_map<run_level, std::vector<std::function<void()>>> delegates {
            { init, std::vector<std::function<void()>> {} },
            { tick, std::vector<std::function<void()>> {} },
            { exit, std::vector<std::function<void()>> {} },
        };

        inline void exec(run_level lvl) {
            for( auto&& fn : delegates.at(lvl) ) { fn(); }
        }

        inline std::vector<void*> modules;
        inline bool should_close = false;

        //struct {
        //    std::vector<std::bitset<std::size_t>> entity_bitset;
        //} inline entity_data;

        //____________________________________
        // Core & builder
        struct _sigil {
            template <typename T>
            _sigil& add_module() {
                ALLOW_UNUSED(id::of_type<T>)
                modules.push_back(new T);
                return *this;
            }

            void run() {
                exec(init);
                while( !should_close ) {
                    exec(tick);
                }
                exec(exit);
            }
        } inline core;
    }

    inline _sigil& init() {
        return core;
    }

    inline void schedule(run_level lvl, std::initializer_list<std::function<void()>> delegate_list) {
        for( auto&& delegate : delegate_list ) {
            delegates.at(lvl).emplace_back(delegate);
        }
    }

    inline void schedule(run_level lvl, std::function<void()> delegate) {
        delegates.at(lvl).emplace_back(delegate);
    }

    inline void request_exit() {
        should_close = true;
    }
}

