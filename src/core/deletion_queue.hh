#pragma once

#include "types.hh"

#include <deque>

namespace sigil {
    
    struct DeletionQueue {
        std::deque<fn<void()>> delegates;

        void push_function(fn<void()>&& fn) {
            delegates.push_back(fn);
        }

        void flush() {
            for( auto it = delegates.rbegin(); it != delegates.rend(); it++ ) {
                (*it)();
            }
            delegates.clear();
        }
    };

} // sigil

