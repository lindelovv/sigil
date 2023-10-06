#pragma once

#include "engine.hh"
#include "system.hh"

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {

    struct KeyInfo {
        uint16_t key;
        uint16_t action;
        uint16_t mods;

        KeyInfo(uint16_t key, uint16_t action, uint16_t mods) {
            this->key = key;
            this->action = action;
            this->mods = mods;
        }

        uint16_t operator[](uint16_t i) {
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
            return ((hash<uint16_t>()(o.key)
                    ^ (hash<uint16_t>()(o.action) << 1) >> 1)
                    ^ (hash<uint16_t>()(o.mods) << 1));
        }
    };
}

namespace sigil {
    
    class Glfw : public System {
        public:
            Glfw()                                      : width(1920), height(1080), title("sigil") {}
            Glfw(uint16_t w, uint16_t h, std::string t) : width(w),    height(h),    title(t)       {}

            virtual void init() override;
            virtual void terminate() override;
            virtual void tick() override;

            Glfw(const Glfw&)            = delete;
            Glfw& operator=(const Glfw&) = delete;

            GLFWwindow* window;

            static void callback(GLFWwindow* window, int key, int scancode, int action, int mods);

        private:
            void setup_standard_input_bindings();
            inline void bind_input(KeyInfo key_info, std::function<void()> callback_fn);

            const uint16_t   width;
            const uint16_t  height;
            std::string title;

            static std::unordered_map<KeyInfo, std::function<void()>> callbacks;
    };
}

