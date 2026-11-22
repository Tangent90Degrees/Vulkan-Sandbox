//
// Created by Victor Liu on 11/22/26.
//

#include "utils.h"
#include <sstream>
#include <stdexcept>


std::string read_text_file(const std::filesystem::path &path) {
    if (std::ifstream file(path, std::ios::binary); file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string output = buffer.str();
        file.close();
        return output;
    }
    return std::string {};
}
