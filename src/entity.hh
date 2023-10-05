#pragma once

#include "component.hh"
#include <vector>

namespace sigil {
    
    class Entity {

        std::vector<Component*> components;
    };
}
