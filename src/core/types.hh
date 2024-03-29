#pragma once

#include <cstdint>
#include <functional>
#include <glm/fwd.hpp>

namespace sigil {

    using u8  = uint8_t;
    using i8  = int8_t;

    using u16 = uint16_t;
    using i16 = int16_t;

    using u32 = uint32_t;
    using i32 = int32_t;

    using u64 = uint64_t;
    using i64 = int64_t;

    using f32 = float;
    using f64 = double;

    template <typename ReturnType>
    using fn = std::function<ReturnType>;

    struct rgb {
        rgb(u8 r, u8 g, u8 b, u8 a)
            : red(r), green(g), blue(b) {};
        u8 red;
        u8 green;
        u8 blue;
    };
    
    struct rgba {
        rgba(u8 r, u8 g, u8 b, u8 a)
            : red(r), green(g), blue(b), alpha(a) {};
        u8 red;
        u8 green;
        u8 blue;
        u8 alpha;
    };
}

