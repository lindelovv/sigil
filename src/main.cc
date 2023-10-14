
#include "sigil.hh"
#include "glfw.hh"
#include "renderer.hh"

using namespace sigil;

int main() {
    sigil::init()
        .add_system<Windowing>()
        .add_system<Renderer>()
        .add_system<Input>()
        .run();
}

