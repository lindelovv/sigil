#pragma once

#include <cstdint>

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

