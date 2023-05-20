#include "engine.hh"
#include "input.hh"
#include <GLFW/glfw3.h>

namespace sigil {

    Engine core;

    void Engine::run() {
        window.init();
        input.init();
        renderer.init();

        while( !window.should_close() ) {
            glfwPollEvents();
            renderer.draw();
        }

        renderer.terminate();
    }
}

