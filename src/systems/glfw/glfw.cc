#include "glfw.hh"
#include "renderer.hh"

#include <iostream>
#include <stdexcept>
#include <tuple>


//    WINDOWING    //

void Windowing::init() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    main_window = new WindowHandle;
}

void Windowing::terminate() {
    glfwDestroyWindow(main_window->instance);
    glfwTerminate();
};

void Windowing::tick() {
    if( glfwWindowShouldClose(main_window->instance) ) {
        sigil->should_close = true;
    }
    glfwPollEvents();
};

//    INPUT    //

void Input::init() {
    window = sigil->get_module<Windowing>()->main_window->instance;
    setup_standard_bindings();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_callback);
}

void Input::tick() {
    last_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    std::cout << "x: " << mouse_position.x << ", y: " << mouse_position.y << "\n"
        << last_mouse_position.x - mouse_position.x << ", " << last_mouse_position.y - mouse_position.y << "\n";
};

glm::dvec2 Input::get_mouse_movement() {
    return mouse_position - last_mouse_position;
}

void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if( callbacks.contains(key) ) {
        if( action == GLFW_PRESS ) {
            callbacks.at({ key }).press();
        } else if( action == GLFW_RELEASE ) {
            callbacks.at({ key }).release();
        }
    }
}

void Input::mouse_callback(GLFWwindow* window, int button, int action, int mods) {
    if( callbacks.contains(button) ) {
        if( action == GLFW_PRESS ) {
            callbacks.at({ button }).press();
        } else if( action == GLFW_RELEASE ) {
            callbacks.at({ button }).release();
        }
    }
}

void Input::setup_standard_bindings() {
    bind(GLFW_KEY_T,      KeyCallback { .press = []{ std::cout << "test\n"; } });
    bind(GLFW_KEY_ESCAPE, KeyCallback { .press = [this]{ glfwSetWindowShouldClose(window, true); } });
}

Input* Input::bind(KeyInfo key_info, KeyCallback callback_fns) {
    if( callbacks.contains(key_info.key) ) {
        std::cout << key_info.key;
        throw std::runtime_error("\n>>>\tError: Overwriting earlier keybind.");
    }
    callbacks.insert({ KeyInfo { key_info.key, key_info.mods }, callback_fns });
    return this;
}

