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
PATCH_V :: 2

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

entity_t :: u32
INVALID  :: entity_t(0)
WORLD    :: entity_t(1)

// todo: explore custom allocator
core: struct {
    entities     : [dynamic]entity_t,
    flags        : [dynamic]bit_array.Bit_Array,
    sets         : map[typeid]set_t,
    groups       : map[u64]group_t,
    set_lookup   : map[int]typeid,
    request_exit : bool
}

set_t :: struct {
    components : rawptr,
    indices    : [dynamic]int,
    count      : int,
    id         : int,
    type       : typeid,
    size       : int,
}

group_t :: struct {
    components : bit_array.Bit_Array,
    range      : map[typeid]range_t,
    id         : u64, // figure out cool way to bit_field magic to set relation to other groups for locking (ex. depth level index)
}

range_t :: struct {
    offset : int,
    count  : int,
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(private="file", init) _init :: proc "contextless" () { 
    context = runtime.default_context()
    core = {}; /* INVALID ENTITY: */ 
    new_entity()
}

run :: #force_inline proc() {
    for fn in query(init) { fn() }
    main_loop: for !core.request_exit { for fn in query(tick) { fn() } free_all(context.temp_allocator) }
    for fn in query(exit) { fn() }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

new_entity :: #force_inline proc() -> entity_t {
    e := entity_t(len(core.entities))
    append(&core.entities, e)
    append(&core.flags, bit_array.create(len(core.sets))^)
    return e
}

delete_entity :: #force_inline proc(#any_int entity: entity_t) {
    if !entity_is_valid(entity) do return
    if len(core.flags) <= auto_cast entity do return

    last: int
    invalidated_groups: [dynamic]u64
    already_swapped: [dynamic]^set_t

    // todo: fix as this scrambles components currently
    cont := true
    it := bit_array.make_iterator(&core.flags[entity])
    for cont {
        id: int
        id, cont = bit_array.iterate_by_set(&it)
        _set := &core.sets[core.set_lookup[id]]
        last =_set.count - 1 
        _swap_component_index(_set, entity, last)
        _remove_component_index(_set, last)

        // this needs to sort all groups that contain one of the removed components
        // otherwise it will be perma broken
        // todo: figure out nice storage for easy access and also make proper sorting impl
        //group_iter: for id, &group in &core.groups {
        //    if !bit_array.get(&group.components, _set.id) do continue
        //    for s in already_swapped do if s == _set do continue group_iter
        //    for type, &range in &group.range {
        //        last = range.offset + range.count// - 1
        //        range.count -= 1
        //        __set := core.sets[type]
        //        _swap_component_index(&__set, entity, last)
        //        if range.count == 0 do append(&invalidated_groups, group.id)
        //    }
        //}
    }
    clear(&core.groups)// nuclear option, does not work lol cus group creation does not sort in any way :-)
    bit_array.clear(&core.flags[entity])
    for id in invalidated_groups do delete_key(&core.groups, id)
}

get_entity :: #force_inline proc(entity: entity_t) -> entity_t {
    for e in core.entities { if e == entity do return e }
    return INVALID
}

entity_is_valid :: proc(entity: entity_t) -> bool {
    return entity != INVALID
}

has_component :: #force_inline proc(entity: entity_t, type: typeid) -> bool {
    e := get_entity(entity)
    if e == 0 || int(e) >= len(core.flags) || core.sets[type].components == nil || &core.flags[e] == nil do return false
    return bit_array.get(&core.flags[e], core.sets[type].id)
}

@(require_results)
get_value :: proc(entity: entity_t, $type: typeid) -> (type, bool) #optional_ok {
    if !has_component(entity, type) do return {}, false
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(entity) || len(data) < set.indices[entity] do return {}, false
    return data[set.indices[entity]], true
}

@(require_results)
get_ref :: proc(entity: entity_t, $type: typeid) -> (^type, bool) #optional_ok {
    if !has_component(entity, type) do return {}, false
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    ref := &data[set.indices[entity]]
    return ref, ref != nil
}

add_to_world :: #force_inline proc(component: $type) -> (type, int) { return add_to_entity(WORLD, component) }
add_to_entity :: #force_inline proc(to: entity_t, component: $type) -> (type, int) {
    if type == typeid_of(none) do return {}, 0
    e := get_entity(to)
    if e == 0 do return {}, 0

    set := core.sets[type]
    if exists := &set.components; exists == nil || set.count == 0 {
        set.components = new([dynamic]type)
        append((^[dynamic]type)(set.components), type{}) // dummy for invalid
        set.id = len(core.sets)
        core.set_lookup[set.id] = type
        set.type = type
        set.size = size_of(type)
    }
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(e) do resize_dynamic_array(&set.indices, len(core.entities) + 64)

    if len(data) <= int(e) || len(data) <= set.indices[e] {
        append(data, component)
        set.indices[e] = len(data) - 1
        set.count += 1
    } else do data[set.indices[e]] = component
    core.sets[type] = set

    if len(core.flags) <= int(e) do resize(&core.flags, len(core.entities) + 1)
    if &core.flags[e] == nil do core.flags[e] = bit_array.create(len(core.entities))^
    bit_array.set(&core.flags[e], set.id)

    return component, set.indices[e]
}
add :: proc { add_to_world, add_to_entity }

// todo: ensure this is doing everything correctly
remove_component :: proc { _remove_component_set, _remove_component_type }
_remove_component_type :: proc(#any_int entity: entity_t, $type: typeid) {
    if !has_component(entity, type) do return
    set := &core.sets[type]
    _remove_component_set(entity, set)
}
_remove_component_set :: proc(#any_int entity: entity_t, set: ^set_t) {
    if set.indices[entity] == 0 do return

    _swap_component_index(set, entity, set.count)
    _remove_component_index(set, set.count)

    last: int
    invalidated_groups: [dynamic]u64
    already_swapped: [dynamic]^set_t
    group_iter: for id, &group in &core.groups {
        if !bit_array.get(&group.components, core.sets[set.type].id) do continue

        range := &group.range[set.type]
        last = range.offset + range.count// - 1
        //if last >= len(data) - 1 || set.indices[entity] >= len(data) -1 do continue

        for _, &r in &group.range {
            r.count -= 1
            if range.count == 0 {
                append(&invalidated_groups, group.id)
                continue group_iter
            }
        }

        // todo: this is potentially swapping more than once per set -- tried fixing
        it := bit_array.make_iterator(&group.components)
        sets: for id in bit_array.iterate_by_set(&it) {
            _set := &core.sets[core.set_lookup[id]]
            if _set.type == set.type do continue
            for s in already_swapped do if s == _set do continue sets
            _swap_component_index(_set, entity, last)
        }
        // combined groups of all overlapping would posibly enable max 2 swaps ? probably false ?
    }
    for id in invalidated_groups do delete_key(&core.groups, id)
}

_remove_component_index :: proc(set: ^set_t, #any_int idx: entity_t) {
    data := (^runtime.Raw_Dynamic_Array)(set.components)
    intrinsics.mem_zero(([^]byte)(data)[data.len*set.size:], set.size)
    data.len -= 1; set.count -= 1
    set.components = data
}

_swap_component_index :: proc(set: ^set_t, #any_int a, b: entity_t) {
    _a := rawptr(uintptr(set.components) + uintptr(int(set.indices[a]) * set.size))
    _b := rawptr(uintptr(set.components) + uintptr(int(b) * set.size))
    _a, _b = _b, _a; set.indices[a], set.indices[b] = set.indices[b], 0
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

query_1 :: #force_inline proc($type: typeid) -> []type {
    set := core.sets[type]
    if set.components == nil do return nil
    data := (cast(^[dynamic]type)(set.components))[1:][:set.count]
    return data
}

query_2 :: #force_inline proc(
    $type1: typeid,
    $type2: typeid
) -> #soa[]struct { 
    x: type1,
    y: type2
} {
    g := get_or_declare_group(type1, type2)
    if g == nil do return {}
    get :: proc(g: ^group_t, $t: typeid) -> []t { return get_component_slice(g, t) }
    return soa_zip(get(g, type1), get(g, type2))
}

query_3 :: #force_inline proc(
    $type1: typeid,
    $type2: typeid,
    $type3: typeid
) -> #soa[]struct { 
    x: type1,
    y: type2,
    z: type3
} {
    g := get_or_declare_group(type1, type2, type3)
    if g == nil do return {}
    get :: proc(g: ^group_t, $t: typeid) -> []t { return get_component_slice(g, t) }
    return soa_zip(get(g, type1), get(g, type2), get(g, type3))
}

query_4 :: #force_inline proc(
    $type1: typeid,
    $type2: typeid,
    $type3: typeid,
    $type4: typeid
) -> #soa[]struct { 
    x: type1,
    y: type2,
    z: type3,
    w: type4
} {
    g := get_or_declare_group(type1, type2, type3, type4)
    if g == nil do return {}
    get :: proc(g: ^group_t, $t: typeid) -> []t { return get_component_slice(g, t) }
    return soa_zip(get(g, type1), get(g, type2), get(g, type3), get(g, type4))
}
query :: proc { query_1, query_2, query_3, query_4 }

get_component_slice :: proc(g: ^group_t, $t: typeid) -> []t {
    set := core.sets[t]
    if info, exists := g.range[t]; exists {
        data := cast(^[dynamic]t)(set.components)
        start := info.offset
        end := start + info.count
        if end > len(data^) do return nil
        return data[start:end]
    }
    return nil
}

has_all_components :: proc(entity: entity_t, mask: ^bit_array.Bit_Array) -> bool {
    if entity <= 0 || int(entity) >= len(core.flags) || &core.flags[entity] == nil do return false
    if entity <= 0 || int(entity) >= len(core.flags) do return false
    if &core.flags[entity] == nil do return false
    
    for i in 0..<len(mask.bits) {
        if mask.bits[i] == 0 do continue
        if (core.flags[entity].bits[i] & mask.bits[i]) != mask.bits[i] do return false
    }
    return true
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

types_hash :: proc(types: ..typeid) -> (h: u64) {
    for t in types do h ~= (^u64)(raw_data(mem.any_to_bytes(t)))^
    return
}

get_or_declare_group_2 :: proc($type1, $type2: typeid) -> ^group_t {
    if _, ok := core.sets[type1]; !ok do return {}
    if _, ok := core.sets[type2]; !ok do return {}

    hash := types_hash(type1, type2)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { core.sets[type1], core.sets[type2] }
    if !declare_group(group, sets[:]) { free(group); return nil }
    group.id = hash
    core.groups[hash] = group^
    return group
}

get_or_declare_group_3 :: proc($type1, $type2, $type3: typeid) -> ^group_t {
    if _, ok := core.sets[type1]; !ok do return {}
    if _, ok := core.sets[type2]; !ok do return {}
    if _, ok := core.sets[type3]; !ok do return {}

    hash := types_hash(type1, type2, type3)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { core.sets[type1], core.sets[type2], core.sets[type3] }
    if !declare_group(group, sets[:]) { free(group); return nil }
    group.id = hash
    core.groups[hash] = group^
    return group
}

get_or_declare_group_4 :: proc($type1, $type2, $type3, $type4: typeid) -> ^group_t {
    if _, ok := core.sets[type1]; !ok do return {}
    if _, ok := core.sets[type2]; !ok do return {}
    if _, ok := core.sets[type3]; !ok do return {}
    if _, ok := core.sets[type4]; !ok do return {}

    hash := types_hash(type1, type2, type3, type4)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { core.sets[type1], core.sets[type2], core.sets[type3], core.sets[type4] }
    if !declare_group(group, sets[:]) { free(group); return nil }
    group.id = hash
    core.groups[hash] = group^
    return group
}
get_or_declare_group :: proc { get_or_declare_group_2, get_or_declare_group_3, get_or_declare_group_4 }

declare_group :: proc(new_group: ^group_t, sets: []set_t) -> bool {
    new_group.components = bit_array.create(len(core.sets))^
    for set in sets do bit_array.set(&new_group.components, set.id)

    count := 0
    group_entities: [dynamic]entity_t
    defer delete(group_entities)
    for e in core.entities {
        if entity_is_valid(e) && has_all_components(e, &new_group.components) {
            append(&group_entities, e)
            count += 1
        }
    }
    if count == 0 do return false

    // believe all of this might be fucked
    to_sort := make([dynamic]set_t)
    for set in sets {
        max, min := 1, max(int)
        for e in group_entities {
            if set.indices[e] < min && set.indices[e] != 0 do min = set.indices[e]
            if set.indices[e] > max do max = set.indices[e]
        }
        if max - count == min { // already in ok order (not thinking about other groups)
            new_group.range[set.type] = range_t { min, count }
        } else {
            append(&to_sort, set)
        }
    }
    for set in to_sort {
        min := max(int)
        for e in group_entities do if set.indices[e] < min do min = set.indices[e]
        new_group.range[set.type] = range_t { min, count }
    }
    return true
}
