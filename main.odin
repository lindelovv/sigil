
package main

import sigil "sigil:core"
import impl  "sigil:ism"

main :: proc() {
    /* سيجيل */
    using sigil
        use(impl.glfw)
        use(impl.scene)
        use(impl.vulkan)
        run()
}

