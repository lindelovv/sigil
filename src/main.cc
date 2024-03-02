#include "sigil.hh"
#include "glfw.hh"
#include "vulkan.hh"

int main() { /*     سيجيل     */
    sigil::init               ()
        .use<glfw>            ()
        .use<vulkan>          ()
        .run                  ();
}

