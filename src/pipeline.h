#pragma once

#include <string>
#include <vector>
namespace sigil {
    class Pipeline {
        public:
            Pipeline(const std::string& vertex_path, const std::string frag_path);
        private:
            static std::vector<char> read_file(const std::string& path);
            void create_graphics_pipeline(const std::string& vertex_path, const std::string fragment_path);
    };
}
