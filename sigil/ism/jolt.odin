
package ism

import sigil "sigil:core"
import "lib:jolt"

jolt :: proc() {
    using sigil
    schedule(init(init_jolt))
    schedule(tick(tick_jolt))
    schedule(exit(exit_jolt))
}

init_jolt :: proc() {
    jolt.Init()
}

tick_jolt :: proc() {
}

exit_jolt :: proc() {
    jolt.Shutdown()
}

