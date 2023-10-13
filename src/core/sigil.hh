#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <cstring>
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
            inline virtual void link(class sigil_t* engine ) {
                core = engine;
            }
            bool can_tick = false;
            sigil_t* core;
    };

    template <typename resource_t>
    class sparse_set_t {
        public:
            resource_t* insert(entity_t id) {
                std::size_t dense_id = dense.emplace_back(id);
                sparse.emplace(sparse.begin() + id, dense_id);
                return &components.emplace_back(resource_t {});
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
                auto system = new SystemType;
                system->core = this;
                systems.push_back(std::unique_ptr<SystemType>(system));
                return *this;
            }

            template <typename resource_t>
            inline sigil_t& add(entity_t id = 0) {
                create<resource_t>();
                return *this;
            }

            template <typename resource_t>
            inline resource_t* create(entity_t id = 0) {
                auto resource_id = id_t::type<resource_t>();
                sparse_set_t<void*>* pool;
                if( pools.size() <= resource_id ) {
                    pools.insert(pools.begin() + resource_id, new sparse_set_t<void*>());
                }
                pool = pools.at(resource_id);
                void* instance = *pool->insert(id);
                resource_t* resource {};
                memcpy(&instance, &resource, sizeof(resource_t));
                return resource;
            }

            template <typename resource_t>
            inline resource_t* get(entity_t id = 0) {
                auto resource_id = id_t::type<resource_t>();
                void* instance = pools.at(resource_id)->at(id);
                resource_t* resource {};
                memcpy(&instance, &resource, sizeof(resource_t));
                return resource;
            }

            std::vector<std::unique_ptr<system_t>> systems;
            std::vector<sparse_set_t<void*>*> pools;
            bool request_exit = false;
    };

    struct test {
        test() { s = "test"; }
        std::string s;
    };

    inline sigil_t& init() {
        auto core = new sigil_t();
        return *core;
    }
}

