#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

//____________________________________
// @TODO: all

namespace sigil::math {

#define PI 3.14159265358979323846

    //inline float ease_in(float x) {
    //    return 1 - cos((x * PI) / 2);
    //}

    //inline float ease_out(float x) {
    //    return sin((x * PI) / 2);
    //}

    //inline float ease_in_out(float x) {
    //    return -(cos(PI * x) - 1) / 2;
    //}

} // sigil::math

