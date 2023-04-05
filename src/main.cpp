#include "engine.h"
#include <cstdlib>
#include <iostream>
#include <exception>

int main() {
    sigil::Engine sigil{};

    try { sigil.run(); } catch(const std::exception &e) {
                            std::cerr << e.what() << '\n';
                            return EXIT_FAILURE;
                        };
    return EXIT_SUCCESS;
}
