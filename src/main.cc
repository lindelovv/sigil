#include "engine.hh"
#include "renderer.hh"
#include "window.hh"
#include "input.hh"

#include <cstdlib>
#include <iostream>
#include <exception>

using sigil::Window,
      sigil::Renderer,
      sigil::Input;

int main() {
    sigil::core
        .add_system<Window>()
        .add_system<Renderer>()
        .add_system<Input>()
        .run();
}

