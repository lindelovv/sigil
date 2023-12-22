#pragma once

#include "sigil.hh"
#include "math.hh"

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

    //_________________________________________
    // Callbacks to bind for input
    struct key_callback {
        std::function<void()> press   = []{};
        std::function<void()> release = []{};
    };
    
    //_________________________________________
    // Collected info of glfw key codes for lookup in unordered_map
    struct key_info {
        int key;
        int mods;
    
        key_info(int key, int mods = 0) {
            this->key    = key;
            this->mods   = mods;
        }
    
        uint16_t operator[](uint16_t i) {
            std::hash<std::string> hsh;
            std::stringstream ss;
            ss << key << mods;
            return hsh(ss.str());
        }
    
        bool operator==(const key_info& o) const {
            return (key == o.key
                 && mods == o.mods);
        }
    
        bool operator()(const key_info&, const key_info&) const {
            return true;
        }
    
        friend std::ostream& operator<<(std::ostream& os, const key_info& k) {
            os << k.key << ", " << k.mods << "\n";
            return os;
        }
    };
}

namespace std {
    template <>
    struct hash<sigil::key_info> {
        size_t operator()(const sigil::key_info& o) const {
            return ((hash<int>()(o.key)
                  ^ (hash<int>()(o.mods) << 1) >> 1));
        }
    };
}
    
namespace sigil {

    //_________________________________________
    // Event handling
    namespace window {
        inline GLFWwindow* handle;
        inline bool resized = false;

        inline void poll() {
            if( glfwWindowShouldClose(window::handle) ) {
                sigil::request_exit();
            }
            glfwPollEvents();
        }
    };
    
    //_________________________________________
    // Input handling
    //
    // @TODO: Improve modkey handling
    namespace input {
        inline glm::dvec2 mouse_position;
        inline glm::dvec2 last_mouse_position;
        inline static std::unordered_map<key_info, key_callback> callbacks;

        inline void poll() {
            last_mouse_position = mouse_position;
            glfwGetCursorPos(window::handle, &mouse_position.x, &mouse_position.y);
        }

        inline glm::dvec2 get_mouse_movement() {
            return mouse_position - last_mouse_position;
        }

        static void keyboard_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
            if( callbacks.contains(key) ) {
                if( action == GLFW_PRESS ) {
                    callbacks.at({ key }).press();
                } else if( action == GLFW_RELEASE ) {
                    callbacks.at({ key }).release();
                }
            }
        }

        static void mouse_callback(GLFWwindow* window, int button, int action, int mods) {
            if( callbacks.contains(button) ) {
                if( action == GLFW_PRESS ) {
                    callbacks.at({ button }).press();
                } else if( action == GLFW_RELEASE ) {
                    callbacks.at({ button }).release();
                }
            }
        }

        inline void bind(key_info info, key_callback callbacks) {
            if( input::callbacks.contains(info.key) ) {
                std::cout << info.key;
                throw std::runtime_error("\n>>>\tError: Overwriting earlier keybind.");
            }
            input::callbacks.insert({ key_info { info.key, info.mods }, callbacks });
        }

        inline void setup_standard_bindings() {
            input::bind(GLFW_KEY_T,      key_callback { .press = []{ std::cout << "test\n"; } });
            input::bind(GLFW_KEY_ESCAPE, key_callback { .press = []{ glfwSetWindowShouldClose(window::handle, true); } });
        }
    };
    
    //_________________________________________
    // Time and timer handler
    namespace time {
        inline float fps;
        inline float ms;
        inline float prev_second;
        inline float delta_time;
        inline float last_frame;

        inline void poll() {
            float current_time = glfwGetTime();
            delta_time = current_time - last_frame;
            if( current_time >= prev_second + 1 ) {
                fps = (1000 / delta_time) / 1000;
                ms  = delta_time * 1000;
                prev_second = current_time;
            }
            last_frame = current_time;
        }
    };


    //_________________________________________
    // @TODO: Possibly combine all into this single struct since they all rely on glfw anyway
    struct glfw {
        glfw() {
            assert(glfwInit());
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            window::handle = glfwCreateWindow(1920, 1080, std::string("sigil").c_str(), nullptr, nullptr);
            glfwSetWindowUserPointer(window::handle, this);
            glfwSetFramebufferSizeCallback(window::handle,
                [](GLFWwindow* window, int w, int h){
                    window::resized = true;
            });
            input::setup_standard_bindings();
            glfwSetInputMode(window::handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            glfwSetKeyCallback(window::handle, input::keyboard_callback);
            glfwSetMouseButtonCallback(window::handle, input::mouse_callback);

            sigil::schedule(tick, {
                time::poll,
                input::poll,
                window::poll
            });
        }
        ~glfw() {
            glfwDestroyWindow(window::handle);
            glfwTerminate();
        }
    };
}

