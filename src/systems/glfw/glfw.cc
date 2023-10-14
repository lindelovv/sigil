#include "glfw.hh"
#include "renderer.hh"

#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>

namespace sigil {

    std::unordered_map<KeyInfo, std::function<void()>>  Input::callbacks;

    struct test {
        test() { s = "test"; }
        std::string s;
    };

/** WINDOW HANLDER **/

    void Windowing::init() {
        glfwInit();
        glfwSetErrorCallback([](int i, const char* c){ std::cout << i << ", " << c << "\n"; });
        //window = core->create<Window>()->instance;
        main_window = new Window();
        create<test>();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        if( auto tst = get<test>() ) {
            std::cout << tst->s;
        } else {
            std::cout << "nullptr";
        }
    }

    void Windowing::terminate() {
        glfwDestroyWindow(main_window->instance);
        glfwTerminate();
    };

    void Windowing::tick() {
        if( glfwWindowShouldClose(main_window->instance) ) {
            request_exit();
        }
        glfwPollEvents();
    };


/** INPUT **/

    void Input::init() {
        window = get<Window>()->instance;
        setup_standard_bindings();
        glfwSetKeyCallback(window, callback);
    }

    void Input::bind(KeyInfo key_info, std::function<void()> callback_fn) {
        callbacks.insert({ key_info, callback_fn });
    }

    void Input::callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        if( callbacks.contains({ (uint16_t)key, (uint16_t)action, (uint16_t)mods }) ) {
            callbacks.at({ (uint16_t)key, (uint16_t)action, (uint16_t)mods })();
        }
    }

    void Input::setup_standard_bindings() {
        bind({ GLFW_KEY_T,      GLFW_PRESS, 0 }, []     () { std::cout << "test\n";               });
        bind({ GLFW_KEY_Q,      GLFW_PRESS, 0 }, [&] () { glfwSetWindowShouldClose(window, true); });
        bind({ GLFW_KEY_ESCAPE, GLFW_PRESS, 0 }, [&] () { glfwSetWindowShouldClose(window, true); });
    }
}
