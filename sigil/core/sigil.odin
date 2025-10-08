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

MAX_TREE_DEPTH :: 16
MAX_NODES_PER_DEPTH :: 4096

node_handle_t :: distinct bit_field u16 {
    depth: u8  | 4,
    index: u16 | 12,
}

INVALID_NODE := node_handle_t { depth = 15, index = 0xFFF }

make_handle :: proc(depth: u8, index: u16) -> node_handle_t {
    assert(depth < MAX_TREE_DEPTH)
    assert(index < MAX_NODES_PER_DEPTH)
    return node_handle_t { depth = depth, index = index }
}

core: struct {
    entities: [dynamic]entity_t,
    flags: [dynamic]bit_array.Bit_Array,
    component_tree: component_tree_t,
    tree_mutex: sync.Mutex,
    sets: map[typeid]set_t,
    set_lookup: map[int]typeid,
    all_component_types: [dynamic]typeid,
}

component_tree_t :: struct {
    nodes: [MAX_TREE_DEPTH][MAX_NODES_PER_DEPTH]group_node_t,
    node_count: [MAX_TREE_DEPTH]int,
    signature_map: map[u64]node_handle_t,
    root_handle: node_handle_t,
    generation: u64,
}

group_node_t :: struct {
    signature: u64,
    components: [8]typeid,
    component_count: u8,
    depth: u8,
    entity_count: int,
    parent: node_handle_t,
    first_child: node_handle_t,
    sibling_next: node_handle_t,
    version: u64,
    occupied: bool,
    reader_count: i64,
    component_ranges: [8]component_range_t,
    built: bool,
    needs_rebuild: bool,
}

component_range_t :: struct {
    set_type: typeid,
    offset: int,
    stride: int,
    count: int,
}

set_t :: struct {
    components: rawptr,
    indices: [dynamic]int,
    reverse_indices: [dynamic]entity_t,
    count: int,
    capacity: int,
    free_list: [dynamic]int,
    id: int,
    type: typeid,
    size: int,
}

hierarchical_bitset_t :: struct {
    words: [dynamic]u64,
    summary: u64,
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
    reserve(&core.entities, 1024)
    reserve(&core.flags, 1024)
    
    /* INVALID */ new_entity()
    /*  WORLD  */ new_entity()
    add(WORLD, name_t("world"))
    
    core.component_tree.signature_map = {}
    core.component_tree.root_handle = INVALID_NODE
}

is_valid_handle :: proc(handle: node_handle_t) -> bool {
    return handle.depth != 15 || handle.index != 0xFFF
}

get_node :: proc(tree: ^component_tree_t, handle: node_handle_t) -> ^group_node_t {
    if !is_valid_handle(handle) do return nil
    depth := handle.depth
    index := handle.index
    if depth >= MAX_TREE_DEPTH || index >= MAX_NODES_PER_DEPTH do return nil
    if int(index) >= tree.node_count[depth] do return nil
    return &tree.nodes[depth][index]
}

hbitset_create :: proc(size: int) -> hierarchical_bitset_t {
    word_count := (size + 63) / 64
    return hierarchical_bitset_t{
        words = make([dynamic]u64, word_count),
        summary = 0,
    }
}

hbitset_set :: proc(hb: ^hierarchical_bitset_t, bit: int) {
    word_idx := bit / 64
    bit_idx := u64(bit % 64)
    if word_idx >= len(hb.words) do resize(&hb.words, word_idx + 1)
    hb.words[word_idx] |= (1 << bit_idx)
    hb.summary |= (1 << u64(word_idx))
}

hbitset_get :: proc(hb: ^hierarchical_bitset_t, bit: int) -> bool {
    word_idx := bit / 64
    if word_idx >= len(hb.words) do return false
    if (hb.summary & (1 << u64(word_idx))) == 0 do return false
    bit_idx := u64(bit % 64)
    return (hb.words[word_idx] & (1 << bit_idx)) != 0
}

hbitset_unset :: proc(hb: ^hierarchical_bitset_t, bit: int) {
    word_idx := bit / 64
    if word_idx >= len(hb.words) do return
    bit_idx := u64(bit % 64)
    hb.words[word_idx] &= ~(1 << bit_idx)
    if hb.words[word_idx] == 0 do hb.summary &= ~(1 << u64(word_idx))
}

components_to_signature :: proc(components: []typeid) -> u64 {
    sig: u64 = 0
    for comp in components do if set, exists := core.sets[comp]; exists do sig |= u64(1) << u64(set.id)
    return sig
}

signature_is_subset :: proc(subset, superset: u64) -> bool {
    return (subset & superset) == subset
}

rebuild_root :: proc() {
    tree := &core.component_tree
    clear(&core.all_component_types)
    for comp_type, _ in core.sets do append(&core.all_component_types, comp_type)
    all_signature := components_to_signature(core.all_component_types[:])
    if !is_valid_handle(tree.root_handle) {
        tree.root_handle = make_handle(0, 0)
        tree.node_count[0] = 1
        root := &tree.nodes[0][0]
        root.signature = all_signature
        root.component_count = u8(min(len(core.all_component_types), 8))
        for i in 0..<int(root.component_count) do root.components[i] = core.all_component_types[i]
        root.depth = 0
        root.parent = INVALID_NODE
        root.first_child = INVALID_NODE
        root.sibling_next = INVALID_NODE
        root.built = false
        tree.signature_map[all_signature] = tree.root_handle
    } else {
        root := get_node(tree, tree.root_handle)
        if root != nil && root.signature != all_signature {
            root.signature = all_signature
            root.component_count = u8(min(len(core.all_component_types), 8))
            for i in 0..<int(root.component_count) do root.components[i] = core.all_component_types[i]
            root.needs_rebuild = true
            delete_key(&tree.signature_map, root.signature)
            tree.signature_map[all_signature] = tree.root_handle
        }
    }
    sync.atomic_add(&tree.generation, 1)
}

acquire_group_read_lockfree :: proc(tree: ^component_tree_t, handle: node_handle_t) -> (version: u64, ok: bool) {
    node := get_node(tree, handle)
    if node == nil do return 0, false
    version = sync.atomic_load(&node.version)
    if sync.atomic_load(&node.occupied) do return 0, false
    current := node.parent
    for is_valid_handle(current) {
        parent := get_node(tree, current)
        if parent == nil do break
        if sync.atomic_load(&parent.occupied) do return 0, false
        current = parent.parent
    }
    if !check_children_not_locked(tree, handle) do return 0, false
    version_after := sync.atomic_load(&node.version)
    if version != version_after do return 0, false
    return version, true
}

validate_read_lockfree :: proc(tree: ^component_tree_t, handle: node_handle_t, version: u64) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    return sync.atomic_load(&node.version) == version
}

acquire_group_read :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    if is_valid_handle(node.parent) {
        parent := get_node(tree, node.parent)
        if parent != nil && sync.atomic_load(&parent.occupied) do return false
    }
    current := node.parent
    for is_valid_handle(current) {
        ancestor := get_node(tree, current)
        if ancestor == nil do break
        if sync.atomic_load(&ancestor.occupied) do return false
        current = ancestor.parent
    }
    if !check_children_not_locked(tree, handle) do return false
    sync.atomic_add(&node.reader_count, 1)
    return true
}

check_children_not_locked :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    if sync.atomic_load(&node.occupied) do return false
    child_handle := node.first_child
    for is_valid_handle(child_handle) {
        child := get_node(tree, child_handle)
        if child == nil do break
        if sync.atomic_load(&child.occupied) do return false
        if !check_children_not_locked(tree, child_handle) do return false
        child_handle = child.sibling_next
    }
    return true
}

release_group_read :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node != nil do sync.atomic_add(&node.reader_count, -1)
}

acquire_group_write :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    if sync.atomic_load(&node.reader_count) > 0 do return false
    if !check_children_readers(tree, handle) do return false
    if !sync.atomic_compare_exchange_strong(&node.occupied, false, true) do return false
    if !lock_parents(tree, handle) {
        sync.atomic_store(&node.occupied, false)
        return false
    }
    if !lock_children(tree, handle) {
        unlock_parents(tree, handle)
        sync.atomic_store(&node.occupied, false)
        return false
    }
    sync.atomic_add(&node.version, 1)
    return true
}

lock_parents :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    locked: [dynamic]node_handle_t
    defer delete(locked)
    current := node.parent
    for is_valid_handle(current) {
        parent := get_node(tree, current)
        if parent == nil do break
        if !sync.atomic_compare_exchange_strong(&parent.occupied, false, true) {
            for locked_handle in locked {
                locked_node := get_node(tree, locked_handle)
                if locked_node != nil do sync.atomic_store(&locked_node.occupied, false)
            }
            return false
        }
        append(&locked, current)
        current = parent.parent
    }
    return true
}

unlock_parents :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node == nil do return
    current := node.parent
    for is_valid_handle(current) {
        parent := get_node(tree, current)
        if parent == nil do break
        sync.atomic_store(&parent.occupied, false)
        current = parent.parent
    }
}

lock_children :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    child_handle := node.first_child
    for is_valid_handle(child_handle) {
        child := get_node(tree, child_handle)
        if child == nil do break
        if !sync.atomic_compare_exchange_strong(&child.occupied, false, true) do return false
        if !lock_children(tree, child_handle) do return false
        child_handle = child.sibling_next
    }
    return true
}

unlock_children :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node == nil do return
    child_handle := node.first_child
    for is_valid_handle(child_handle) {
        child := get_node(tree, child_handle)
        if child == nil do break
        sync.atomic_store(&child.occupied, false)
        unlock_children(tree, child_handle)
        child_handle = child.sibling_next
    }
}

check_children_readers :: proc(tree: ^component_tree_t, handle: node_handle_t) -> bool {
    node := get_node(tree, handle)
    if node == nil do return false
    if sync.atomic_load(&node.reader_count) > 0 do return false
    child_handle := node.first_child
    for is_valid_handle(child_handle) {
        if !check_children_readers(tree, child_handle) do return false
        child := get_node(tree, child_handle)
        if child == nil do break
        child_handle = child.sibling_next
    }
    return true
}

release_group_write :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node == nil do return
    sync.atomic_add(&node.version, 1)
    unlock_children(tree, handle)
    unlock_parents(tree, handle)
    sync.atomic_store(&node.occupied, false)
}

find_or_create_group :: proc(components: []typeid) -> (handle: node_handle_t, success: bool) {
    tree := &core.component_tree
    if !is_valid_handle(tree.root_handle) do rebuild_root()
    signature := components_to_signature(components)
    if signature == get_node(tree, tree.root_handle).signature {
        ensure_node_built(tree, tree.root_handle)
        return tree.root_handle, true
    }
    if existing_handle, exists := tree.signature_map[signature]; exists {
        ensure_node_built(tree, existing_handle)
        return existing_handle, true
    }
    return create_group_node(components, signature)
}

ensure_node_built :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node == nil do return
    if node.built && !node.needs_rebuild do return
    if acquire_group_write(tree, handle) {
        if !node.built || node.needs_rebuild {
            build_group_data(tree, handle)
            node.built = true
            node.needs_rebuild = false
        }
        release_group_write(tree, handle)
    }
}

find_best_parent :: proc(tree: ^component_tree_t, components: []typeid, signature: u64) -> node_handle_t {
    best_parent := INVALID_NODE
    best_component_count := max(int)
    for sig, handle in tree.signature_map {
        if signature_is_subset(signature, sig) && sig != signature {
            node := get_node(tree, handle)
            if node == nil do continue
            node_component_count := int(node.component_count)
            if node_component_count < best_component_count && node_component_count > len(components) {
                best_parent = handle
                best_component_count = node_component_count
            }
        }
    }
    if !is_valid_handle(best_parent) do best_parent = tree.root_handle
    return best_parent
}

create_group_node :: proc(components: []typeid, signature: u64) -> (handle: node_handle_t, success: bool) {
    tree := &core.component_tree
    parent_handle := find_best_parent(tree, components, signature)
    parent := get_node(tree, parent_handle)
    if parent == nil do return INVALID_NODE, false
    new_depth := parent.depth + 1
    if int(new_depth) >= MAX_TREE_DEPTH do return INVALID_NODE, false
    if tree.node_count[new_depth] >= MAX_NODES_PER_DEPTH do return INVALID_NODE, false
    new_index := u16(tree.node_count[new_depth])
    tree.node_count[new_depth] += 1
    new_handle := make_handle(new_depth, new_index)
    new_node := &tree.nodes[new_depth][new_index]
    new_node.signature = signature
    new_node.component_count = u8(min(len(components), 8))
    for i in 0..<int(new_node.component_count) do new_node.components[i] = components[i]
    new_node.depth = new_depth
    new_node.parent = parent_handle
    new_node.first_child = INVALID_NODE
    new_node.sibling_next = INVALID_NODE
    new_node.built = false
    new_node.needs_rebuild = false
    if !is_valid_handle(parent.first_child) do parent.first_child = new_handle
    else {
        sibling_handle := parent.first_child
        for is_valid_handle(sibling_handle) {
            sibling := get_node(tree, sibling_handle)
            if sibling == nil do break
            if !is_valid_handle(sibling.sibling_next) {
                sibling.sibling_next = new_handle
                break
            }
            sibling_handle = sibling.sibling_next
        }
    }
    tree.signature_map[signature] = new_handle
    reorganize_tree_for_node(tree, new_handle)
    sync.atomic_add(&tree.generation, 1)
    return new_handle, true
}

reorganize_tree_for_node :: proc(tree: ^component_tree_t, new_handle: node_handle_t) {
    new_node := get_node(tree, new_handle)
    if new_node == nil || !is_valid_handle(new_node.parent) do return
    parent_handle := new_node.parent
    parent := get_node(tree, parent_handle)
    if parent == nil do return
    new_sig := new_node.signature
    to_reparent: [dynamic]node_handle_t
    defer delete(to_reparent)
    sibling_handle := parent.first_child
    prev_handle := INVALID_NODE
    for is_valid_handle(sibling_handle) {
        sibling := get_node(tree, sibling_handle)
        if sibling == nil do break
        next_handle := sibling.sibling_next
        sibling_depth := sibling_handle.depth
        sibling_index := sibling_handle.index
        new_h_depth := new_handle.depth
        new_h_index := new_handle.index
        if sibling_depth == new_h_depth && sibling_index != new_h_index {
            if signature_is_subset(sibling.signature, new_sig) && int(sibling.component_count) < int(new_node.component_count) {
                append(&to_reparent, sibling_handle)
                if is_valid_handle(prev_handle) {
                    prev_node := get_node(tree, prev_handle)
                    if prev_node != nil do prev_node.sibling_next = next_handle
                } else do parent.first_child = next_handle
            } else do prev_handle = sibling_handle
        } else do prev_handle = sibling_handle
        sibling_handle = next_handle
    }
    for child_handle in to_reparent {
        child := get_node(tree, child_handle)
        if child == nil do continue
        child.parent = new_handle
        child.depth = new_node.depth + 1
        child.sibling_next = new_node.first_child
        new_node.first_child = child_handle
    }
}

build_group_data :: proc(tree: ^component_tree_t, handle: node_handle_t) {
    node := get_node(tree, handle)
    if node == nil do return
    node.entity_count = 0
    components := node.components[:node.component_count]
    for e in core.entities {
        if !entity_is_valid(e) do continue
        has_all := true
        for comp in components {
            if !has_component(e, comp) {
                has_all = false
                break
            }
        }
        if has_all do node.entity_count += 1
    }
    for i in 0..<int(node.component_count) {
        comp := node.components[i]
        set := core.sets[comp]
        if node.entity_count == 0 {
            node.component_ranges[i] = component_range_t{
                set_type = comp,
                offset = 0,
                stride = 1,
                count = 0,
            }
            continue
        }
        min_index := max(int)
        max_index := 0
        for e in core.entities {
            if !entity_is_valid(e) do continue
            has_all := true
            for c in components {
                if !has_component(e, c) {
                    has_all = false
                    break
                }
            }
            if has_all {
                idx := set.indices[e]
                if idx != -1 && idx < len(set.reverse_indices) {
                    if idx < min_index do min_index = idx
                    if idx > max_index do max_index = idx
                }
            }
        }
        node.component_ranges[i] = component_range_t{
            set_type = comp,
            offset = min_index,
            stride = 1,
            count = node.entity_count,
        }
    }
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
    return e
}

delete_entity :: #force_inline proc(entity: entity_t) {
    if !entity_is_valid(entity) do return
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
    idx := set.indices[entity]
    if idx == -1 || len(data) <= idx do return {}, false
    return data[idx], true
}

@(require_results)
get_ref :: proc(entity: entity_t, $type: typeid) -> (^type, bool) #optional_ok {
    if !has_component(entity, type) do return {}, false
    set := core.sets[type]
    data := cast(^[dynamic]type)(set.components)
    idx := set.indices[entity]
    if idx == -1 || len(data) <= idx do return {}, false
    ref := &data[idx]
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
    needs_root_rebuild := false
    if exists := &set.components; exists == nil || set.count == 0 {
        set.components = new([dynamic]type)
        set.capacity = 64
        reserve((^[dynamic]type)(set.components), set.capacity)
        append((^[dynamic]type)(set.components), type{})  // dummy
        set.id = len(core.sets)
        core.set_lookup[set.id] = type
        set.type = type
        set.size = size_of(type)
        needs_root_rebuild = true
    }
    data := cast(^[dynamic]type)(set.components)
    if len(set.indices) <= int(e) {
        old_len := len(set.indices)
        resize_dynamic_array(&set.indices, len(core.entities) + 1)
        for i in old_len..<len(set.indices) do set.indices[i] = -1
    }
    component_idx: int
    if len(set.free_list) > 0 {
        component_idx = pop(&set.free_list)
        data[component_idx] = component
    } else {
        append(data, component)
        component_idx = len(data) - 1
    }
    set.indices[e] = component_idx
    if len(set.reverse_indices) <= component_idx {
        resize_dynamic_array(&set.reverse_indices, component_idx + 1)
    }
    set.reverse_indices[component_idx] = e
    set.count += 1
    core.sets[type] = set
    if len(core.flags) <= int(e) do resize(&core.flags, len(core.entities) + 1)
    if &core.flags[e] == nil do core.flags[e] = bit_array.create(len(core.entities))^
    bit_array.set(&core.flags[e], set.id)
    if needs_root_rebuild do rebuild_root()
    invalidate_groups_containing_component(type)
    return component, component_idx
}
add :: proc { add_to_world, add_to_entity }

invalidate_groups_containing_component :: proc(comp_type: typeid) {
    tree := &core.component_tree
    set := core.sets[comp_type]
    comp_bit := u64(1) << u64(set.id)
    if is_valid_handle(tree.root_handle) {
        invalidate_node_if_contains(tree, tree.root_handle, comp_bit)
    }
    sync.atomic_add(&tree.generation, 1)
}

invalidate_node_if_contains :: proc(tree: ^component_tree_t, handle: node_handle_t, comp_bit: u64) {
    node := get_node(tree, handle)
    if node == nil do return
    if (node.signature & comp_bit) != 0 {
        node.needs_rebuild = true
    }
    child_handle := node.first_child
    for is_valid_handle(child_handle) {
        invalidate_node_if_contains(tree, child_handle, comp_bit)
        child := get_node(tree, child_handle)
        if child == nil do break
        child_handle = child.sibling_next
    }
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
    idx := set.indices[entity]
    if idx == -1 do return
    append(&set.free_list, idx)
    set.indices[entity] = -1
    set.reverse_indices[idx] = INVALID
    set.count -= 1
    bit_array.unset(&core.flags[entity], set.id)
    invalidate_groups_containing_component(set.type)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

get_component_slice_from_node :: proc(tree: ^component_tree_t, handle: node_handle_t, $t: typeid) -> []t {
    node := get_node(tree, handle)
    if node == nil do return nil
    set := core.sets[t]
    for i in 0..<int(node.component_count) {
        comp_range := node.component_ranges[i]
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
    data := (cast(^[dynamic]type)(set.components))[1:set.count+1]
    return data
}

query_2 :: #force_inline proc($type1: typeid, $type2: typeid) -> #soa[]struct { x: type1, y: type2 } {
    tree := &core.component_tree
    components := []typeid { type1, type2 }
    handle, ok := find_or_create_group(components)
    if !ok do return {}
    if !acquire_group_read(tree, handle) do return {}
    slice1 := get_component_slice_from_node(tree, handle, type1)
    slice2 := get_component_slice_from_node(tree, handle, type2)
    if slice1 == nil || slice2 == nil {
        release_group_read(tree, handle)
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
    tree := &core.component_tree
    components := []typeid{type1, type2, type3}
    handle, ok := find_or_create_group(components)
    if !ok do return {}
    if !acquire_group_read(tree, handle) do return {}
    slice1 := get_component_slice_from_node(tree, handle, type1)
    slice2 := get_component_slice_from_node(tree, handle, type2)
    slice3 := get_component_slice_from_node(tree, handle, type3)
    if slice1 == nil || slice2 == nil || slice3 == nil {
        release_group_read(tree, handle)
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
    tree := &core.component_tree
    components := []typeid{type1, type2, type3, type4}
    handle, ok := find_or_create_group(components)
    if !ok do return {}
    if !acquire_group_read(tree, handle) do return {}
    slice1 := get_component_slice_from_node(tree, handle, type1)
    slice2 := get_component_slice_from_node(tree, handle, type2)
    slice3 := get_component_slice_from_node(tree, handle, type3)
    slice4 := get_component_slice_from_node(tree, handle, type4)
    if slice1 == nil || slice2 == nil || slice3 == nil || slice4 == nil {
        release_group_read(tree, handle)
        return {}
    }
    return soa_zip(slice1, slice2, slice3, slice4)
}
query :: proc { query_1, query_2, query_3, query_4 }

release_query :: proc(types: ..typeid) {
    tree := &core.component_tree
    if len(types) == 0 do return
    components := types[:]
    handle, found := find_or_create_group(components)
    if found do release_group_read(tree, handle)
}

//-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    
