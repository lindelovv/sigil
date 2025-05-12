
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

//scheduler :: struct {
//    test: proc()
//}

name     :: distinct string

module_create_info :: struct {
    name: name,
    id  : entity_t,
}
module :: #type proc(entity_t) -> typeid // redundant ? maybe remake to stuct($t) { using t } for getting
init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

relation :: struct($topic, $target: typeid) { target, target }
before :: struct(fn: proc()) { using fn }
after  :: struct(fn: proc()) { using fn }

request_exit : bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(init) _init :: proc() {
    core = {}
    /* INVALID */ new_entity() // why did this & WORLD = 1 break everything ?
    /*  WORLD  */ new_entity()
    //add(WORLD, core)
    add(WORLD, name("world"))
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
    //test()
    for fn in query(init) { fn() }
    main_loop: for !request_exit { for fn in query(tick) { fn() } }
    for fn in query(exit) { fn() }
}

@(disabled=!ODIN_DEBUG)
test :: proc() {
    fmt.println("__run__")
    e := new_entity()
    //fmt.println("--- expecting  e", e)
    add(e, name("name1"))
    add(e, []name{"name1"})
    add(e, f32(e))
    add(e, int(e))
    add(e, u8(e))

    e2 := new_entity()
    //fmt.println("--- expecting  e", e2)
    add(e2, name("name2"))
    add(e2, name("name2 -"))
    add(e2, []name{"name2"})
    add(e2, []name{"name2 -"})
    add(e2, []name{"n"})
    add(e2, f32(e2))
    add(e2, int(e2))
    add(e2, u8(e2))

    e3 := new_entity()
    //fmt.println("--- expecting  e", e3)
    add(e3, name("name3"))
    add(e3, name("name3 -"))
    add(e3, []name{"name3"})
    add(e3, []name{"name3 -", "n"})
    add(e3, f32(e3))
    add(e3, int(e3))
    add(e3, u8(e3))

    //fmt.println("")
    add(e, e3)
    add(e2, e)
    add(e3, e2)

    index := 0
    fmt.printfln("\nname: %v", query(name))

    for soa, _ in query(int, f32, name) {
        // make actual iterator to get expanded values directly + entity id
        i, f, h := expand_values(soa)
        // since this is not giving any info about entity id it is way less useful than it should be
        fmt.println("int:", i, "-- f32:", f, "-- name:", h)
        index += 1
    }

    //for i, f in query(int, f32) {
    //    fmt.printfln("i %d, f %d", i, f)
    //}

    u := new_entity()
    f := new_entity()
    add(u, int(15))
    add(u, f32(1.5))
    add(f, f32(1.6))
    fmt.printfln("u has int: %v, f has int: %v", has_component(u, int), has_component(f, int))
    fmt.printfln("u has int: %v, f has name: %v", has_component(u, int), has_component(f, name))
    h := new_entity()
    add(f, int(16))
    add(h, f32(1.7))
    add(h, int(1))

    //fmt.printfln("e has int with value: %v", get_component(e, int))
    //fmt.printfln("%v", query(int))
    //fmt.printfln("f has int with value: %v", get_component(f, int)^)
    fmt.printfln("h has int with value: %v", get_value(h, int))

    m, ok := get_ref(f, int); if ok {
        m^ = 55
    }

    fmt.printfln("f has int with value: %v", get_value(f, int))

    //sort(f32, int)
    //add(e, relation(int, f32){})
    //add(f, relation(name, u8){ {}, 2 })
    //for r in query(relation(name, u8)) {
    //    fmt.printfln("relation %v", r)
    //}
    // would be nice to have ability to query all relatios with one specific qualifier for example
    // would require a lot of custom logic for that case it seems

    for e in core.entities {
        if len(core.flags) >= int(e) {
            if has_component(e, name) do fmt.printfln("e (%v) len %v -- %v", e, core.flags[e].length, get_value(e, name))
            else do fmt.printfln("e (%v) len %v", e, core.flags[e].length)
        }
    }

    ggg :: distinct int
    ccc :: distinct int
    add(f, ggg(16))
    add(h, ccc(17))

    hash1 := types_hash(int, name, f32)
    hash2 := types_hash(name, f32)
    hash3 := types_hash(f32, name)
    fmt.printfln("hash1: %v", hash1)
    fmt.printfln("hash2: %v", hash2)
    fmt.printfln("hash2: %v", hash3)

    g := get_or_create_group(int, f32)
    fmt.printfln("group: %v", g)
    
    fmt.println("---------------")
    fmt.printfln("%#v", query(int, f32, name))
    fmt.println("---------------")
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

new_entity :: #force_inline proc() -> entity_t {
    e := entity_t(len(core.entities))
    append(&core.entities, e)
    append(&core.flags, bit_array.create(len(core.sets)))
    return e
}

get_entity :: #force_inline proc(entity: entity_t) -> entity_t {
    // breaking now with auto creation of entity, need fix
    //if len(core.entities) < int(entity + 1) { return new_entity() }
    for e in core.entities { if e == entity do return e }
    return 0
}

has_component :: proc(entity: entity_t, type: typeid) -> bool {
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

    update_group_after_addition(e, type)
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
    g := get_or_create_group(t1, t2)
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
    g := get_or_create_group(t1, t2, t3)
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
    g := get_or_create_group(t1, t2, t3, t4)
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

get_or_create_group_2 :: proc($T1, $T2: typeid) -> ^group_t {
    hash := types_hash(T1, T2)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !create_group(group, T1, T2) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}

get_or_create_group_3 :: proc($T1, $T2, $T3: typeid) -> ^group_t {
    hash := types_hash(T1, T2, T3)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !create_group(group, T1, T2, T3) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}

get_or_create_group_4 :: proc($T1, $T2, $T3, $T4: typeid) -> ^group_t {
    hash := types_hash(T1, T2, T3, T4)
    if group, exists := &core.groups[hash]; exists do return group
    group := new(group_t)
    if !create_group(group, T1, T2, T3, T4) { free(group); return nil }
    maintain_group_storage(group)
    core.groups[hash] = group^
    return group
}
get_or_create_group :: proc { get_or_create_group_2, get_or_create_group_3, get_or_create_group_4 }

create_group_2 :: proc(group: ^group_t, $T1, $T2: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[T1].id)
    bit_array.set(group.components, core.sets[T2].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    primary_set := core.sets[T1].count < core.sets[T2].count ? core.sets[T1] : core.sets[T2]
    sort_entities(group.entities[:], primary_set.indices[:])
    add_type_info(group, T1)
    add_type_info(group, T2)
    return true
}

create_group_3 :: proc(group: ^group_t, $T1, $T2, $T3: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[T1].id)
    bit_array.set(group.components, core.sets[T2].id)
    bit_array.set(group.components, core.sets[T3].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    primary_set := core.sets[T1].count < core.sets[T2].count ? core.sets[T1] : core.sets[T2]
    primary_set  = primary_set.count < core.sets[T3].count ? primary_set : core.sets[T3]
    sort_entities(group.entities[:], primary_set.indices[:])
    add_type_info(group, T1)
    add_type_info(group, T2)
    add_type_info(group, T3)
    return true
}

create_group_4 :: proc(group: ^group_t, $T1, $T2, $T3, $T4: typeid) -> bool {
    group.components = bit_array.create(len(core.sets))
    bit_array.set(group.components, core.sets[T1].id)
    bit_array.set(group.components, core.sets[T2].id)
    bit_array.set(group.components, core.sets[T3].id)
    bit_array.set(group.components, core.sets[T4].id)
    for e in core.entities do if has_all_components(e, group.components) {
        append(&group.entities, e)
    }
    if len(group.entities) == 0 do return false
    primary_set := core.sets[T1].count < core.sets[T2].count ? core.sets[T1] : core.sets[T2]
    primary_set  = primary_set.count < core.sets[T3].count ? primary_set : core.sets[T3]
    primary_set  = primary_set.count < core.sets[T4].count ? primary_set : core.sets[T4]
    sort_entities(group.entities[:], primary_set.indices[:])
    add_type_info(group, T1)
    add_type_info(group, T2)
    add_type_info(group, T3)
    add_type_info(group, T4)
    return true
}

create_group :: proc { create_group_2, create_group_3, create_group_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

add_type_info :: proc(group: ^group_t, $type: typeid) {
    set := core.sets[type]
    info := type_range_t{ type = type }
    info.offset = find_contiguous_start(type, group.entities[:])
    defragment_components(type, group.entities[:], info.offset)
    append(&group.type_range, info)
}

defragment_components :: proc($T: typeid, entities: []entity_t, offset: int) {
    set := core.sets[T]
    data := cast(^[dynamic]T)(set.components)
    required := offset + len(entities)
    if len(data^) < required do resize(data, required)
    temp := make([]T, len(entities))
    for e, i in entities do temp[i] = data[set.indices[e]]
    for e, i in entities {
        data[offset + i] = temp[i]
        set.indices[e] = offset + i
    }
    delete(temp)
}

update_group_indices :: proc(group: ^group_t) {
    primary_type := group.type_range[0].type
    primary_set := core.sets[primary_type]
    sort_entities(group.entities[:], primary_set.indices[:])
    for info in group.type_range {
        t := info.type
        set := core.sets[t]
        for e, i in group.entities do set.indices[e] = info.offset + i
    }
}

update_group_after_addition :: proc(entity: entity_t, added_type: typeid) {
    for hash, &group in &core.groups {
        if bit_array.get(group.components, core.sets[added_type].id) && has_all_components(entity, group.components) {
            if !slice.contains(group.entities[:], entity) {
                append(&group.entities, entity)
                maintain_group_storage(&group)
            }
        }
    }
}

update_entity_indices :: proc(entity: entity_t, new_index: int) {
    for type, set in core.sets do if has_component(entity, type) {
        set.indices[entity] = new_index
    }
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
            if info.type == primary_type do update_entity_indices(e, new_idx)
        }
    }
}

verify_component_contiguity :: proc(g: ^group_t) -> bool {
    for info in g.type_range {
        t := info.type
        set := core.sets[t]
        start := info.offset
        for e, i in g.entities do if set.indices[e] != start + i do return false
    }
    return true
}

find_contiguous_start :: proc($T: typeid, entities: []entity_t) -> int {
    set := core.sets[T]
    component_array := cast(^[dynamic]T)(set.components)
    required := len(entities) - 1
    candidate := 1

    min_needed := candidate + required
    if len(component_array^) < min_needed do resize(component_array, min_needed)
    for {
        free := true
        for check := candidate; check < candidate + required; check += 1 {
            for e in entities do if set.indices[e] == check { free = false; break }
            if !free do break
        }
        if free do return candidate
        candidate += 1
        if candidate + required > len(component_array^) do resize(component_array, candidate + required)
    }
}

get_component_slice :: proc(g: ^group_t, $T: typeid) -> []T {
    set := core.sets[T]
    for info in g.type_range do if info.type == T {
        data := cast(^[dynamic]T)(set.components)
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
    pivot := indices[entities[len(entities)/2]]
    left, right := 0, len(entities)-1

    for left <= right {
        for indices[entities[left]] < pivot do left += 1
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

