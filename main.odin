package main

import sigil "sigil:core"
import       "sigil:ism"

main :: proc() { /* سيجيل */

    using sigil
        use(ism.glfw)
        use(ism.vulkan)
        use(ism.miniaudio)
        use(ism.jolt)
        use(ism.scene)
        run()
}

