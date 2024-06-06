package main
import sigil "src"

main :: proc() {
    using sigil
        use(__glfw)
        use(__vulkan)
        run()
}

