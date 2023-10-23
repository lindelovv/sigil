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

    struct Sigil {
        void run();
        template <std::derived_from<struct Module> T> Sigil& add_module();
        std::vector<std::unique_ptr<Module>> modules;
        bool should_close = false;
    };

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
        Sigil* core;
        template <std::derived_from<Module> T> T* get_module();
        void request_exit();
    };

    inline auto& init() {
        Sigil* core = new Sigil;
        return *core;
    }
}

