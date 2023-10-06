#pragma once

#include <crossguid/guid.hpp>
#include <cstdint>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace sigil {

    struct Component {
        xg::Guid owner_id;
    };

    struct Transform : Component {
        glm::vec3 position { 0, 0 ,0 };
        glm::vec3 rotation { 0, 0 ,0 };
        glm::vec3 scale    { 0, 0 ,0 };
    };

    struct Lens : Component {
        
    };
}
