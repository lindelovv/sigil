package main

import sigil "sigil:core"
import       "sigil:ism"

main :: proc() { /* سيجيل */

    world := sigil.init_world({
        ism.glfw,
        ism.vulkan,
        ism.miniaudio,
        ism.jolt,
        ism.scene,
    /* +-+-+-+-+-+-+-+-+ */  })
    sigil.run(world)
}
