#include "sigil.hh"

#include "renderer.hh"
#include "glfw.hh"

using namespace sigil;

int main() {
    sigil::init()
          .add<System, Windowing>()
          .add<System, Input>()
          .add<System, Renderer> ()
          .add<Component, Transform>()
          .add<Component, Lens>()
          .add<Component, Lens>()
          .run();
}

