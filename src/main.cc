#include "engine.hh"
#include "renderer.hh"
#include "glfw.hh"

int main() {
    sigil::core
        .add_system<sigil::Glfw>()
        .add_system<sigil::Renderer>()
        .run();
}

