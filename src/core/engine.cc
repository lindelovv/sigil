#include "engine.hh"
#include "system.hh"
#include "window.hh"
#include "input.hh"

#include <GLFW/glfw3.h>

namespace sigil {
    
    void Engine::run() {
        for( auto& system : systems ) {
            system->init();
        }
        while( !should_close ) {
            glfwPollEvents();
            for( auto& system : systems ) {
                if( system->can_tick ) {
                    system->tick();
                }
            }
        }
        for( auto& system : systems ) {
            system->terminate();
        }
    }
}

