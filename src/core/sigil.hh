#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <optional>

namespace sigil {
    using entity_t = std::size_t;

    class id_t {
        static ::std::size_t id() noexcept {
            static std::size_t value = 0;
            return value++;
        }
    public:
        template <typename>
        static std::size_t type() noexcept {
            static const std::size_t value = id();
            return value;
        }
    };

    class system_t {
        public:
            virtual ~system_t()       {};
            virtual void init()       {};
            virtual void terminate()  {};
            virtual void tick()       {};
            bool can_tick = false;
    };

    template <typename resource_t>
    class sparse_set_t {
        public:
            template <typename insert_t>
            resource_t* insert(entity_t id) {
                entity_t dense_id = dense.emplace_back(id);
                sparse.emplace(sparse.begin() + id, dense_id);
                return &components.emplace_back(insert_t {});
            }

            resource_t* at(entity_t id) {
                return &components.at(sparse.at(id));
            }

        private:
            std::vector<entity_t> sparse;
            std::vector<entity_t> dense;
            std::vector<resource_t> components;
    };

    class sigil_t {
        public:
            sigil_t() {}
            inline void run() {
                for( auto& system : systems ) {
                    system->init();
                }
                while( !request_exit ) {
                    for( auto& system : systems ) {
                        if( system->can_tick ) {
                            system->tick();
                        }
                    }
                }
                for( auto& system : systems ) {
                    system->terminate();
                }
            }

            template <std::derived_from<system_t> SystemType>
            inline sigil_t& add_system() {
                systems.push_back(std::shared_ptr<SystemType>(new SystemType));
                return get_core();
            }

            inline static sigil_t& get_core() {
                static sigil_t core;
                return core;
            }

            std::vector<std::shared_ptr<system_t>> systems;
            std::vector<sparse_set_t<void*>*> pools;
            bool request_exit = false;
    };

    inline sigil_t& init() {
        return sigil_t::get_core();
    }

    template <typename resource_t>
    inline resource_t* get(entity_t id = 0) {
        auto resource_id = id_t::type<resource_t>();
        void* resource = sigil_t::get_core().pools.at(resource_id)->at(id);
        return (resource_t*)resource;
    }

    template <typename resource_t>
    inline resource_t* create(entity_t id = 0) {
        auto sigil_core = sigil_t::get_core();
        auto resource_id = id_t::type<resource_t>();
        sparse_set_t<void*>* pool;
        std::cout << resource_id << " \n";
        if( sigil_core.pools.size() <= resource_id ) {
            sigil_core.pools.insert(sigil_core.pools.begin() + resource_id, new sparse_set_t<void*>());
        }
        pool = sigil_core.pools.at(resource_id);
        void* instance = *pool->insert(id);
        resource_t* resource {};
        memcpy(&instance, &resource, sizeof(resource_t));
        return resource;
    }

    inline void request_exit() {
        sigil_t::get_core().request_exit = true;
    }
}

