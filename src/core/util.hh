#pragma once

#define VULKAN_HPP_NO_EXCEPTIONS
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

    
template <typename T>
inline static T unwrap(vk::ResultValue<T> result) {
    if( result.result != vk::Result::eSuccess ) {
        throw std::runtime_error("\nError: "
                                  + static_cast<std::string>(typeid(result.value).name())
                                  + "\n");
    } else {
        return result.value;
    }
};

template <typename T>
inline static T expect(std::string msg, vk::ResultValue<T> result) {
    if( result.result != vk::Result::eSuccess ) {
        throw std::runtime_error( "\nError: " + msg + " :: " 
                                  + static_cast<std::string>(typeid(result.value).name())
                                  + "\n");
    } else {
        return result.value;
    }
};

inline static void expect(std::string msg, vk::Result result) {
    if( result != vk::Result::eSuccess ) {
        throw std::runtime_error( "\nError: " + msg + " :: " 
                                  + static_cast<std::string>(typeid(result).name())
                                  + "\n");
    }
};

inline static void expect(std::string msg, VkResult result) {
    if( result != VK_SUCCESS ) {
        throw std::runtime_error( "\nError: " + msg + " :: " 
                                  + static_cast<std::string>(typeid(result).name())
                                  + "\n");
    }
};

template <typename T>
inline static void expect(std::string msg, T* value) {
    if( value == nullptr ) {
        throw std::runtime_error( "\nError: " + msg + " :: " 
                                  + static_cast<std::string>(typeid(value).name())
                                  + "\n");
    }
};

inline static void expect(std::string msg, bool value) {
    if( !value ) {
        throw std::runtime_error( "\nError: " + msg + " :: " 
                                  + static_cast<std::string>(typeid(value).name())
                                  + "\n");
    }
};

