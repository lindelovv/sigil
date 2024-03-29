#pragma once

#include "types.hh"

#include <initializer_list>
#include <string>
#include <vector>

#include <fmt/core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define ALLOW_UNUSED(...) { ((void)(__VA_ARGS__)); }

namespace sigil {
    using entity_t = u32;

    //____________________________________
    // Version info
    namespace version {
        static u8 major = 0;
        static u8 minor = 0;
        static u8 patch = 1;
        static const std::string as_string = fmt::format("v. {}.{}.{} ", major, minor, patch);
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

        enum Runlvl {
            eInit,
            eTick,
            eExit,
        };

        inline std::unordered_map<Runlvl, std::vector<fn<void()>>> delegates {
            { eInit, std::vector<fn<void()>> {} },
            { eTick, std::vector<fn<void()>> {} },
            { eExit, std::vector<fn<void()>> {} },
        };

        inline void exec(Runlvl lvl) {
            for( auto&& fn : delegates.at(lvl) ) { fn(); }
        }

        inline std::vector<void*> _modules;
        inline bool should_close = false;

        //____________________________________
        // Core & builder
        struct _sigil {
            template <typename T>
            _sigil& use() {
                ALLOW_UNUSED(id::of_type<T>);
                _modules.push_back(new T);
                return *this;
            }

            void run() {
                exec(eInit);
                while( !should_close ) {
                    exec(eTick);
                }
                exec(eExit);
            }
        } inline _core;

        struct Module {
            void setup();
        };
    }

    inline _sigil& init() {
        return _core;
    }

    inline void schedule(Runlvl lvl, std::initializer_list<std::function<void()>> delegate_list) {
        for( auto&& delegate : delegate_list ) {
            delegates.at(lvl).emplace_back(delegate);
        }
    }

    inline void schedule(Runlvl lvl, std::function<void()> delegate) {
        delegates.at(lvl).emplace_back(delegate);
    }

    inline void request_exit() {
        should_close = true;
    }
}

