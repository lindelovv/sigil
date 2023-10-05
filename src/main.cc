#include "engine.hh"
#include "renderer.hh"
#include "window.hh"
#include "input.hh"

using namespace sigil;

int main() {
    sigil::init()
        .add_system<Window>()
        .add_system<Renderer>()
        .add_system<Input>()
        .run();
}

