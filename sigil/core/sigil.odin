package __sigil

import "core:mem"
import "core:slice"
import "core:math"
import "core:fmt"
import "core:hash"
import "base:runtime"
import "base:intrinsics"
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

// todo: explore custom allocator
world_t :: struct {
    //allocator    : runtime.Allocator,
    entities     : [dynamic]entity_t,
    flags        : [dynamic]bit_array.Bit_Array,
    sets         : map[typeid]set_t,
    groups       : map[u64]group_t,
    set_lookup   : map[int]typeid,
    request_exit : bool
}

set_t :: struct {
    components : rawptr,
    indices    : [dynamic]u32,
    count      : int,
    id         : int,
    type       : typeid,
    size       : int,
}

group_t :: struct {
    components : bit_array.Bit_Array,
    range      : map[typeid]range_t,
    using _    : bit_field u64 {
        hash  : u64 | 56,
        flags : u8  | 8,
    }, // figure out cool way to bit_field magic to set relation to other groups for locking (ex. depth level index)
}

range_t :: struct {
    offset : u32,
    count  : u32,
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

init_world :: proc() -> (world: ^world_t) {
    world = &world_t {}
    world.sets = make(map[typeid]set_t)
    world.entities = make([dynamic]entity_t)
    world.flags = make([dynamic]bit_array.Bit_Array)
    world.groups = make(map[u64]group_t)
    new_entity(world)
    return
}

deinit_world :: proc(world: ^world_t) { 
    delete(world.entities)
    delete(world.groups)
    delete(world.set_lookup)
    for type, &set in world.sets {
        components := cast(^mem.Raw_Dynamic_Array)(set.components)
        slice := slice.from_ptr(&components.data, components.len)
        delete(slice)
        delete(set.indices)
    }
    delete(world.sets)
    //for &flag in world.flags {
    //    bit_array.destroy(&flag)
    //}
}

run :: #force_inline proc(world: ^world_t) {
    for fn in query(world, init) { fn(world) }
    main_loop: for !world.request_exit { for fn in query(world, tick) { fn(world) } free_all(context.temp_allocator) }
    slice.reverse(query(world, exit))
    for fn in query(world, exit) { fn(world) }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

new_entity :: #force_inline proc(world: ^world_t) -> entity_t {
    world := world^
    e := entity_t(len(world.entities))
    append(&world.entities, e)
    append(&world.flags, bit_array.create(len(world.sets))^)
    return e
}

delete_entity :: #force_inline proc(world: ^world_t, #any_int entity: entity_t) {
    if !entity_is_valid(world, entity) do return
    if len(world.flags) <= auto_cast entity do return

    last: u32
    invalidated_groups: [dynamic]u64
    already_swapped: [dynamic]^set_t

    // todo: fix as this scrambles components currently
    cont := true
    it := bit_array.make_iterator(&world.flags[entity])
    for cont {
        id: int
        id, cont = bit_array.iterate_by_set(&it)
        _set := &world.sets[world.set_lookup[id]]
        last = u32(_set.count - 1)
        _swap_component_index(_set, entity, last)
        _remove_component_index(_set, last)

        // this needs to sort all groups that contain one of the removed components
        // otherwise it will be perma broken
        // todo: figure out nice storage for easy access and also make proper sorting impl
        //group_iter: for id, &group in &world.groups {
        //    if !bit_array.get(&group.components, _set.id) do continue
        //    for s in already_swapped do if s == _set do continue group_iter
        //    for type, &range in &group.range {
        //        last = range.offset + range.count// - 1
        //        range.count -= 1
        //        __set := world.sets[type]
        //        _swap_component_index(&__set, entity, last)
        //        if range.count == 0 do append(&invalidated_groups, group.id)
        //    }
        //}
    }
    clear(&world.groups)// nuclear option, does not work lol cus group creation does not sort in any way :-)
    bit_array.clear(&world.flags[entity])
    for id in invalidated_groups do delete_key(&world.groups, id)
}

get_entity :: #force_inline proc(world: ^world_t, entity: entity_t) -> entity_t {
    world := world^
    for e in world.entities { if e == entity do return e }
    return INVALID
}

entity_is_valid :: proc(world: ^world_t, entity: entity_t) -> bool {
    return entity != INVALID
}

has_component :: #force_inline proc(world: ^world_t, entity: entity_t, type: typeid) -> bool {
    e := get_entity(world, entity)
    if e == 0 || int(e) >= len(world.flags) || world.sets[type].components == nil || &world.flags[e] == nil do return false
    return bit_array.get(&world.flags[e], world.sets[type].id)
}

@(require_results)
get_value :: proc(world: ^world_t, entity: entity_t, $type: typeid) -> (type, bool) #optional_ok {
    if !has_component(world, entity, type) do return {}, false
    set := world.sets[type]
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(entity) || len(data) < auto_cast set.indices[entity] do return {}, false
    return data[set.indices[entity]], true
}

@(require_results)
get_ref :: proc(world: ^world_t, entity: entity_t, $type: typeid) -> (^type, bool) #optional_ok {
    if !has_component(world, entity, type) do return {}, false
    set := world.sets[type]
    data := cast(^[dynamic]type)(set.components)
    ref := &data[set.indices[entity]]
    return ref, ref != nil
}

add :: #force_inline proc(world: ^world_t, to: entity_t, component: $type) -> (type, u32) {
    world := world^
    if type == typeid_of(none) do return {}, 0
    e := get_entity(&world, to)
    if e == 0 do return {}, 0

    set := world.sets[type]
    if exists := &set.components; exists == nil || set.count == 0 {
        set.components = new([dynamic]type)
        append((^[dynamic]type)(set.components), type{}) // dummy for invalid
        set.id = len(world.sets)
        world.set_lookup[set.id] = type
        set.type = type
        set.size = size_of(type)
    }
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(e) do resize_dynamic_array(&set.indices, len(world.entities) + 64)

    if len(data) <= int(e) || len(data) <= auto_cast set.indices[e] {
        append(data, component)
        set.indices[e] = auto_cast len(data) - 1
        set.count += 1
    } else do data[set.indices[e]] = component
    world.sets[type] = set

    if len(world.flags) <= int(e) do resize(&world.flags, len(world.entities) + 1)
    if &world.flags[e] == nil do world.flags[e] = bit_array.create(len(world.entities))^
    bit_array.set(&world.flags[e], set.id)

    return component, set.indices[e]
}

// todo: ensure this is doing everything correctly
remove_component :: proc { _remove_component_set, _remove_component_type }
_remove_component_type :: proc(world: ^world_t, #any_int entity: entity_t, $type: typeid) {
    if !has_component(world, entity, type) do return
    set := &world.sets[type]
    _remove_component_set(entity, set)
}
_remove_component_set :: proc(world: ^world_t, #any_int entity: entity_t, set: ^set_t) {
    if set.indices[entity] == 0 do return

    _swap_component_index(set, entity, set.count)
    _remove_component_index(set, set.count)

    last: u32
    invalidated_groups: [dynamic]u64
    already_swapped: [dynamic]^set_t
    group_iter: for id, &group in &world.groups {
        if !bit_array.get(&group.components, world.sets[set.type].id) do continue

        range := &group.range[set.type]
        last = range.offset + range.count// - 1
        //if last >= len(data) - 1 || set.indices[entity] >= len(data) -1 do continue

        for _, &r in &group.range {
            r.count -= 1
            if range.count == 0 {
                append(&invalidated_groups, group.hash)
                continue group_iter
            }
        }

        // todo: this is potentially swapping more than once per set -- tried fixing
        it := bit_array.make_iterator(&group.components)
        sets: for id in bit_array.iterate_by_set(&it) {
            _set := &world.sets[world.set_lookup[id]]
            if _set.type == set.type do continue
            for s in already_swapped do if s == _set do continue sets
            _swap_component_index(_set, entity, last)
        }
        // combined groups of all overlapping would posibly enable max 2 swaps ? probably false ?
    }
    for id in invalidated_groups do delete_key(&world.groups, id)
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

query_1 :: #force_inline proc(world: ^world_t, $type: typeid) -> []type {
    set := world.sets[type]
    if set.components == nil do return nil
    data := (cast(^[dynamic]type)(set.components))[1:][:set.count]
    return data
}

query_2 /* +-+-+-+-+ */ :: #force_inline proc(
    world: ^world_t, 
    $type1: typeid,
    $type2: typeid
/* +-+-+-+-+-+-+-+-+ */ ) -> #soa[]struct { 
    x: type1, 
    y: type2
} {
    g := get_or_declare_group(world, type1, type2)
    if g == nil do return {}
    get :: proc(world: ^world_t, g: ^group_t, $t: typeid) -> []t { return get_component_slice(world, g, t) }
    return soa_zip(get(world, g, type1), get(world, g, type2))
}

query_3 /* +-+-+-+-+ */ :: #force_inline proc(
    world: ^world_t, 
    $type1: typeid,
    $type2: typeid,
    $type3: typeid
/* +-+-+-+-+-+-+-+-+ */ ) -> #soa[]struct { 
    x: type1, 
    y: type2, 
    z: type3 
} {
    g := get_or_declare_group(world, type1, type2, type3)
    if g == nil do return {}
    get :: proc(world: ^world_t, g: ^group_t, $t: typeid) -> []t { return get_component_slice(world, g, t) }
    return soa_zip(get(world, g, type1), get(world, g, type2), get(world, g, type3))
}

query_4 /* +-+-+-+-+ */ :: #force_inline proc(
    world: ^world_t, 
    $type1: typeid,
    $type2: typeid,
    $type3: typeid,
    $type4: typeid
/* +-+-+-+-+-+-+-+-+ */ ) -> #soa[]struct { 
    x: type1,
    y: type2,
    z: type3,
    w: type4
} {
    g := get_or_declare_group(world, type1, type2, type3, type4)
    if g == nil do return {}
    get :: proc(world: ^world_t, g: ^group_t, $t: typeid) -> []t { return get_component_slice(world, g, t) }
    return soa_zip(get(world, g, type1), get(world, g, type2), get(world, g, type3), get(world, g, type4))
}
query :: proc { query_1, query_2, query_3, query_4 }

get_component_slice :: proc(world: ^world_t, g: ^group_t, $t: typeid) -> []t {
    set := world.sets[t]
    if info, exists := g.range[t]; exists {
        data := cast(^[dynamic]t)(set.components)
        start := info.offset
        end := start + info.count
        if auto_cast end > len(data^) do return nil
        return data[start:end]
    }
    return nil
}

has_all_components :: proc(world: ^world_t, entity: entity_t, mask: ^bit_array.Bit_Array) -> bool {
    if entity <= 0 || int(entity) >= len(world.flags) || &world.flags[entity] == nil do return false
    if entity <= 0 || int(entity) >= len(world.flags) do return false
    if &world.flags[entity] == nil do return false
    
    for i in 0..<len(mask.bits) {
        if mask.bits[i] == 0 do continue
        if (world.flags[entity].bits[i] & mask.bits[i]) != mask.bits[i] do return false
    }
    return true
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

types_hash :: proc(types: ..typeid) -> (h: u64) {
    for t in types do h ~= (^u64)(raw_data(mem.any_to_bytes(t)))^
    return
}

get_or_declare_group_2 :: proc(world: ^world_t, $type1, $type2: typeid) -> ^group_t {
    if _, ok := world.sets[type1]; !ok do return {}
    if _, ok := world.sets[type2]; !ok do return {}

    hash := types_hash(type1, type2)
    if group, exists := &world.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { world.sets[type1], world.sets[type2] }
    if !declare_group(world, group, sets[:]) { free(group); return nil }
    group.hash = hash
    world.groups[hash] = group^
    return group
}

get_or_declare_group_3 :: proc(world: ^world_t, $type1, $type2, $type3: typeid) -> ^group_t {
    if _, ok := world.sets[type1]; !ok do return {}
    if _, ok := world.sets[type2]; !ok do return {}
    if _, ok := world.sets[type3]; !ok do return {}

    hash := types_hash(type1, type2, type3)
    if group, exists := &world.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { world.sets[type1], world.sets[type2], world.sets[type3] }
    if !declare_group(world, group, sets[:]) { free(group); return nil }
    group.hash = hash
    world.groups[hash] = group^
    return group
}

get_or_declare_group_4 :: proc(world: ^world_t, $type1, $type2, $type3, $type4: typeid) -> ^group_t {
    if _, ok := world.sets[type1]; !ok do return {}
    if _, ok := world.sets[type2]; !ok do return {}
    if _, ok := world.sets[type3]; !ok do return {}
    if _, ok := world.sets[type4]; !ok do return {}

    hash := types_hash(type1, type2, type3, type4)
    if group, exists := &world.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { world.sets[type1], world.sets[type2], world.sets[type3], world.sets[type4] }
    if !declare_group(world, group, sets[:]) { free(group); return nil }
    group.hash = hash
    world.groups[hash] = group^
    return group
}
get_or_declare_group :: proc { get_or_declare_group_2, get_or_declare_group_3, get_or_declare_group_4 }

declare_group :: proc(world: ^world_t, new_group: ^group_t, sets: []set_t) -> bool {
    new_group.components = bit_array.create(len(world.sets))^
    for set in sets do bit_array.set(&new_group.components, set.id)

    count : u32 = 0
    group_entities: [dynamic]entity_t
    defer delete(group_entities)
    for e in world.entities {
        if entity_is_valid(world, e) && has_all_components(world, e, &new_group.components) {
            append(&group_entities, e)
            count += 1
        }
    }
    if count == 0 do return false

    // believe all of this might be fucked
    to_sort := make([dynamic]set_t)
    defer delete(to_sort)
    for set in sets {
        max, min : u32 = 1, max(u32)
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
        min := max(u32)
        for e in group_entities do if set.indices[e] < min do min = set.indices[e]
        new_group.range[set.type] = range_t { min, count }
    }
    return true
}
