#include "sigil.hh"
#include "glfw.hh"
#include "renderer.hh"

using namespace sigil;

int main() {
    Sigil { /*    سيجيل    */ }
        .add_module<Windowing>()
        .add_module<Input>    ()
        .add_module<Renderer> ()
        .run();
}

