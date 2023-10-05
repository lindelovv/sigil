#pragma once

#include "engine.hh"

namespace sigil {

    class System {
    
        public:
            virtual ~System()         {};
            virtual void init()      = 0;
            virtual void terminate() = 0;
            virtual void tick()      = 0;

            bool can_tick = 0;
            static Engine* core;
    };
}
