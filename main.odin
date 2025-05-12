
package main

import sigil "sigil:core"
import       "sigil:ism"

main :: proc() { /* سيجيل */

    using sigil
        use(ism.glfw)
        use(ism.scene)
        use(ism.jolt)
        use(ism.vulkan)
        run()
}

