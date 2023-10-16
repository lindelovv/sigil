#pragma once

#include "sigil.hh"

#include <string>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <functional>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {
    
    class Window : public system_t {
        public:
            Window() : width(1920), height(1080), title("sigil") {}

            Window(Window& window) : width(window.width), height(window.height), title(window.title) {}

            Window(int w, int h, std::string t) : width(w), height(h), title(t) {}

            virtual void init() override;
            virtual void terminate() override;
            virtual void tick() override;

            Window(const Window&)            = delete;
            Window& operator=(const Window&) = delete;

            GLFWwindow* main_window;

        private:
            const int   width;
            const int  height;
            std::string title;
    };

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

