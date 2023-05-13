#pragma once

#include "window.h"
#include "pipeline.h"
#include <vector>

namespace sigil {
    class Engine {
        public:
            static constexpr int WIDTH = 1920;
            static constexpr int HEIGHT = 1080;

            void run();
            void init_vulkan();
            void create_instance();
            void select_physical_device();
            int score_device_suitability(VkPhysicalDevice);
            struct QueueFamilyIndices find_queue_families(VkPhysicalDevice);
            std::vector<const char*> get_required_extensions();
            void print_extensions();
        private:
            VkInstance instance;
            sigil::Window window{WIDTH, HEIGHT, "Vulkan"};
            sigil::Pipeline pipeline{"./shaders/simple_shader.vert.spv", "./shaders/simple_shader.frag.spv"};
    };
}
