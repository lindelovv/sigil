
package __sigil

import "core:fmt"
import "core:mem"
import "base:intrinsics"

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
TITLE   :: "__sigil_"
MAJOR_V :: 0
MINOR_V :: 0
PATCH_V :: 2

entity :: distinct int
@(private="file") _sparse_set :: struct {
    indices    : [dynamic]int,
    components : rawptr,
    count      : int,
}
@(private="file") _core: struct {
    entities: [dynamic]entity,
    sets: map[typeid]_sparse_set,
}

scheduler :: struct {
    test: proc()
}

module :: proc()
init :: distinct proc()
tick :: distinct proc()
exit :: distinct proc()

request_exit : bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
use :: #force_inline proc(setup: module) { setup() }

before :: proc(fn: $type) where intrinsics.type_is_proc(type) -> int {
    return 0
}
after :: proc(fn: $type) where intrinsics.type_is_proc(type) -> int {
    return 0
}
require :: proc(fn: $type) where intrinsics.type_is_proc(type) -> int { return after(fn) }

schedule :: #force_inline proc(fn: $type) where intrinsics.type_is_proc(type) {
    // add require param to schedule proc depending on dependant modules
    // as in: before(module), after(other_module)
    add_component(type(fn))
}

add_component :: #force_inline proc(value: $type) {
    data := _core.sets[type]
    if exists := &data.components; exists == nil || data.count == 0 {
        data.components = new([dynamic]type)
    }
    append_elem(cast(^[dynamic]type)(data.components), value)
    data.count += 1
    _core.sets[type] = data
}

query :: #force_inline proc($type: typeid) -> ^[dynamic]type {
    return cast(^[dynamic]type)(_core.sets[type].components)
}

run :: #force_inline proc() {
    for fn in query(init) { fn() }
    main_loop: for !request_exit { for fn in query(tick) { fn() } }
    for fn in query(exit) { fn() }
}
//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

