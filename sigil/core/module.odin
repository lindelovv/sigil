package __sigil

import "core:dynlib"
import "core:os"

name_t :: distinct string

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

// todo: implement an actual cross module scheduler for this, but might not be needed tbh
module_create_info_t :: struct {
    id    : entity_t,
    data  : rawptr,
    setup : proc(world: ^world_t, e: entity_t),
    name  : name_t,
    //dir   : os.Dir,
    //lib   : dynlib.Library,
    //mod_time : os.File_Time,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

init :: distinct #type proc(^world_t)
tick :: distinct #type proc(^world_t)
exit :: distinct #type proc(^world_t)
none :: distinct #type proc()

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

// todo: not sure about this change but explore it more (not a fan of manual switch)
use_old :: #force_inline proc(world: ^world_t, setup: module) { e := new_entity(world); add(world, e, setup(world, e)) }
module :: #type proc(^world_t, entity_t) -> typeid
use_new :: #force_inline proc(world: ^world_t, module: module_create_info_t) {
    e := new_entity(world);
    add(world, e, module.name)
    module.setup(world, e)
}
use :: proc { use_old, use_new }

