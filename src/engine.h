#pragma once

#include "window.h"
#include "pipeline.h"

namespace sigil {
    class Engine {
        public:
            static constexpr int WIDTH = 1920;
            static constexpr int HEIGHT = 1080;

            void run();
        private:
            sigil::Window window{WIDTH, HEIGHT, "Vulkan"};
            sigil::Pipeline pipeline{"./shaders/simple_shader.vert.spv", "./shaders/simple_shader.frag.spv"};
    };
}
