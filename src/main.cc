#include "sigil.hh"
#include "glfw.hh"
#include "renderer.hh"

int main() {
    sigil::init()
          .add_module<Windowing>()
          .add_module<Renderer>()
          .add_module<Input>()
          .run();
}

