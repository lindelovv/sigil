package __sigil

import "core:mem"
import "core:slice"
import "core:math"
import "core:fmt"
import "core:hash"
import "base:intrinsics"
import "base:runtime"
import "core:sys/info"
import "core:container/bit_array"

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

TITLE   :: "__sigil_"
MAJOR_V :: 0
MINOR_V :: 0
PATCH_V :: 4

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

entity_t :: u32
INVALID  :: entity_t(0)

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

world_t :: struct {
    entities       : [dynamic]entity_t,
    flags          : [dynamic]bit_array.Bit_Array,
    sets           : map[typeid]set_t,
    groups         : map[u64]^group_t,
    owners         : map[typeid]^group_t,
    set_lookup     : map[int]typeid,
    request_exit   : bool
}

set_t :: struct {
    components : rawptr,
    entities   : [dynamic]entity_t,
    indices    : [dynamic]int,
    count      : int,
    id         : int,
    type       : typeid,
    size       : int,
}

group_t :: struct {
    types      : [dynamic]typeid,
    components : bit_array.Bit_Array,
    id         : u64,
    count      : int, 
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

init_world :: proc(modules: []module_create_info_t) -> (world: ^world_t) {
    world = new(world_t)
    new_entity(world) /* INVALID */
    for module in modules { setup_module(world, module) }
    return
}

run :: #force_inline proc(world: ^world_t) {
    for fn in query(world, init) { fn(world) }
    main_loop: for !world.request_exit { for fn in query(world, tick) { fn(world) } free_all(context.temp_allocator) }
    #reverse for fn in query(world, exit) { fn(world) }
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

new_entity :: #force_inline proc(world: ^world_t) -> entity_t {
    e := entity_t(len(world.entities))
    append(&world.entities, e)
    if len(world.flags) <= int(e) do resize(&world.flags, int(e) + 1)
    world.flags[e] = bit_array.create(max(1, len(world.sets)))^
    return e
}

delete_entity :: proc(world: ^world_t, #any_int entity: entity_t) {
    if !entity_is_valid(world, entity) do return
    if int(entity) < len(world.flags) {
        it := bit_array.make_iterator(&world.flags[entity])
        for {
            id, cont := bit_array.iterate_by_set(&it)
            if !cont do break
            type := world.set_lookup[id]
            remove_component(world, entity, type)
        }
        bit_array.clear(&world.flags[entity])
    }
}

get_entity :: #force_inline proc(world: ^world_t, #any_int entity: entity_t) -> entity_t {
    if int(entity) < len(world.entities) && world.entities[entity] == entity {
        return entity
    }
    return INVALID
}

entity_is_valid :: #force_inline proc(world: ^world_t, #any_int entity: entity_t) -> bool {
    return entity != INVALID && int(entity) < len(world.entities)
}

has_component :: #force_inline proc(world: ^world_t, #any_int entity: entity_t, type: typeid) -> bool {
    if entity == INVALID || int(entity) >= len(world.flags) do return false
    set, ok := world.sets[type]
    if !ok do return false
    return bit_array.get(&world.flags[entity], set.id)
}

has_all_components :: proc(world: ^world_t, #any_int entity: entity_t, mask: ^bit_array.Bit_Array) -> bool {
    if entity == INVALID || int(entity) >= len(world.flags) do return false
    for i in 0..<len(mask.bits) {
        if mask.bits[i] == 0 do continue
        if (world.flags[entity].bits[i] & mask.bits[i]) != mask.bits[i] do return false
    }
    return true
}

@(require_results)
get_value :: proc(world: ^world_t, #any_int entity: entity_t, $type: typeid) -> (type, bool) #optional_ok {
    if !has_component(world, entity, type) do return {}, false
    set := world.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    return data[idx], true
}

@(require_results)
get_ref :: proc(world: ^world_t, #any_int entity: entity_t, $type: typeid) -> (^type, bool) #optional_ok {
    if !has_component(world, entity, type) do return {}, false
    set := world.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    return &data[idx], true
}

add_component :: proc(world: ^world_t, #any_int to: entity_t, component: $type) -> (type, int) {
    if to == INVALID do return {}, 0
    type_id := typeid_of(type)
    if !(type_id in world.sets) do world.sets[type_id] = {}
    set := &world.sets[type_id]
    if set.components == nil do _set_init(world, set, typeid_of(type))
    idx := _set_add(world, set, to, component)
    if len(world.flags) <= int(to) do resize(&world.flags, int(len(world.entities)) + 1)
    if &world.flags[to] == nil do world.flags[to] = bit_array.create(len(world.sets))^
    bit_array.set(&world.flags[to], set.id)
    world.owners[type_id] = nil 
    return component, idx
}

remove_component :: proc(world: ^world_t, #any_int entity: entity_t, type: typeid) -> bool {
    if !has_component(world, entity, type) do return false
    set := &world.sets[type]
    _set_remove(world, set, entity)
    bit_array.unset(&world.flags[entity], set.id)
    world.owners[type] = nil
    return true
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

_set_init :: proc(world: ^world_t, set: ^set_t, type: typeid) {
    set.components = new(runtime.Raw_Dynamic_Array)
    set.type = type
    ti := type_info_of(type)
    set.size = ti.size
    set.id = len(world.sets) - 1
    world.set_lookup[set.id] = set.type
    if len(set.indices) < len(world.entities) do resize(&set.indices, len(world.entities) + 64)
}

_set_add :: proc(world: ^world_t, set: ^set_t, #any_int entity: entity_t, component: $type) -> int {
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(entity) do resize(&set.indices, len(world.entities) + 64)
    idx := len(data)
    append(data, component)
    append(&set.entities, entity)
    set.indices[entity] = idx
    set.count += 1
    return idx
}

_set_remove :: proc(world: ^world_t, set: ^set_t, #any_int entity: entity_t) {
    idx := set.indices[entity]
    last_idx := set.count - 1
    if idx != last_idx do _set_swap(world, set, idx, last_idx)
    data := (^runtime.Raw_Dynamic_Array)(set.components)
    data.len -= 1
    pop(&set.entities)
    set.indices[entity] = 0
    set.count -= 1
}

_set_swap :: proc(world: ^world_t, set: ^set_t, idx_a, idx_b: int) {
    if idx_a == idx_b do return
    entity_a := set.entities[idx_a]
    entity_b := set.entities[idx_b]
    set.entities[idx_a], set.entities[idx_b] = set.entities[idx_b], set.entities[idx_a]
    raw_array := (^runtime.Raw_Dynamic_Array)(set.components)
    data_ptr  := uintptr(raw_array.data)
    ptr_a := rawptr(data_ptr + uintptr(idx_a * set.size))
    ptr_b := rawptr(data_ptr + uintptr(idx_b * set.size))
    _mem_swap(ptr_a, ptr_b, set.size)
    set.indices[entity_a] = idx_b
    set.indices[entity_b] = idx_a
}

_mem_swap :: proc(a, b: rawptr, size: int) {
    if size == 0 do return
    tmp := make([]byte, size, context.temp_allocator)
    mem.copy(raw_data(tmp), a, size)
    mem.copy(a, b, size)
    mem.copy(b, raw_data(tmp), size)
}

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

_query_1 :: #force_inline proc(world: ^world_t, $type: typeid) -> []type {
    set := world.sets[type]
    if set.components == nil do return nil
    data := (cast(^[dynamic]type)(set.components))
    return data[:]
}

_query_2 :: #force_inline proc(world: ^world_t, $type1, $type2: typeid)->
#soa[] struct   {
    x: type1,
    y: type2,
} /* +-+-+-+ */ {
    g := get_or_declare_group(world, type1, type2)
    _ensure_group_valid(world, g) // todo: move these to add/delete or group creation instead
    return soa_zip(
        _get_group_slice(world, g, type1),
        _get_group_slice(world, g, type2)
    )
}

_query_3 :: #force_inline proc(world: ^world_t, $type1, $type2, $type3: typeid)->
#soa[] struct   {
    x: type1,
    y: type2,
    z: type3,
} /* +-+-+-+ */ {
    g := get_or_declare_group(world, type1, type2, type3)
    _ensure_group_valid(world, g) // todo: move these to add/delete or group creation instead
    return soa_zip(
        _get_group_slice(world, g, type1),
        _get_group_slice(world, g, type2),
        _get_group_slice(world, g, type3),
    )
}

_query_4 :: #force_inline proc(world: ^world_t, $type1, $type2, $type3, $type4: typeid)->
#soa[] struct   {
    x: type1,
    y: type2,
    z: type3,
    w: type4,
} /* +-+-+-+ */ {
    g := get_or_declare_group(world, type1, type2, type3, type4)
    _ensure_group_valid(world, g) // todo: move these to add/delete or group creation instead
    return soa_zip(
        _get_group_slice(world, g, type1),
        _get_group_slice(world, g, type2),
        _get_group_slice(world, g, type3),
        _get_group_slice(world, g, type4),
    )
}
query :: proc { _query_1, _query_2, _query_3, _query_4 }

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

_get_group_slice :: #force_inline proc(world: ^world_t, g: ^group_t, $T: typeid) -> []T {
    set := world.sets[T]
    data := (cast(^[dynamic]T)(set.components))
    return data[:g.count]
}

_get_or_declare_group_2 :: proc(world: ^world_t, $type1, $type2: typeid) -> ^group_t {
    return _get_or_create_group(world, { type1, type2 })
}
_get_or_declare_group_3 :: proc(world: ^world_t, $type1, $type2, $type3: typeid) -> ^group_t {
    return _get_or_create_group(world, { type1, type2, type3 })
}
_get_or_declare_group_4 :: proc(world: ^world_t, $type1, $type2, $type3, $type4: typeid) -> ^group_t {
    return _get_or_create_group(world, { type1, type2, type3, type4 })
}
get_or_declare_group :: proc { _get_or_declare_group_2, _get_or_declare_group_3, _get_or_declare_group_4 }

_get_or_create_group :: proc(world: ^world_t, types: []typeid) -> ^group_t {
    h := u64(0)
    for t in types {
        bytes := mem.any_to_bytes(t)
        h = hash.fnv64a(bytes, h)
    }
    if g, ok := world.groups[h]; ok do return g
    g := new(group_t)
    g.id = h
    g.types = make([dynamic]typeid, len(types))
    g.components = bit_array.create(len(world.sets))^
    for t, i in types {
        g.types[i] = t
        if !(t in world.sets) { world.sets[t] = {} }
        if world.sets[t].components == nil {
             set := &world.sets[t]
             _set_init(world, set, t)
        }
        set_id := world.sets[t].id
        bit_array.set(&g.components, set_id)
    }
    world.groups[h] = g
    return g
}

_ensure_group_valid :: proc(world: ^world_t, g: ^group_t) {
    smallest_count := max(int)
    smallest_type  : typeid
    for t in g.types {
        count := world.sets[t].count
        if count < smallest_count {
            smallest_count = count
            smallest_type  = t
        }
    }
    if smallest_count == 0 {
        g.count = 0
        return
    }
    all_owned := true
    for t in g.types {
        if world.owners[t] != g {
            all_owned = false
            break
        }
    }
    if all_owned do return
    matches := 0
    s_set := &world.sets[smallest_type]
    {
        matches = 0
        for i in 0 ..< s_set.count {
            e := s_set.entities[i]
            if has_all_components(world, e, &g.components) {
                if i != matches {
                    _set_swap(world, s_set, i, matches)
                }
                matches += 1
            }
        }
        world.owners[smallest_type] = g
    }
    g.count = matches
    for t in g.types {
        if t == smallest_type do continue
        set := &world.sets[t]
        target_idx := 0
        for i in 0 ..< g.count {
            e := s_set.entities[i]
            curr_idx := set.indices[e]
            if curr_idx != target_idx {
                _set_swap(world, set, curr_idx, target_idx)
            }
            target_idx += 1
        }
        world.owners[t] = g
    }
}
