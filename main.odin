
package main

import sigil "sigil:core"
import       "sigil:ism"
import "core:fmt"
import "core:mem"

main :: proc() { /* سيجيل */

    using sigil
        use(ism.glfw)
        use(ism.jolt)
        use(ism.vulkan)
        use(ism.scene)
        run()
}

