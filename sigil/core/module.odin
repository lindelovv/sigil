package __sigil

import "core:dynlib"
import "core:os"

name_t :: distinct string

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

// todo: implement an actual cross module scheduler for this, but might not be needed tbh
module_create_info_t :: struct {
    id    : entity_t,
    data  : rawptr,
    setup : proc(e: entity_t),
    name  : name_t,
    //dir   : os.Dir,
    //lib   : dynlib.Library,
    //mod_time : os.File_Time,
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

// todo: not sure about this change but explore it more (not a fan of manual switch)
use_old :: #force_inline proc(setup: module) { e := new_entity(); add(e, setup(e)) }
module :: #type proc(entity_t) -> typeid
use_new :: #force_inline proc(module: module_create_info_t) {
    e := new_entity();
    add(e, module.name)
    module.setup(e)
}
use :: proc { use_old, use_new }

