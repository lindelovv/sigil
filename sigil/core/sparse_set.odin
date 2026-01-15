package __sigil

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

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

/* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

