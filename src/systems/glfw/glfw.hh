#pragma once

#include "sigil.hh"

#include <cassert>
#include <functional>
#include <string>
#include <sstream>
#include <unordered_map>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace sigil {

    struct KeyInfo {
        uint16_t key;
        uint16_t action;
        uint16_t mods;

        KeyInfo(uint16_t key, uint16_t action, uint16_t mods) {
            this->key    = key;
            this->action = action;
            this->mods   = mods;
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

    struct Window {
        Window( uint16_t width = 1920, uint16_t height = 1080, std::string title = std::string("sigil") ) {
            instance = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
            assert(instance != nullptr);
            glfwSetWindowUserPointer(instance, this);
            glfwSetFramebufferSizeCallback(instance, [](GLFWwindow* win, int w, int h){
                static_cast<Window*>(glfwGetWindowUserPointer(win))->resized = true;
            });
        }
        bool resized = false;
        GLFWwindow* instance;
    };
    
    class Windowing : public system_t {
        public:
            virtual void init()              override;
            virtual void terminate()         override;
            virtual void tick()              override;

            Window* main_window;
    };

    class Input : public system_t {
        public:
            virtual void init()              override;

            static void callback(GLFWwindow* window, int key, int scancode, int action, int mods);
        private:
            void setup_standard_bindings();
            inline void bind(KeyInfo key_info, std::function<void()> callback_fn);
            
            GLFWwindow* window;
            static std::unordered_map<KeyInfo, std::function<void()>> callbacks;
    };
}

