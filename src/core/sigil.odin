package sigil
import sys "../systems"

SIGIL_V :: "v. 0.0.1"

init :: proc() {
    sys.glfw_init()
    sys.vulkan_test()
}
