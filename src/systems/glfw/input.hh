#pragma once

#include "sigil.hh"
#include "window.hh"

#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <functional>
#include <optional>

namespace sigil {

    struct KeyInfo {
        int key;
        int action;
        int mods;

        KeyInfo(int key, int action, int mods) {
            this->key = key;
            this->action = action;
            this->mods = mods;
        }

        int operator[](int i) {
            std::hash<std::string> hsh;
            std::stringstream ss;
            ss << key << action << mods;
            return hsh(ss.str());
        }

        bool operator==(const KeyInfo& o) const {
            return (key == o.key
                 && action == o.action
                 && mods == o.mods);
        }

        bool operator()(const KeyInfo&, const KeyInfo&) const {
            return true;
        }

        friend std::ostream& operator<<(std::ostream& os, const KeyInfo& k) {
            os << k.key << ", " << k.action << ", " << k.mods << "\n";
            return os;
        }
    };
}

namespace std {

    template <>
    struct hash<sigil::KeyInfo> {
        size_t operator()(const sigil::KeyInfo& o) const {
            return ((hash<int>()(o.key)
                    ^ (hash<int>()(o.action) << 1) >> 1)
                    ^ (hash<int>()(o.mods) << 1));
        }
    };
}

namespace sigil {

    class Input : public system_t {

        public:
            virtual void init() override;
            virtual void terminate() override {};
            virtual void tick() override {};
            static void callback(GLFWwindow* window, int key, int scancode, int action, int mods);
        private:
            void setup_standard_bindings();
            static std::unordered_map<KeyInfo, std::function<void()>> callbacks;
            inline void bind(KeyInfo key_info, std::function<void()> callback_fn) {
                callbacks.insert({ key_info, callback_fn });
            }
            Window* window;
    };
}

