#include "sigil.hh"

namespace sigil {

    //    CORE    //

    void Sigil::run() {
        for( auto& module : modules ) {
            module->init();
        }
        while( !should_close ) {
            for( auto& module : modules ) {
                module->tick();
            }
        }
        for( auto& module : modules ) {
            module->terminate();
        }
    }

    template <std::derived_from<Module> T>
    Sigil& Sigil::add_module() {
        auto module = new T;
        ALLOW_UNUSED(Id::of_type<T>); // generate id (and thus index) for module
        modules.push_back(std::unique_ptr<Module>(module));
        return *this;
    }

    //    MODULE    //

    template <std::derived_from<Module> T>
    T* Module::get_module() {
        return dynamic_cast<T*>(core->modules.at(Id::of_type<T>).get());
    }

    void Module::request_exit() {
        core->should_close = true;
    }
}

