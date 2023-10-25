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

//_________________________________________
// Callbacks to bind for input
struct KeyCallback {
    std::function<void()> press   = []{};
    std::function<void()> release = []{};
};

//_________________________________________
// Collected info of glfw key codes for lookup in unordered_map
struct KeyInfo {
    int key;
    int mods;

    KeyInfo(int key, int mods = 0) {
        this->key    = key;
        this->mods   = mods;
    }

    uint16_t operator[](uint16_t i) {
        std::hash<std::string> hsh;
        std::stringstream ss;
        ss << key << mods;
        return hsh(ss.str());
    }

    bool operator==(const KeyInfo& o) const {
        return (key == o.key
             && mods == o.mods);
    }

    bool operator()(const KeyInfo&, const KeyInfo&) const {
        return true;
    }

    friend std::ostream& operator<<(std::ostream& os, const KeyInfo& k) {
        os << k.key << ", " << k.mods << "\n";
        return os;
    }
};

namespace std {
    template <>
    struct hash<KeyInfo> {
        size_t operator()(const KeyInfo& o) const {
            return ((hash<int>()(o.key)
                  ^ (hash<int>()(o.mods) << 1) >> 1));
        }
    };
}

//_________________________________________
// Per window info and instancing
struct WindowHandle {
    WindowHandle( uint16_t width = 1920, uint16_t height = 1080, std::string title = std::string("sigil") ) {
        instance = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(instance, this);
        glfwSetFramebufferSizeCallback(instance, [](GLFWwindow* win, int w, int h){
            static_cast<WindowHandle*>(glfwGetWindowUserPointer(win))->resized = true;
        });
    }
    bool resized = false;
    GLFWwindow* instance;
};

//_________________________________________
// Window handling
struct Windowing : sigil::Module {
    public:
    virtual void init()      override;
    virtual void terminate() override;
    virtual void tick()      override;

    WindowHandle* main_window;
};

//_________________________________________
// Input handling
//
// @TODO: Improve modkey handling
struct Input : sigil::Module {
    public:
    virtual void init() override;
    virtual void tick() override;

    glm::dvec2 get_mouse_movement();
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_callback(GLFWwindow* window, int button, int action, int mods);
    Input* bind(KeyInfo key_info, KeyCallback callbacks);

private:
    glm::dvec2 last_mouse_position;
    glm::dvec2 mouse_position;
    void setup_standard_bindings();
    
    GLFWwindow* window;
    inline static std::unordered_map<KeyInfo, KeyCallback> callbacks;
};

