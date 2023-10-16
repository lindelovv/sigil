#include "glfw.hh"
#include "renderer.hh"

#include <stdexcept>
#include <vector>

namespace sigil {

    std::unordered_map<KeyInfo, std::function<void()>>  Input::callbacks;

    struct test {
        test() { s = "test"; }
        std::string s;
    };

/** WINDOW HANLDER **/

    void Windowing::init() {
        can_tick = true;
        glfwInit();
        glfwSetErrorCallback([](int i, const char* c){ std::cout << "Error code " << i << " :: " << c << "\n"; });
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        main_window = &world->create<Window>();
        assert(main_window);
        //main_window = new Window();
        //world->create<test>();
        //if( auto tst = &world->get<test>() ) {
        //    std::cout << tst->s;
        //} else {
        //    std::cout << "nullptr";
        //}
    }

    void Windowing::terminate() {
        glfwDestroyWindow(main_window->instance);
        glfwTerminate();
    };

    void Windowing::tick() {
        glfwPollEvents();
        if( glfwWindowShouldClose(main_window->instance) ) {
            world->request_exit = true;
        }
    };


/** INPUT **/

    void Input::init() {
        window = world->get<Window>().instance;
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
