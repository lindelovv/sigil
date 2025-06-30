
package __sigil

import "core:fmt"
import "core:mem"
import "core:slice"
import "core:math"
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

// not used as of now
HAS_AVX2  :: #config(HAS_AVX2, ODIN_ARCH == .amd64 && false)
HAS_SSE4  :: #config(HAS_SSE4, ODIN_ARCH == .amd64 && false)
HAS_NEON  :: #config(HAS_NEON, ODIN_ARCH == .arm64)

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

entity_t :: distinct int

WORLD    :: entity_t(1)
INVALID  :: entity_t(0)

core: struct {
    entities : [dynamic]entity_t,
    flags    : [dynamic]^bit_array.Bit_Array,
    sets     : map[typeid]set_t,
    groups   : map[u64]group_t,
}

set_t :: struct {
    components : rawptr,
    indices    : [dynamic]int,
    count      : int,
    id         : int,
    type       : typeid,
}

group_t :: struct {
    entities   : [dynamic]entity_t,
    components : ^bit_array.Bit_Array,
    range      : map[typeid]range_t,
}

range_t :: struct {
    offset : int,
    count  : int,
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

name_t :: distinct string

// todo: implement an actual scheduler for this, but might not be needed tbh

module_create_info_t :: struct {
    name: name_t,
    id  : entity_t,
}
module :: #type proc(entity_t) -> typeid
init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

// later note: relations would be good to solve for all entities, then modules could just be that
// example: before(owner, module)
relation :: struct($topic, $target: typeid) { topic, target }

tag_t :: distinct struct {} // maybe use?

before     :: distinct []proc()
after      :: distinct []proc()

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

request_exit : bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(init) _init :: proc() {
    core = {}
    /* INVALID */ new_entity()
    /*  WORLD  */ new_entity()
    add(WORLD,  name_t("world"))
}

use :: #force_inline proc(setup: module) { e := new_entity(); add(e, setup(e)) }

schedule :: #force_inline proc(e: entity_t, fn: $type, conditions: []union { before, after } = nil) where intrinsics.type_is_proc(type) {
    // add require param to schedule proc depending on dependant modules
    // as in: before(module), after(other_module)
    // update: condition union with before and after arrays of proc addresses
    add(e, type(fn))
    for type in conditions do switch condition in type {
        case before: for fn in condition {}
        case after:  for fn in condition {}
    }
}

run :: #force_inline proc() {
    for fn in query(init) { fn() }
    main_loop: for !request_exit { for fn in query(tick) { fn() }; free_all(context.temp_allocator) }
    for fn in query(exit) { fn() }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

new_entity :: #force_inline proc() -> entity_t {
    e := entity_t(len(core.entities))
    append(&core.entities, e)
    append(&core.flags, bit_array.create(len(core.sets)))
    return e
}

get_entity :: #force_inline proc(entity: entity_t) -> entity_t {
    for e in core.entities { if e == entity do return e }
    return 0
}

is_valid_entity :: proc(entity: entity_t) -> bool {
    return entity != INVALID
}

has_component :: #force_inline proc(entity: entity_t, type: typeid) -> bool {
    e := get_entity(entity)
    if e == 0 || int(e) >= len(core.flags) || core.sets[type].components == nil || core.flags[e] == nil {
        return false
    }
    return bit_array.get(core.flags[e], core.sets[type].id)
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

add_to_world :: #force_inline proc(component: $type) -> type { return add_to_entity(WORLD, component) }
add_to_entity :: #force_inline proc(to: entity_t, component: $type) -> type {
    if type == typeid_of(none) do return {}
    e := get_entity(to)
    if e == 0 do return {}

    set := core.sets[type]
    if exists := &set.components; exists == nil || set.count == 0 {
        set.components = new([dynamic]type)
        append((^[dynamic]type)(set.components), type{}) // dummy for invalid
        set.id = len(core.sets)
        set.type = type
    }

    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(e) do resize_dynamic_array(&set.indices, len(core.entities) + 1)

    if len(data) <= int(e) || len(data) <= set.indices[e] {
        append(data, component)
        set.indices[e] = len(data) - 1
        set.count += 1
    } else {
        data[set.indices[e]] = component
    }
    core.sets[type] = set

    if len(core.flags) <= int(e) do resize(&core.flags, len(core.entities) + 1)
    if core.flags[e] == nil do core.flags[e] = bit_array.create(len(core.entities))
    bit_array.set(core.flags[e], set.id)

    //fmt.printfln("adding %v to %v", typeid_of(type), e)

    for hash, &group in &core.groups {
        if bit_array.get(group.components, core.sets[type].id) && has_all_components(e, group.components) {
            if !slice.contains(group.entities[:], e) {
                append(&group.entities, e)
            }
        }
    }
    return component
}
add :: proc { add_to_world, add_to_entity }

remove_component :: proc(entity: entity_t, $type: typeid) {
    if !has_component(entity, type) do return
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    data[idx] = nil
    set.indices[entity] = 0
    set.count -= 1
    if idx < set.first_free do set.first_free = idx
    bit_array.clear(core.flags[entity], set.id)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

query_1 :: #force_inline proc($type: typeid) -> []type {
    if core.sets[type].components == nil do return nil
    data := (cast(^[dynamic]type)(core.sets[type].components))[1:][:core.sets[type].count]
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
    slice1 := get_component_slice(g, type1)
    slice2 := get_component_slice(g, type2)
    return soa_zip(slice1, slice2)
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
    slice1 := get_component_slice(g, type1)
    slice2 := get_component_slice(g, type2)
    slice3 := get_component_slice(g, type3)
    //fmt.printfln("soa %v %v %v \n %#v", typeid_of(type1), typeid_of(type2), typeid_of(type3), soa_zip(slice1, slice2, slice3))
    return soa_zip(slice1, slice2, slice3)
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
    slice1 := get_component_slice(g, type1)
    slice2 := get_component_slice(g, type2)
    slice3 := get_component_slice(g, type3)
    slice4 := get_component_slice(g, type4)
    return soa_zip(slice1, slice2, slice3, slice4)
}

query :: proc { query_1, query_2, query_3, query_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

types_hash :: proc(types: ..typeid) -> (h: u64) { for t in types do h ~= (^u64)(raw_data(mem.any_to_bytes(t)))^; return }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

get_or_declare_group_2 :: proc($type1, $type2: typeid) -> ^group_t {
    if _, ok := core.sets[type1]; !ok do return {}
    if _, ok := core.sets[type2]; !ok do return {}

    hash := types_hash(type1, type2)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    sets : []set_t = { core.sets[type1], core.sets[type2] }
    if !declare_group(group, sets[:]) { free(group); return nil }
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
    core.groups[hash] = group^
    return group
}
get_or_declare_group :: proc { get_or_declare_group_2, get_or_declare_group_3, get_or_declare_group_4 }

declare_group :: proc(new_group: ^group_t, sets: []set_t) -> bool {
    new_group.components = bit_array.create(len(core.sets))
    for set in sets {
        bit_array.set(new_group.components, set.id)
    }
    
    count := 0
    for e in core.entities {
        if is_valid_entity(e) && has_all_components(e, new_group.components) {
            append(&new_group.entities, e)
            count += 1
        }
    }
    if count == 0 do return false

    to_sort := make([dynamic]set_t)
    for set in sets {
        max, min := 1, max(int)
        for e in new_group.entities {
            if set.indices[e] < min && set.indices[e] != 0 do min = set.indices[e]
            if set.indices[e] > max do max = set.indices[e]
            //fmt.println(max, min)
        }
        if max - count == min { // already in ok order (not thinking about other groups)
            fmt.println(max, count, min, max - count)
            new_group.range[set.type] = range_t { min, count }
            fmt.println(min, max, set.type, new_group.range[set.type])
            fmt.println(set.indices)
        } else {
            append(&to_sort, set)
        }
    }
    // broken as shit
    //swap :: proc(arr: rawptr, a, b, size: int) {
    //    if a == b do return
    //    ptr_a := rawptr(uintptr(arr) + (uintptr(a) * uintptr(size)))
    //    ptr_b := rawptr(uintptr(arr) + (uintptr(b) * uintptr(size)))
    //    tmp := make([]byte, size)
    //    defer delete(tmp)
    //    mem.copy(raw_data(tmp), ptr_a, size)
    //    mem.copy(ptr_a, ptr_b, size)
    //    mem.copy(ptr_b, raw_data(tmp), size)
    //}
    for set in to_sort {
        min := max(int)
        for e in new_group.entities {
            if set.indices[e] == 0 do fmt.println("invalid")
            if set.indices[e] < min do min = set.indices[e]
        }
        //for e, i in new_group.entities {
        //    if set.indices[e] != i {
        //        if len(set.indices) < i + 1 do continue
        //        set.indices[i + 1], set.indices[i] = set.indices[i], set.indices[i + 1]
        //        swap(set.components, i, i + 1, set.sizeof_type)
        //        //set.components[i + 1], set.indices[i] = set.indices[i], set.indices[i + 1]
        //    }
        //}
        new_group.range[set.type] = range_t { min, count }
    }
    //fmt.printfln("%#v", new_group.range)

    //compare_t :: struct {
    //    group   : group_t,
    //    type    : typeid,
    //    overlap : [2]int,
    //}
    //overlap := make([dynamic]compare_t)
    //for hash, group in core.groups do for set in sets {
    //    if bit_array.get(new_group.components, set.id) {
    //        range := group.range[set.type]
    //        append(&overlap, compare_t { group, set.type, { range.offset, range.count } })
    //        fmt.println(set.type, range.offset, range.count)
    //    }
    //}
    //for group in overlap {
    //    // figure out how to handle sorting these sections, who should take president
    //}

    return true
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

get_component_slice :: proc(g: ^group_t, $t: typeid) -> []t {
    set := core.sets[t]
    if info, exists := g.range[t]; exists {
        data := cast(^[dynamic]t)(set.components)
        start := info.offset
        end := start + len(g.entities)
        if end > len(data^) do return nil
        return data[start:end]
    }
    return nil
}

has_all_components :: proc(entity: entity_t, mask: ^bit_array.Bit_Array) -> bool {
    if entity <= 0 || int(entity) >= len(core.flags) || core.flags[entity] == nil do return false
    if entity <= 0 || int(entity) >= len(core.flags) do return false
    if core.flags[entity] == nil do return false
    
    for i in 0..<len(mask.bits) {
        if mask.bits[i] == 0 do continue
        if (core.flags[entity].bits[i] & mask.bits[i]) != mask.bits[i] do return false
    }
    return true
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

