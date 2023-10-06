#pragma once

#include "component.hh"
#include "engine.hh"

#include <concepts>
#include <memory>
#include <vector>
#include <cstdint>
#include <crossguid/guid.hpp>

namespace sigil {
    
    class Entity {
        public:
            Entity();
            inline void with(std::vector<Component*> components) {
                for( auto& component : components ) {
                    component->owner_id = this->id;
                    core.components.push_back(std::shared_ptr<Component>(component));
                }
            };
            xg::Guid id = xg::newGuid();
    };
}
