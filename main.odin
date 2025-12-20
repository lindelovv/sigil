package main

import sigil "sigil:core"
import       "sigil:ism"
import       "core:mem"
import       "core:fmt"
import       "base:runtime"

main :: proc() { /* سيجيل */
    
    track: mem.Tracking_Allocator
    context = runtime.default_context()
    mem.tracking_allocator_init(&track, context.allocator)
    context.allocator = mem.tracking_allocator(&track)

    world := sigil.init_world()
    sigil.use(world, ism.glfw)
    sigil.use(world, ism.vulkan)
    sigil.use(world, ism.miniaudio)
    sigil.use(world, ism.jolt)
    sigil.use(world, ism.scene)
    sigil.run(world)
    sigil.deinit_world(world)

    if len(track.allocation_map) > 0 {
        fmt.println("=== MEMORY LEAKS DETECTED ===")
        for ptr, entry in track.allocation_map {
            fmt.printf("Leaked %v bytes at %p\n", entry.size, ptr)
            fmt.printf("  Allocated in %s:%d\n", entry.location.file_path, entry.location.line)
        }
    }
    mem.tracking_allocator_destroy(&track)
}

