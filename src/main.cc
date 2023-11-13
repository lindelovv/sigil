#include "sigil.hh"
#include "glfw.hh"
#include "renderer.hh"

using namespace sigil;

int main() { /*     سيجيل     */
    sigil::init               ()
        .add_module<glfw>     ()
        .add_module<renderer> ()
        .run                  ();
}

