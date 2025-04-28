#+feature dynamic-literals

package __sigil

import "core:fmt"
import "core:mem"
import "core:slice"
import "base:intrinsics"
import "base:runtime"

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

TITLE   :: "__sigil_"
MAJOR_V :: 0
MINOR_V :: 0
PATCH_V :: 2

entity_t :: distinct int
name :: distinct string

@(private="file") core: struct {
    entities: [dynamic]entity_t,
    next, max_id: entity_t,
    flags: [dynamic]map[typeid]bool,
    sets: map[typeid]struct {
        indices    : [dynamic]int,
        components : rawptr,
        count      : int,
    }
}

scheduler :: struct {
    test: proc()
}

module :: #type proc()
init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

request_exit : bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(init) _init :: proc() {
    new_entity()
    add :: proc(value: $type) -> type {
        e := 0
        //fmt.printfln("=== adding %v [%v] to entity %d", typeid_of(type), value, e)
        if e == -1 do return value

        data := core.sets[type]
        if exists := &data.components; exists == nil || data.count == 0 {
            data.components = new([dynamic]type)
        }
        i, _ := append_elem(cast(^[dynamic]type)(data.components), value)
        resize_dynamic_array(&data.indices, e + 1)
        data.indices[e] = i
        data.count += 1
        core.sets[type] = data
        return value
    }
    add(core)
    //add(name("world"))
}

use :: #force_inline proc(setup: module) { setup() }

schedule :: #force_inline proc(fn: $type) where intrinsics.type_is_proc(type) {
    // add require param to schedule proc depending on dependant modules
    // as in: before(module), after(other_module)
    add_component(type(fn))
}

run :: #force_inline proc() {
    test()

    for fn in query(init) { fn() }
    main_loop: for !request_exit { for fn in query(tick) { fn() } }
    for fn in query(exit) { fn() }
}

@(disabled=!ODIN_DEBUG)
test :: proc() {
    fmt.println("__run__")
    e := new_entity()
    //fmt.println("--- expecting  e", e)
    add_component(e, name("name1"))
    add_component(e, []name{"name1"})
    add_component(e, f32(e))
    add_component(e, int(e))
    add_component(e, u8(e))

    e2 := new_entity()
    //fmt.println("--- expecting  e", e2)
    add_component(e2, name("name2"))
    add_component(e2, name("name2 -"))
    add_component(e2, []name{"name2"})
    add_component(e2, []name{"name2 -"})
    add_component(e2, []name{"n"})
    add_component(e2, f32(e2))
    add_component(e2, int(e2))
    add_component(e2, u8(e2))

    e3 := new_entity()
    //fmt.println("--- expecting  e", e3)
    add_component(e3, name("name3"))
    add_component(e3, name("name3 -"))
    add_component(e3, []name{"name3"})
    add_component(e3, []name{"name3 -", "n"})
    add_component(e3, f32(e3))
    add_component(e3, int(e3))
    add_component(e3, u8(e3))

    //fmt.println("")
    add_component(e, e3)
    add_component(e2, e)
    add_component(e3, e2)

    //fmt.println("")
    index := 0
    //fmt.println(query(name))
    //s, en := q(name)
    //for i in 0..<len(s) {
    //    //fmt.printfln("%v - %v", en[i], s[i])
    //}
    //for i in query([]name) do fmt.printfln("[]name: %v", i)

    fmt.printfln("\nname: %v", query(name))

    for soa, _ in query(int, f32, name) {
        // make actual iterator to get expanded values directly + entity id
        i, f, h := expand_values(soa)
        // since this is not giving any info about entity id it is way less useful than it should be
        //fmt.println("int:", i, "-- f32:", f, "-- name:", h)
        index += 1
    }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

new_entity :: #force_inline proc() -> entity_t {
    idx := len(core.entities)
    //fmt.println("--- new        e", idx); 
    if int(core.next) <= idx || core.next == 0 {
        // change resize value to a more reasonable size
        resize_dynamic_array(&core.entities, idx + 1)
        core.entities[idx] = core.next
        core.next += 1
        core.max_id += 1
        //fmt.println("resize e:", core.entities[idx])
        return core.entities[idx]
    } else {
        core.entities[core.next] = core.max_id 
        core.max_id += 1
        e := core.entities[core.next]
        core.next = 0
        //fmt.println("same e:", e)
        return e
    }
}

get_entity :: #force_inline proc(entity: entity_t) -> entity_t {
    // breaking now with auto creation of entity, need fix
    if len(core.entities) < int(entity + 1) { return new_entity() }
    for e, _ in core.entities { if e == entity { return e } }
    return -1
}

@(private="file") add_component_world :: #force_inline proc(value: $type) -> type { return add_component_entity(0, value) }

@(private="file")
add_component_entity :: #force_inline proc(entity: entity_t, value: $type) -> type {
    e := get_entity(entity)
    if e == -1 do return {}

    set := core.sets[type]
    if exists := &set.components; exists == nil || set.count == 0 {
        set.components = new([dynamic]type)
    }

    data := cast(^[dynamic]type)(set.components)
    if type == name {
        fmt.println()
        fmt.printfln("e == 0 \t\t\t- %v", e == 0)
        fmt.printfln("len(data) < int(e)) \t- %v", len(data) <= int(e))
        fmt.printfln("len(set.indices) < len(core.entities) \t- %v", len(set.indices) < len(core.entities))
    }
    if len(set.indices) < len(core.entities) do resize_dynamic_array(&set.indices, len(core.entities))
    if type == name do fmt.printfln("len(data) <= set.indices[e] \t- %v", len(data) <= set.indices[e])
    if e == 0 || len(data) <= int(e) || len(data) <= set.indices[e] {
        append(data, value)
        set.indices[e] = len(data)
        //fmt.printfln("add\t- en: %v\t- %v - ind: %v\t- %v: %v (%v)", e, typeid_of(type), set.indices[i - 1], typeid_of(type), value, i)
        set.count += 1

        i := len(data)
        ii := len(set.indices)
        if type == name do fmt.printfln("adding %v [%v] to entity %d [len: %v, ind len: %v]", typeid_of(type), value, e, i, ii)
    } else {
        //fmt.printfln("change %v [%v] to [%v] on entity %v", typeid_of(type), data[e], value, e)
        data[set.indices[e]] = value
    }
    core.sets[type] = set

    //if len(core.flags) < int(e + 1) { 
    //    resize_dynamic_array(&core.flags, e + 1)
    //}
    //if t, exists := &core.flags[e][type]; exists {
    //    t^ = true
    //}
    return value
}

add_component :: proc { add_component_world, add_component_entity }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

//sort_2 :: #force_inline proc($t1: typeid, $t2: typeid) {
//    set1 := core.sets[t1]
//    set2 := core.sets[t2]
//    len := len(set1.indices) > len(set2.indices) ? len(set1.indices) : len(set2.indices)
//
//    fmt.printfln("checking %v & %v", type_info_of(t1), type_info_of(t2))
//
//    for i := 0; i < len; i += 1 {
//        if core.entities[set1.indices[i]] != core.entities[set2.indices[i]] {
//            fmt.printfln("missmatch @ %d", i)
//        } else {
//            fmt.printfln("all good @ %d", i)
//        }
//    }
//}
//
//sort_3 :: #force_inline proc(v1: $t1, v2: $t2, v3: $t3) {
//
//}
//
//sort_4 :: #force_inline proc(v1: $t1, v2: $t2, v3: $t3, v4: $t4) {
//
//}
//
//sort :: proc { sort_2, sort_3, sort_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

//q :: #force_inline proc($type: typeid) -> ([]type, []entity_t) {
//    if core.sets[type].components == nil do return nil, nil
//    set := (cast(^[dynamic]type)(core.sets[type].components))[:core.sets[type].count]
//    indices := core.sets[type].indices
//    owners: [dynamic]entity_t
//    for s, i in set {
//        //fmt.printfln("e: %v, v: %v, i: %v", core.entities[indices[i]], s, i)
//        //fmt.printfln("%v", core.entities[indices[i]])
//        append(&owners, core.entities[indices[i]])
//    }
//    return set, owners[:len(owners)]
//}

query_1 :: #force_inline proc($type: typeid) -> []type {
    if core.sets[type].components == nil do return nil
    return (cast(^[dynamic]type)(core.sets[type].components))[:core.sets[type].count]
}

query_2 :: #force_inline proc($t1: typeid, $t2: typeid) -> #soa[]struct { _0: t1, _1: t2 } {
    // need to impl owning entities to query those with both types to then make an (cached) iterator
    if core.sets[t1].components == nil do return nil
    s1 := (cast(^[dynamic]t1)(core.sets[t1].components))[:core.sets[t1].count]

    if core.sets[t2].components == nil do return nil
    s2 := (cast(^[dynamic]t2)(core.sets[t2].components))[:core.sets[t2].count]
    return soa_zip(s1, s2)
}

query_3 :: #force_inline proc($t1: typeid, $t2: typeid, $t3: typeid) -> #soa[]struct { _0: t1, _1: t2, _2: t3 } {
    // need to impl owning entities to query those with both types to then make an (cached) iterator
    if core.sets[t1].components == nil do return nil
    s1 := (cast(^[dynamic]t1)(core.sets[t1].components))[:core.sets[t1].count]

    if core.sets[t2].components == nil do return nil
    s2 := (cast(^[dynamic]t2)(core.sets[t2].components))[:core.sets[t2].count]

    if core.sets[t3].components == nil do return nil
    s3 := (cast(^[dynamic]t3)(core.sets[t3].components))[:core.sets[t3].count]
    return soa_zip(s1, s2, s3)
}

query_4 :: #force_inline proc(
    $t1: typeid,
    $t2: typeid,
    $t3: typeid,
    $t4: typeid
) -> #soa[]struct {
    _0: t1,
    _1: t2,
    _2: t3,
    _3: t4
} {
    // need to impl owning entities to query those with both types to then make an (cached) iterator
    if core.sets[t1].components == nil do return nil
    s1 := (cast(^[dynamic]t1)(core.sets[t1].components))[:core.sets[t1].count]

    if core.sets[t2].components == nil do return nil
    s2 := (cast(^[dynamic]t2)(core.sets[t2].components))[:core.sets[t2].count]

    if core.sets[t3].components == nil do return nil
    s3 := (cast(^[dynamic]t3)(core.sets[t3].components))[:core.sets[t3].count]

    if core.sets[t4].components == nil do return nil
    s4 := (cast(^[dynamic]t4)(core.sets[t4].components))[:core.sets[t4].count]
    return soa_zip(s1, s2, s3, s4)
}

query :: proc { query_1, query_2, query_3, query_4 }

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

