#include "engine.hh"
#include "system.hh"

#include <GLFW/glfw3.h>

namespace sigil {

    Engine core;
    
    void Engine::run() {
        for( auto& system : systems ) {
            system->init();
        }
        while( !should_exit ) {
            glfwPollEvents();
            for( auto& system : systems ) {
                system->tick();
            }
        }
        for( auto& system : systems ) {
            system->terminate();
        }
    }
}

