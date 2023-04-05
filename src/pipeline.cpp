#include "pipeline.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace sigil {
    Pipeline::Pipeline(const std::string& vertex_path, const std::string frag_path) {
        create_graphics_pipeline(vertex_path, frag_path);
    }

    std::vector<char> Pipeline::read_file(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if( !file.is_open() ) {
            throw std::runtime_error("Error: Failed to open file at:" + path);
        }

        size_t file_size = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(file_size);

        file.seekg(0);
        file.read(buffer.data(), file_size);

        file.close();
        return buffer;
    }

    void Pipeline::create_graphics_pipeline(const std::string& vertex_path, const std::string fragment_path) {
        std::vector<char> vertex = read_file(vertex_path);
        std::vector<char> fragment = read_file(fragment_path);

        std::cout << "Vertex shader code size: " << vertex.size() << '\n';
        std::cout << "Fragment shader code size: " << fragment.size() << '\n';
    }
}
