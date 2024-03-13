cmake_minimum_required(VERSION 3.18)

project(
    sigil
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(submodules)

aux_source_directory(./src/ SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})