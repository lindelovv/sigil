package __sigil

import "core:mem"
import "core:fmt"
import "core:slice"
import "core:math"
import "core:hash"
import "core:sync"
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

MAX_COMPONENT_TYPES :: 64
MAX_TREE_DEPTH :: 16
MAX_GROUPS_PER_DEPTH :: 1024

core: struct {
    entities: [dynamic]entity_t,
    flags: [dynamic]bit_array.Bit_Array,
    component_tree: component_tree_t,
    tree_mutex: sync.Mutex,
    sets: map[typeid]set_t,
    set_lookup: map[int]typeid,
}

component_tree_t :: struct {
    groups: [MAX_TREE_DEPTH][MAX_GROUPS_PER_DEPTH]group_data_t,
    group_count: [MAX_TREE_DEPTH]int,
    signature_maps: [MAX_TREE_DEPTH]map[u64]int,
    occupied: [MAX_TREE_DEPTH]u64,
    reader_counts: [MAX_TREE_DEPTH][MAX_GROUPS_PER_DEPTH]i64,
}

group_data_t :: struct {
    signature: u64,
    entity_start: int,
    entity_count: int,
    parent_index: int,
    first_child: int,
    child_count: int,
    subtree_size: int,
    component_ranges: [MAX_COMPONENT_TYPES]struct {
        set_type: typeid,
        offset: int,
        count: int,
    },
}

set_t :: struct {
    components: rawptr,
    indices: [dynamic]int,
    count: int,
    id: int,
    type: typeid,
    size: int,
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

name_t :: distinct string

module_create_info_t :: struct {
    name: name_t,
    id: entity_t,
}
module :: #type proc(entity_t) -> typeid
init :: distinct #type proc()
tick :: distinct #type proc()
exit :: distinct #type proc()
none :: distinct #type proc()

relation :: struct($topic, $target: typeid) { topic, target }
tag_t :: distinct struct {}

before :: distinct []proc()
after :: distinct []proc()

request_exit: bool

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

@(init)
_init :: proc() {
    core = {}
    /* INVALID */ new_entity()
    /*  WORLD  */ new_entity()
    add(WORLD, name_t("world"))
    core.component_tree = {}
    core.component_tree.group_count[0] = 1
    core.component_tree.groups[0][0] = group_data_t{
        signature = 0,
        entity_start = 0,
        entity_count = len(core.entities),
        parent_index = -1,
        first_child = -1,
        child_count = 0,
        subtree_size = 1,
    }
    core.component_tree.signature_maps[0] = {}
    core.component_tree.signature_maps[0][0] = 0
    for i in 0..<MAX_TREE_DEPTH do if core.component_tree.signature_maps[i] == nil do core.component_tree.signature_maps[i] = {}
}

components_to_signature :: proc(components: []typeid) -> u64 {
    sig: u64 = 0
    for comp in components do if set, exists := core.sets[comp]; exists do sig |= u64(1) << u64(set.id)
    return sig
}

get_ancestor_index_at_depth :: proc(tree: ^component_tree_t, start_depth: int, start_index: int, target_depth: int) -> int {
    if target_depth > start_depth do return -1
    if start_depth < 0 || start_depth >= MAX_TREE_DEPTH do return -1
    if start_index < 0 || start_index >= tree.group_count[start_depth] do return -1
    current_depth := start_depth
    current_index := start_index
    for current_depth > target_depth {
        parent_index := tree.groups[current_depth][current_index].parent_index
        if parent_index == -1 do return -1
        if current_depth-1 < 0 do return -1 // We're about to go below depth 0
        current_index = parent_index
        current_depth -= 1
        if current_index < 0 || current_index >= tree.group_count[current_depth] do return -1
    }
    if current_depth != target_depth do return -1
    if current_index < 0 || current_index >= tree.group_count[current_depth] do return -1
    return current_index
}

acquire_group_read :: proc(tree: ^component_tree_t, depth: int, group_index: int) -> bool {
    current_depth := depth
    current_index := group_index
    for current_depth >= 0 {
        occupied_mask := sync.atomic_load(&tree.occupied[depth])
        check_index := get_ancestor_index_at_depth(tree, depth, group_index, current_depth)
        if check_index == -1 do return false // Handle the error - this shouldn't happen in a valid tree
        if (occupied_mask & (1 << u64(check_index))) != 0 do return false
        if current_depth == 0 do break
        parent_index := tree.groups[current_depth][current_index].parent_index
        if parent_index == -1 do break // We've reached the root
        current_index = parent_index
        current_depth -= 1
    }
    sync.atomic_add(&tree.reader_counts[depth][group_index], 1)
    return true
}

find_or_create_group :: proc(components: []typeid) -> (depth: int, index: int, success: bool) {
    tree := &core.component_tree
    signature := components_to_signature(components)
    depth = len(components)
    if depth < MAX_TREE_DEPTH && tree.signature_maps[depth] != nil {
        if index, exists := tree.signature_maps[depth][signature]; exists do return depth, index, true
    }
    return create_group_path(tree, components)
}

create_group_path :: proc(tree: ^component_tree_t, components: []typeid) -> (depth: int, index: int, success: bool) {
    depth = len(components)
    if depth >= MAX_TREE_DEPTH do return -1, -1, false
    signature := components_to_signature(components)
    parent_depth, parent_index: int = -1, -1
    if depth > 1 {
        parent_components := components[0:len(components)-1]
        parent_depth, parent_index, success = find_or_create_group(parent_components)
        if !success do return -1, -1, false
    }
    if tree.group_count[depth] >= MAX_GROUPS_PER_DEPTH {
        return -1, -1, false
    }
    index = tree.group_count[depth]
    tree.group_count[depth] += 1
    group := &tree.groups[depth][index]
    group.signature = signature
    group.parent_index = parent_index
    group.first_child = -1
    group.child_count = 0
    group.subtree_size = 1
    if parent_index != -1 {
        parent := &tree.groups[depth-1][parent_index]
        if parent.child_count == 0 {
            parent.first_child = index
        }
        parent.child_count += 1
        parent.subtree_size += 1 // Update subtree sizes up the tree
        current_depth := depth - 1
        current_index := parent_index
        for current_depth >= 0 && current_index >= 0 { // Check that current_index is also valid
            ancestor := &tree.groups[current_depth][current_index]
            ancestor.subtree_size += 1
            if current_depth == 0 do break
            current_index = ancestor.parent_index
            current_depth -= 1
        }
    }
    if tree.signature_maps[depth] == nil do tree.signature_maps[depth] = {}
    tree.signature_maps[depth][signature] = index
    build_group_data(tree, depth, index, components)
    return depth, index, true
}

build_group_data :: proc(tree: ^component_tree_t, depth: int, index: int, components: []typeid) {
    group := &tree.groups[depth][index]
    group_entities: [dynamic]entity_t
    for e in core.entities {
        if !entity_is_valid(e) do continue
        has_all := true
        for comp in components {
            if !has_component(e, comp) {
                has_all = false
                break
            }
        }
        if has_all do append(&group_entities, e)
    }
    fmt.printf("Group with components: ")
    for comp in components do fmt.printf("%v ", comp)
    fmt.printf(" - Found %d entities\n", len(group_entities))
    group.entity_count = len(group_entities)
    if group.entity_count == 0 do return
    for comp in components {
        set := core.sets[comp]
        min_index := max(int)
        max_index := 0
        for e in group_entities {
            idx := set.indices[e]
            if idx < min_index do min_index = idx
            if idx > max_index do max_index = idx
        }
        actual_count := 0
        for e in group_entities {
            idx := set.indices[e]
            if idx >= min_index && idx <= min_index + group.entity_count - 1 {
                actual_count += 1
            }
        }
        group.component_ranges[set.id] = {
            set_type = comp,
            offset = min_index,
            count = group.entity_count,
        }
    }
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

release_group_read :: proc(tree: ^component_tree_t, depth: int, group_index: int) {
    sync.atomic_add(&tree.reader_counts[depth][group_index], -1)
}

acquire_group_write :: proc(tree: ^component_tree_t, depth: int, group_index: int) -> bool {
    if !can_acquire_write(tree, depth, group_index) do return false
    current_depth := depth
    current_index := group_index
    for current_depth >= 0 {
        ancestor_index := get_ancestor_index_at_depth(tree, depth, group_index, current_depth)
        if ancestor_index == -1 do return false
        mask_to_set := u64(1 << u64(ancestor_index))
        for {
            old_mask := sync.atomic_load(&tree.occupied[current_depth])
            if (old_mask & mask_to_set) != 0 {
                for rollback_depth in current_depth+1..=depth {
                    rollback_index := get_ancestor_index_at_depth(tree, depth, group_index, rollback_depth)
                    if rollback_index != -1 {
                        rollback_mask := u64(1 << u64(rollback_index))
                        for {
                            rollback_old := sync.atomic_load(&tree.occupied[rollback_depth])
                            rollback_new := rollback_old & ~rollback_mask
                            if _, ok := sync.atomic_compare_exchange_strong(&tree.occupied[rollback_depth], rollback_old, rollback_new); ok do break
                        }
                    }
                }
                return false
            }
            new_mask := old_mask | mask_to_set
            if _, ok := sync.atomic_compare_exchange_strong(&tree.occupied[current_depth], old_mask, new_mask); ok do break
        }
        if current_depth == 0 do break
        current_index = tree.groups[current_depth][current_index].parent_index
        current_depth -= 1
    }
    return true
}

release_group_write :: proc(tree: ^component_tree_t, depth: int, group_index: int) {
    current_depth := depth
    current_index := group_index
    for current_depth >= 0 {
        ancestor_index := get_ancestor_index_at_depth(tree, depth, group_index, current_depth)
        if ancestor_index != -1 {
            mask_to_clear := u64(1 << u64(ancestor_index))
            for {
                old_mask := sync.atomic_load(&tree.occupied[current_depth])
                new_mask := old_mask & ~mask_to_clear
                if _, ok := sync.atomic_compare_exchange_strong(&tree.occupied[current_depth], old_mask, new_mask); ok do break
            }
        }
        if current_depth == 0 do break
        current_index = tree.groups[current_depth][current_index].parent_index
        current_depth -= 1
    }
}

can_acquire_write :: proc(tree: ^component_tree_t, depth: int, group_index: int) -> bool {
    if sync.atomic_load(&tree.reader_counts[depth][group_index]) > 0 do return false
    return check_subtree_readers(tree, depth, group_index)
}

check_subtree_readers :: proc(tree: ^component_tree_t, depth: int, group_index: int) -> bool {
    group := &tree.groups[depth][group_index]
    for i in 0..<group.child_count {
        child_index := group.first_child + i
        if depth + 1 >= MAX_TREE_DEPTH do continue
        if !check_subtree_readers(tree, depth + 1, child_index) do return false
    }
    return true
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

use :: #force_inline proc(setup: module) { e := new_entity(); add(e, setup(e)) }

schedule :: #force_inline proc(e: entity_t, fn: $type, conditions: []union { before, after } = nil)
where intrinsics.type_is_proc(type) {
    add(e, type(fn))
    for type in conditions do switch condition in type {
        case before: for fn in condition {}
        case after: for fn in condition {}
    }
}

run :: #force_inline proc() {
    for fn in query(init) { fn() }
    main_loop: for !request_exit { for fn in query(tick) { fn() }; free_all(context.temp_allocator) }
    for fn in query(exit) { fn() }
}

new_entity :: #force_inline proc() -> entity_t {
    sync.mutex_lock(&core.tree_mutex)
    defer sync.mutex_unlock(&core.tree_mutex)
    e := entity_t(len(core.entities))
    append(&core.entities, e)
    append(&core.flags, bit_array.create(len(core.sets))^)
    core.component_tree.groups[0][0].entity_count += 1
    return e
}

delete_entity :: #force_inline proc(entity: entity_t) {
    if !entity_is_valid(entity) do return
    sync.mutex_lock(&core.tree_mutex)
    defer sync.mutex_unlock(&core.tree_mutex)
    if len(core.flags) <= auto_cast entity do return
    it := bit_array.make_iterator(&core.flags[entity])
    for cont := true; cont; {
        id: int
        id, cont = bit_array.iterate_by_set(&it)
        set := &core.sets[core.set_lookup[id]]
        remove_component(entity, set)
    }
}

get_entity :: #force_inline proc(entity: entity_t) -> entity_t {
    for e in core.entities { if e == entity do return e }
    return INVALID
}

entity_is_valid :: proc(entity: entity_t) -> bool {
    return entity != INVALID && int(entity) < len(core.entities)
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
    sync.mutex_lock(&core.tree_mutex)
    defer sync.mutex_unlock(&core.tree_mutex)
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
    if len(set.indices) <= int(e) do resize_dynamic_array(&set.indices, len(core.entities) + 1)
    if len(data) <= int(e) || len(data) <= set.indices[e] {
        append(data, component)
        set.indices[e] = len(data) - 1
        set.count += 1
    } else do data[set.indices[e]] = component
    core.sets[type] = set
    if len(core.flags) <= int(e) do resize(&core.flags, len(core.entities) + 1)
    if &core.flags[e] == nil do core.flags[e] = bit_array.create(len(core.entities))^
    bit_array.set(&core.flags[e], set.id)
    invalidate_groups_containing_component(type)
    return component, set.indices[e]
}
add :: proc { add_to_world, add_to_entity }

invalidate_groups_containing_component :: proc(comp_type: typeid) {
    tree := &core.component_tree
    set := core.sets[comp_type]
    comp_bit := u64(1) << u64(set.id)
    for depth in 0..<MAX_TREE_DEPTH do for i in 0..<tree.group_count[depth] {
        group := &tree.groups[depth][i]
        if (group.signature & comp_bit) != 0 {
            build_group_data(tree, depth, i, get_components_from_signature(group.signature)[:])
        }
    }
}

get_components_from_signature :: proc(sig: u64) -> [dynamic]typeid {
    components: [dynamic]typeid
    for i in 0..<MAX_COMPONENT_TYPES {
        if (sig & (1 << u64(i))) != 0 {
            if comp_type, exists := core.set_lookup[i]; exists do append(&components, comp_type)
        }
    }
    return components
}

remove_component :: proc { _remove_component_type, _remove_component_set }
_remove_component_type :: proc(#any_int entity: entity_t, $type: typeid) {
    if !has_component(entity, type) do return
    set := &core.sets[type]
    _remove_component_set(entity, set)
}
_remove_component_set :: proc(#any_int entity: entity_t, set: ^set_t) {
    sync.mutex_lock(&core.tree_mutex)
    defer sync.mutex_unlock(&core.tree_mutex)
    if set.indices[entity] == 0 do return
    _swap_component_index(set, entity, set.count)
    _remove_component_index(set, set.count)
    bit_array.unset(&core.flags[entity], set.id)
    invalidate_groups_containing_component(set.type)
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

get_component_slice_from_group :: proc(group: ^group_data_t, $t: typeid) -> []t {
    set := core.sets[t]
    for comp_range in group.component_ranges {
        if comp_range.set_type == t {
            data := cast(^[dynamic]t)(set.components)
            if comp_range.offset + comp_range.count <= len(data) {
                return data[comp_range.offset:comp_range.offset + comp_range.count]
            }
        }
    }
    return nil
}

query_1 :: #force_inline proc($type: typeid) -> []type {
    set := core.sets[type]
    if set.components == nil do return nil
    data := (cast(^[dynamic]type)(set.components))[1:][:set.count]
    return data
}

query_2 :: #force_inline proc($type1: typeid, $type2: typeid) -> #soa[]struct { x: type1, y: type2 } {
    components := []typeid { type1, type2 }
    depth, index, ok := find_or_create_group(components)
    if !ok do return {}
    tree := &core.component_tree
    if !acquire_group_read(tree, depth, index) do return {}
    group := &tree.groups[depth][index]
    slice1 := get_component_slice_from_group(group, type1)
    slice2 := get_component_slice_from_group(group, type2)
    if slice1 == nil || slice2 == nil {
        release_group_read(tree, depth, index)
        return {}
    }
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
    components := []typeid{type1, type2, type3}
    depth, index, ok := find_or_create_group(components)
    if !ok do return {}
    tree := &core.component_tree
    if !acquire_group_read(tree, depth, index) do return {}
    group := &tree.groups[depth][index]
    slice1 := get_component_slice_from_group(group, type1)
    slice2 := get_component_slice_from_group(group, type2)
    slice3 := get_component_slice_from_group(group, type3)
    if slice1 == nil || slice2 == nil || slice3 == nil {
        release_group_read(tree, depth, index)
        return {}
    }
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
    components := []typeid{type1, type2, type3, type4}
    depth, index, ok := find_or_create_group(components)
    if !ok do return {}
    tree := &core.component_tree
    if !acquire_group_read(tree, depth, index) do return {}
    group := &tree.groups[depth][index]
    slice1 := get_component_slice_from_group(group, type1)
    slice2 := get_component_slice_from_group(group, type2)
    slice3 := get_component_slice_from_group(group, type3)
    slice4 := get_component_slice_from_group(group, type4)
    if slice1 == nil || slice2 == nil || slice3 == nil || slice4 == nil {
        release_group_read(tree, depth, index)
        return {}
    }
    return soa_zip(slice1, slice2, slice3, slice4)
}
query :: proc { query_1, query_2, query_3, query_4 }

release_query :: proc(types: ..typeid) {
    if len(types) == 0 do return
    components := types[:]
    depth, index, found := find_or_create_group(components)
    if found do release_group_read(&core.component_tree, depth, index)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
