#include "engine.h"

namespace sigil {
    void Engine::run() {
        while( !window.should_close() ) {
            glfwPollEvents();
        }
    }
}
