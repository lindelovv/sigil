#include "engine.hh"

#include <cstdlib>
#include <iostream>
#include <exception>

int main() {
    try { sigil::core.run(); } catch(const std::exception &error) {
                                 std::cerr << error.what() << '\n';
                                 return EXIT_FAILURE;
                             };
    return EXIT_SUCCESS;
}

