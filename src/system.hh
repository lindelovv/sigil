#pragma once

namespace sigil {

    class System {
    
        public:
            virtual ~System()                    {};
            virtual void init()                 = 0;
            virtual void link(class Engine*)    = 0;
            virtual void terminate()            = 0;
            virtual bool can_tick()             = 0;
            virtual void tick()                 = 0;
    };
}
