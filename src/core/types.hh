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

    template <typename ReturnType>
    using fn = std::function<ReturnType>;

    typedef glm::vec3 position;
    typedef glm::vec3 rotation;
    typedef glm::vec3 scale;

    typedef glm::vec3 velocity;
    
    struct rgb {
        rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
            : red(r), green(g), blue(b) {};
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };
    
    struct rgba {
        rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
            : red(r), green(g), blue(b), alpha(a) {};
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
    };
}

