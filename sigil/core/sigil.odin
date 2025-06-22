
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
}

group_t :: struct {
    entities   : [dynamic]entity_t,
    components : ^bit_array.Bit_Array,
    type_range : [dynamic]type_range_t,
}

type_range_t :: struct {
    type   : typeid,
    offset : int,
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

name :: distinct string

// todo: implement an actual scheduler for this, but might not be needed tbh

module_create_info_t :: struct {
    name: name,
    id  : entity_t,
}
module :: #type proc(entity_t) -> typeid
init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

relation :: struct($topic, $target: typeid) { target, target }
before :: struct(fn: proc()) { using fn }
after  :: struct(fn: proc()) { using fn }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

request_exit : bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(init) _init :: proc() {
    core = {}
    /* INVALID */ new_entity()
    /*  WORLD  */ new_entity()
    add(WORLD,  name("world"))
}

use :: #force_inline proc(setup: module) { e := new_entity(); add(e, setup(e)) }

schedule :: #force_inline proc(e: entity_t, fn: $type) where intrinsics.type_is_proc(type) {
    // add require param to schedule proc depending on dependant modules
    // as in: before(module), after(other_module)
    add(e, type(fn))

    // later note: relations would be good to solve for all entities, then modules could just be that
    // example: before(owner, module)
    relation :: struct($topic, $target: typeid) { target, target }
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
                maintain_group_storage(&group)
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
    $t1: typeid,
    $t2: typeid
) -> #soa[]struct {
    x: t1,
    y: t2
} {
    g := get_or_declare_group(t1, t2)
    if g == nil do return {}
    slice1 := get_component_slice(g, t1)
    slice2 := get_component_slice(g, t2)
    return soa_zip(slice1, slice2)
}

query_3 :: #force_inline proc(
    $t1: typeid,
    $t2: typeid,
    $t3: typeid
) -> #soa[]struct {
    x: t1,
    y: t2,
    z: t3
} {
    g := get_or_declare_group(t1, t2, t3)
    if g == nil do return {}
    slice1 := get_component_slice(g, t1)
    slice2 := get_component_slice(g, t2)
    slice3 := get_component_slice(g, t3)
    return soa_zip(slice1, slice2, slice3)
}

query_4 :: #force_inline proc(
    $t1: typeid,
    $t2: typeid,
    $t3: typeid,
    $t4: typeid
) -> #soa[]struct {
    x: t1,
    y: t2,
    z: t3,
    w: t4
} {
    g := get_or_declare_group(t1, t2, t3, t4)
    if g == nil do return {}
    slice1 := get_component_slice(g, t1)
    slice2 := get_component_slice(g, t2)
    slice3 := get_component_slice(g, t3)
    slice4 := get_component_slice(g, t4)
    return soa_zip(slice1, slice2, slice3, slice4)
}

query :: proc { query_1, query_2, query_3, query_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

types_hash :: proc(types: ..typeid) -> (h: u64) { for t in types do h ~= (^u64)(raw_data(mem.any_to_bytes(t)))^; return }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

get_or_declare_group_2 :: proc($t1, $t2: typeid) -> ^group_t {
    if _, ok := core.sets[t1]; !ok do return {}
    if _, ok := core.sets[t2]; !ok do return {}

    hash := types_hash(t1, t2)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !declare_group(group, t1, t2) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}

get_or_declare_group_3 :: proc($t1, $t2, $t3: typeid) -> ^group_t {
    if _, ok := core.sets[t1]; !ok do return {}
    if _, ok := core.sets[t2]; !ok do return {}
    if _, ok := core.sets[t3]; !ok do return {}

    hash := types_hash(t1, t2, t3)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !declare_group(group, t1, t2, t3) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}

get_or_declare_group_4 :: proc($type1, $type2, $type3, $type4: typeid) -> ^group_t {
    if _, ok := core.sets[t1]; !ok do return {}
    if _, ok := core.sets[t2]; !ok do return {}
    if _, ok := core.sets[t3]; !ok do return {}
    if _, ok := core.sets[t4]; !ok do return {}

    hash := types_hash(type1, type2, type3, type4)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !declare_group(group, type1, type2, type3, type4) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}
get_or_declare_group :: proc { get_or_declare_group_2, get_or_declare_group_3, get_or_declare_group_4 }

declare_group_2 :: proc(group: ^group_t, $type1, $type2: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[type1].id)
    bit_array.set(group.components, core.sets[type2].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    set := core.sets[type1].count < core.sets[type2].count ? core.sets[type1] : core.sets[type2]
    //if len(set.indices) < len(core.entities) do resize(&set.indices, len(core.entities) + 1)
    sort_entities(group.entities[:], set.indices[:])
    add_group_type(group, type1)
    add_group_type(group, type2)
    return true
}

declare_group_3 :: proc(group: ^group_t, $type1, $type2, $type3: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[type1].id)
    bit_array.set(group.components, core.sets[type2].id)
    bit_array.set(group.components, core.sets[type3].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    set := core.sets[type1].count < core.sets[type2].count ? core.sets[type1] : core.sets[type2]
    set = set.count < core.sets[type3].count ? set : core.sets[type3]
    //if len(set.indices) < len(core.entities) do resize(&set.indices, len(core.entities) + 1)
    sort_entities(group.entities[:], set.indices[:])
    add_group_type(group, type1)
    add_group_type(group, type2)
    add_group_type(group, type3)
    return true
}

declare_group_4 :: proc(group: ^group_t, $type1, $type2, $type3, $type4: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[type1].id)
    bit_array.set(group.components, core.sets[type2].id)
    bit_array.set(group.components, core.sets[type3].id)
    bit_array.set(group.components, core.sets[type4].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    set := core.sets[type1].count < core.sets[type2].count ? core.sets[type1] : core.sets[type2]
    set  = set.count < core.sets[type3].count ? set : core.sets[type3]
    set  = set.count < core.sets[type4].count ? set : core.sets[type4]
    //if len(set.indices) < len(core.entities) do resize(&set.indices, len(core.entities) + 1)
    sort_entities(group.entities[:], set.indices[:])
    add_group_type(group, type1)
    add_group_type(group, type2)
    add_group_type(group, type3)
    add_group_type(group, type4)
    return true
}

declare_group :: proc { declare_group_2, declare_group_3, declare_group_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

add_group_type :: proc(group: ^group_t, $type: typeid) {
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    entities := group.entities[:]
    required := len(entities) - 1
    start := 1

    // find start
    min := start + required
    if len(data) < min do resize(data, min)
    for {
        free := true
        for check := start; check < start + required; check += 1 {
            for e in entities do if set.indices[e] == check { free = false; break }
            if !free do break
        }
        if free do break
        start += 1
        if start + required > len(data) do resize(data, start + required)
    }

    info := type_range_t { type = type }
    info.offset = start

    // defrag
    required = info.offset + len(entities)
    if len(data) < required do resize(data, required)
    temp := make([]type, len(entities))
    for e, i in entities do temp[i] = data[set.indices[e]]
    for e, i in entities {
        data[info.offset + i] = temp[i]
        set.indices[e] = info.offset + i
    }
    delete(temp)

    append(&group.type_range, info)
}

maintain_group_storage :: proc(group: ^group_t) {
    if len(group.entities) == 0 do return
    primary_type := group.type_range[0].type
    primary_set := core.sets[primary_type]
    sort_entities(group.entities[:], primary_set.indices[:])
    for info in &group.type_range {
        t := info.type
        set := core.sets[t]
        component_size := runtime.type_info_base(type_info_of(t)).size
        required_size := info.offset + len(group.entities)
        if len((^[dynamic]byte)(set.components)^) < required_size {
            resize((^[dynamic]byte)(set.components), required_size)
        }
        temp := make([dynamic]byte, len(group.entities) * component_size)
        defer delete(temp)
        for e, i in group.entities {
            src := rawptr(uintptr(set.components) + uintptr(set.indices[e] * component_size))
            dst := rawptr(uintptr(raw_data(temp)) + uintptr(i * component_size))
            mem.copy(dst, src, component_size)
        }
        for e, i in group.entities {
            new_idx := info.offset + i
            dst := rawptr(uintptr(set.components) + uintptr(new_idx * component_size))
            src := rawptr(uintptr(raw_data(temp)) + uintptr(i * component_size))
            mem.copy(dst, src, component_size)
            if info.type == primary_type do for type, set in core.sets do if has_component(e, type) {
                set.indices[e] = new_idx
            }
        }
    }
}

get_component_slice :: proc(g: ^group_t, $t: typeid) -> []t {
    set := core.sets[t]
    for info in g.type_range do if info.type == t {
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

sort_entities :: proc(entities: []entity_t, indices: []int) {
    if len(entities) <= 1 do return
    pivot := indices[entities[len(entities) / 2]]
    left, right := 0, len(entities) - 1

    for left <= right {
        for indices[entities[left]] < pivot  do left += 1
        for indices[entities[right]] > pivot do right -= 1
        if left <= right {
            entities[left], entities[right] = entities[right], entities[left]
            left += 1; right -= 1
        }
    }
    sort_entities(entities[:right+1], indices)
    sort_entities(entities[left:], indices)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

