#pragma once

namespace sigil {

    class Engine;

    class System {
        public:
            virtual ~System()                    {};

            virtual void init()                 = 0;
            virtual void terminate()            = 0;
            inline virtual void tick()           {};
            inline virtual void link(Engine* engine ) {
                core = engine;
            }
            Engine* core;
            bool can_tick = false;
    };
}
