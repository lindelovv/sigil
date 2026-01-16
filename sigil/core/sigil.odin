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

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TITLE   :: "__sigil_"
MAJOR_V :: 0
MINOR_V :: 0
PATCH_V :: 3

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

entity_t :: u32
INVALID  :: entity_t(0)
WORLD    :: entity_t(1)

core: struct {
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

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(private="file", init) _init :: proc "contextless" () { 
    context = runtime.default_context()
    core = {}
    new_entity() /* INVALID */
}

run :: #force_inline proc() {
    for fn in query(init) { fn() }
    main_loop: for !core.request_exit { for fn in query(tick) { fn() } free_all(context.temp_allocator) }
    #reverse for fn in query(exit) { fn() }
}

new_entity :: #force_inline proc() -> entity_t {
    e := entity_t(len(core.entities))
    append(&core.entities, e)
    if len(core.flags) <= int(e) do resize(&core.flags, int(e) + 1)
    core.flags[e] = bit_array.create(max(1, len(core.sets)))^
    return e
}

delete_entity :: proc(#any_int entity: entity_t) {
    if !entity_is_valid(entity) do return
    if int(entity) < len(core.flags) {
        it := bit_array.make_iterator(&core.flags[entity])
        for {
            id, cont := bit_array.iterate_by_set(&it)
            if !cont do break
            type := core.set_lookup[id]
            remove_component(entity, type)
        }
        bit_array.clear(&core.flags[entity])
    }
}

get_entity :: #force_inline proc(#any_int entity: entity_t) -> entity_t {
    if int(entity) < len(core.entities) && core.entities[entity] == entity {
        return entity
    }
    return INVALID
}

entity_is_valid :: #force_inline proc(#any_int entity: entity_t) -> bool {
    return entity != INVALID && int(entity) < len(core.entities)
}

has_component :: #force_inline proc(#any_int entity: entity_t, type: typeid) -> bool {
    if entity == INVALID || int(entity) >= len(core.flags) do return false
    set, ok := core.sets[type]
    if !ok do return false
    return bit_array.get(&core.flags[entity], set.id)
}

has_all_components :: proc(#any_int entity: entity_t, mask: ^bit_array.Bit_Array) -> bool {
    if entity == INVALID || int(entity) >= len(core.flags) do return false
    for i in 0..<len(mask.bits) {
        if mask.bits[i] == 0 do continue
        if (core.flags[entity].bits[i] & mask.bits[i]) != mask.bits[i] do return false
    }
    return true
}

@(require_results)
get_value :: proc(#any_int entity: entity_t, $type: typeid) -> (type, bool) #optional_ok {
    if !has_component(entity, type) do return {}, false
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    return data[idx], true
}

@(require_results)
get_ref :: proc(#any_int entity: entity_t, $type: typeid) -> (^type, bool) #optional_ok {
    if !has_component(entity, type) do return {}, false
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    return &data[idx], true
}

add_to_world :: #force_inline proc(component: $type) -> (type, int) { return add_to_entity(WORLD, component) }
add_to_entity :: proc(#any_int to: entity_t, component: $type) -> (type, int) {
    if to == INVALID do return {}, 0
    type_id := typeid_of(type)
    if !(type_id in core.sets) do core.sets[type_id] = {}
    set := &core.sets[type_id]
    if set.components == nil do _init_set(set, typeid_of(type))
    idx := _set_add(set, to, component)
    if len(core.flags) <= int(to) do resize(&core.flags, int(len(core.entities)) + 1)
    if &core.flags[to] == nil do core.flags[to] = bit_array.create(len(core.sets))^
    bit_array.set(&core.flags[to], set.id)
    core.owners[type_id] = nil 
    return component, idx
}
add :: proc { add_to_world, add_to_entity }

remove_component :: proc(#any_int entity: entity_t, type: typeid) {
    if !has_component(entity, type) do return
    set := &core.sets[type]
    _set_remove(set, entity)
    bit_array.unset(&core.flags[entity], set.id)
    core.owners[type] = nil
}

_ensure_group_valid :: proc(g: ^group_t) {
    smallest_count := max(int)
    smallest_type  : typeid
    for t in g.types {
        count := core.sets[t].count
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
        if core.owners[t] != g {
            all_owned = false
            break
        }
    }
    if all_owned do return
    matches := 0
    s_set := &core.sets[smallest_type]
    {
        matches = 0
        for i in 0 ..< s_set.count {
            e := s_set.entities[i]
            if has_all_components(e, &g.components) {
                if i != matches {
                    _set_swap(s_set, i, matches)
                }
                matches += 1
            }
        }
        core.owners[smallest_type] = g
    }
    g.count = matches
    for t in g.types {
        if t == smallest_type do continue
        set := &core.sets[t]
        target_idx := 0
        for i in 0 ..< g.count {
            e := s_set.entities[i]
            curr_idx := set.indices[e]
            if curr_idx != target_idx {
                _set_swap(set, curr_idx, target_idx)
            }
            target_idx += 1
        }
        core.owners[t] = g
    }
}

_init_set :: proc(set: ^set_t, type: typeid) {
    set.components = new(runtime.Raw_Dynamic_Array)
    set.type = type
    ti := type_info_of(type)
    set.size = ti.size
    set.id = len(core.sets) - 1
    core.set_lookup[set.id] = set.type
    if len(set.indices) < len(core.entities) do resize(&set.indices, len(core.entities) + 64)
}

_set_add :: proc(set: ^set_t, #any_int entity: entity_t, component: $type) -> int {
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(entity) do resize(&set.indices, len(core.entities) + 64)
    idx := len(data)
    append(data, component)
    append(&set.entities, entity)
    set.indices[entity] = idx
    set.count += 1
    return idx
}

_set_remove :: proc(set: ^set_t, #any_int entity: entity_t) {
    idx := set.indices[entity]
    last_idx := set.count - 1
    if idx != last_idx do _set_swap(set, idx, last_idx)
    data := (^runtime.Raw_Dynamic_Array)(set.components)
    data.len -= 1
    pop(&set.entities)
    set.indices[entity] = 0
    set.count -= 1
}

_set_swap :: proc(set: ^set_t, idx_a, idx_b: int) {
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

query_1 :: #force_inline proc($type: typeid) -> []type {
    set := core.sets[type]
    if set.components == nil do return nil
    data := (cast(^[dynamic]type)(set.components))
    return data[:]
}

query_2 :: #force_inline proc($T1, $T2: typeid) -> #soa[]struct{x: T1, y: T2} {
    g := get_or_declare_group(T1, T2)
    _ensure_group_valid(g)
    return soa_zip(
        _get_group_slice(g, T1),
        _get_group_slice(g, T2)
    )
}

query_3 :: #force_inline proc($T1, $T2, $T3: typeid) -> #soa[]struct{x: T1, y: T2, z: T3} {
    g := get_or_declare_group(T1, T2, T3)
    _ensure_group_valid(g)
    return soa_zip(
        _get_group_slice(g, T1),
        _get_group_slice(g, T2),
        _get_group_slice(g, T3)
    )
}

query_4 :: #force_inline proc($T1, $T2, $T3, $T4: typeid) -> #soa[]struct{x: T1, y: T2, z: T3, w: T4} {
    g := get_or_declare_group(T1, T2, T3, T4)
    _ensure_group_valid(g)
    return soa_zip(
        _get_group_slice(g, T1),
        _get_group_slice(g, T2),
        _get_group_slice(g, T3),
        _get_group_slice(g, T4)
    )
}
query :: proc { query_1, query_2, query_3, query_4 }

_get_group_slice :: #force_inline proc(g: ^group_t, $T: typeid) -> []T {
    set := core.sets[T]
    data := (cast(^[dynamic]T)(set.components))
    return data[:g.count]
}

get_or_declare_group_2 :: proc($T1, $T2: typeid) -> ^group_t { return _get_or_create_group({T1, T2}) }
get_or_declare_group_3 :: proc($T1, $T2, $T3: typeid) -> ^group_t { return _get_or_create_group({T1, T2, T3}) }
get_or_declare_group_4 :: proc($T1, $T2, $T3, $T4: typeid) -> ^group_t { return _get_or_create_group({T1, T2, T3, T4}) }
get_or_declare_group :: proc { get_or_declare_group_2, get_or_declare_group_3, get_or_declare_group_4 }

_get_or_create_group :: proc(types: []typeid) -> ^group_t {
    h := u64(0)
    for t in types {
        bytes := mem.any_to_bytes(t)
        h = hash.fnv64a(bytes, h)
    }
    if g, ok := core.groups[h]; ok do return g
    g := new(group_t)
    g.id = h
    g.types = make([dynamic]typeid, len(types))
    g.components = bit_array.create(len(core.sets))^
    for t, i in types {
        g.types[i] = t
        if !(t in core.sets) { core.sets[t] = {} }
        if core.sets[t].components == nil {
             set := &core.sets[t]
             _init_set(set, t)
        }
        set_id := core.sets[t].id
        bit_array.set(&g.components, set_id)
    }
    core.groups[h] = g
    return g
}
