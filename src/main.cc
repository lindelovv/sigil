#include "sigil.hh"
#include "glfw.hh"
#include "vulkan.hh"
#include "scene.hh"

int main() { /*     سيجيل     */
    sigil::init               ()
        .use<glfw>            ()
        .use<vulkan>          ()
        .use<scene>           ()
        .run                  ();
}

