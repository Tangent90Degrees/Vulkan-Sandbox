//
// Created by Victor Liu on 11/22/26.
//

#ifndef VULKAN_UTILS_H
#define VULKAN_UTILS_H

#include <fstream>
#include <iostream>


template <typename... Args>
void show_error_message(Args &&... args) {
    (std::cerr << ... << std::forward<Args>(args)) << '\n';
}

std::string read_text_file(const std::filesystem::path &path);


#endif //VULKAN_UTILS_H
