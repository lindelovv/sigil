#pragma once

#include <any>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <optional>

namespace sigil {
    using entity_t = std::size_t;

    class id {
        inline static entity_t identifier {};
    public:
        template <typename>
        inline static const entity_t of_type = identifier++;
    };

    template <class resource_t>
    class sparse_set {
        // @TODO: fix up and optimize
        public:
            const resource_t& emplace(entity_t id, resource_t&& instance) {
                std::size_t dense_id = dense.emplace_back(id);
                sparse.emplace(sparse.begin() + id, dense_id);
                components.emplace_back(std::make_any(instance));
                return instance;
            }
            resource_t& at(entity_t id) {
                std::cout << id << ", sparse id: " << sparse.at(id) << "\n";
                if( components.size() <= sparse.at(id) ) {
                    throw std::runtime_error("\n>>>\tError: Tried to access uninitialized component.");
                }
                return components.at(sparse.at(id));
            }
        private:
            std::vector<entity_t> sparse;
            std::vector<entity_t> dense;
            std::vector<resource_t> components;
    };

    struct system_t {
        virtual ~system_t()       {};
        virtual void init()       {};
        virtual void terminate()  {};
        virtual void tick()       {};
        bool can_tick = false;
        class sigil_t* world;
    };
    
    class sigil_t {
        public:
            inline void run() {
                for( auto& system : systems ) {
                    system->world = this;
                    system->init();
                }
                while( !request_exit ) {
                    for( auto& system : systems ) {
                        //std::cout << "pretick\n";
                        if( system->can_tick ) {
                            //std::cout << "tick\n";
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
                system->world = this;
                systems.push_back(std::unique_ptr<SystemType>(system));
                return *this;
            }

            template <typename resource_t>
            inline sigil_t& add(entity_t id = 0) {
                create<resource_t>();
                return *this;
            }

            template <typename resource_t>
            inline resource_t& create(entity_t entity_id = 0, resource_t* instance = new resource_t {}) {
                auto resource_id = id::of_type<resource_t>;
                if( pools.size() >= resource_id ) {
                    pools.emplace(pools.begin() + resource_id, sparse_set<std::any>{});
                }
                std::any resource = pools.at(resource_id).emplace(entity_id, instance);
                return std::any_cast<resource_t&>(resource);
            }

            template <typename resource_t>
            inline resource_t& get(entity_t id = 0) {
                auto resource_id = id::of_type<resource_t>;
                if( pools.size() <= resource_id ) {
                    throw std::runtime_error("\n>>>\tError: Tried to access component type that has never been created.");
                }
                return std::any_cast<resource_t&>(pools.at(resource_id).at(id));
            }

            std::vector<entity_t> entities { 0 };
            std::vector<sparse_set<std::any>> pools;
            std::vector<std::unique_ptr<system_t>> systems;
            bool request_exit = false;
    };

    inline sigil_t& init() {
        auto& core = *new sigil_t();
        return core;
    }
}

