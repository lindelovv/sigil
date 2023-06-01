#pragma once

#include "window.hh"
#include "renderer.hh"
#include "input.hh"

#include <vector>
#include <optional>

#include <vulkan/vulkan_core.h>

namespace sigil {    

    static constexpr int                       WIDTH = 1920;
    static constexpr int                      HEIGHT = 1080;

    class Engine {
        public:
            void run();
                            // OBJECTS //
            Window        window { WIDTH, HEIGHT, "sigil" };
            Renderer                            renderer {};
        private:
            Input                                  input {};
    };
    extern Engine core;
}

