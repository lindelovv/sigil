#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_ASSERT
#include <vulkan/vulkan.hpp>

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>

namespace sigil {
    
    template <typename T>
    static T unwrap(vk::ResultValue<T> result) {
        if( result.result != vk::Result::eSuccess ) {
            throw std::runtime_error("\tError: "
                                      + static_cast<std::string>(typeid(result.value).name())
                                      + "\n");
        } else {
            return result.value;
        }
    };

    template <typename T>
    static T expect(std::string msg, vk::ResultValue<T> result) {
        if( result.result != vk::Result::eSuccess ) {
            throw std::runtime_error( "\tError: "
                                      + msg 
                                      + " :: " 
                                      + static_cast<std::string>(typeid(result.value).name())
                                      + "\n");
        } else {
            return result.value;
        }
    };

    static void expect(std::string msg, vk::Result result) {
        if( result != vk::Result::eSuccess ) {
            throw std::runtime_error( "\tError: "
                                      + msg 
                                      + " :: " 
                                      + static_cast<std::string>(typeid(result).name())
                                      + "\n");
        }
    };

    static void expect(std::string msg, VkResult result) {
        if( result != VK_SUCCESS ) {
            throw std::runtime_error( "\tError: "
                                      + msg 
                                      + " :: " 
                                      + static_cast<std::string>(typeid(result).name())
                                      + "\n");
        }
    };
    
    template <typename T>
    static void expect(std::string msg, T value) {
        if( value == nullptr ) {
            throw std::runtime_error( "\tError: "
                                      + msg 
                                      + " :: " 
                                      + static_cast<std::string>(typeid(value).name())
                                      + "\n");
        }
    };

    static void expect(std::string msg, bool value) {
        if( !value ) {
            throw std::runtime_error( "\tError: "
                                      + msg 
                                      + " :: " 
                                      + static_cast<std::string>(typeid(value).name())
                                      + "\n");
        }
    };
}

