#pragma once

#include <concepts>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace sigil {    

    class Resource {
        public:
            Resource() :type_id(-1) {};
            Resource(std::size_t id) : type_id(id) {}
            Resource& operator=(const Resource& o) { return *new Resource(o.type_id); };
            const std::size_t type_id;
            virtual std::size_t get_id() { return type_id; };
            virtual ~Resource() {};
    };

    struct SharedResource : Resource { using Resource::Resource; };

    struct Component : Resource { using Resource::Resource; };

    struct Transform : Component {
        using Component::Component;
        glm::vec3 position { 0, 0 ,0 };
        glm::vec3 rotation { 0, 0 ,0 };
        glm::vec3 scale    { 0, 0 ,0 };
    };

    struct Velocity : Component {
        using Component::Component;
        glm::vec3 movement { 0, 0 ,0 };
        glm::vec3 rotation { 0, 0 ,0 };
    };

    struct Lens : Component {
        using Component::Component;
    };

    class System : public Resource {
        public:
            using Resource::Resource;
            virtual void init()      {};
            virtual void terminate() {};
            virtual void tick()      {};
            class Core* core;
    };

    class Id {
        static std::size_t id() {
            static std::size_t value = 0;
            return value++;
        }
        static std::size_t subid() {
            static std::size_t value = 0;
            return value++;
        }
        public:    
            template <typename>
            static std::size_t type() {
                static std::size_t data_id = id();
                return data_id;
            }
            template <typename>
            static std::size_t subtype() {
                static std::size_t data_id = subid();
                return data_id;
            }
    };

    class Core {
        public:
            inline void run() {
                auto systems = get_all<System>();
                for( auto& system : systems ) {
                    system->core = this;
                    system->init();
                }
                while( !request_exit ) {
                    for( auto& system : systems ) {
                        if( request_exit ) break;
                        system->tick();
                    }
                }
                for( auto& system : systems ) {
                    system->terminate();
                }
            }

            template <std::derived_from<Resource> ResourceType>
            inline std::vector<ResourceType*> get_all() {
                std::size_t resource_id = Id::type<ResourceType>();
                if( resources.size() <= resource_id ) {
                    throw std::runtime_error("Error:\n\tUninitialized resource type requested.");
                }
                std::vector<ResourceType*> typecast_resources;
                for( auto resource : resources.at(resource_id) ) {
                    typecast_resources.push_back(dynamic_cast<ResourceType*>(resource));
                }
                return typecast_resources;
            }

            template <std::derived_from<Resource>     ResourceType,
                      std::derived_from<ResourceType> InstanceType>
            inline InstanceType* get() {
                std::size_t resource_id = Id::type<ResourceType>();
                std::size_t instance_id = Id::subtype<InstanceType>();
                if( resources.size() <= resource_id || resources.at(resource_id).size() <= instance_id) {
                    return nullptr;
                }
                return dynamic_cast<InstanceType*>(resources.at(resource_id).at(instance_id));
            }

            template <std::derived_from<Resource>     ResourceType,
                      std::derived_from<ResourceType> InstanceType>
            inline InstanceType* create() {
                std::size_t resource_id = Id::type<ResourceType>();
                std::size_t instance_id = Id::subtype<InstanceType>();
                std::cout << "type_id: " << resource_id << ", instance_id: " << instance_id << "\n";
                if( auto resource = new InstanceType(instance_id) ) {
                    if( resources.size() <= resource_id ) {
                        resources.emplace(
                            resources.begin() + resource_id,
                            std::vector<Resource*>{}
                        );
                    }
                    auto* type_vector = &resources.at(resource_id);
                    if( type_vector->size() <= resource_id ) {
                        type_vector->push_back(resource);
                    }
                    type_vector->emplace(type_vector->begin() + instance_id, resource);
                    return resource;
                } else {
                    throw std::runtime_error("Error:\n\tTried to create invalid object instance.");
                }
            }

            template <std::derived_from<Resource>     ResourceType,
                      std::derived_from<ResourceType> InstanceType>
            inline Core& add() {
                create<ResourceType, InstanceType>();
                return *this;
            }

            std::vector<std::vector<Resource*>> resources;
            bool request_exit = false;
    };

    inline Core& init() {
        static Core* core = new Core;
        return *core;
    }
}

