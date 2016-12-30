//=======================================================================
// Copyright (c) 2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>

#include <dirent.h>

#include "mnist/mnist_reader.hpp"
#include "mnist/mnist_utils.hpp"

int main(int argc, char* argv[]) {
    // Get the dataset path
    std::string dataset = "/home/wichtounet/datasets/histograph";
    if (argc > 1) {
        dataset = argv[1];
    }

    std::string locations_folder = dataset + "/gt/locations/01/";

    std::cout << "Start reading frakking SVG folder:" << locations_folder << std::endl;

    // TODO
    struct dirent* entry;
    auto dir = opendir(locations_folder.c_str());

    while ((entry = readdir(dir))) {
        std::string file_name(entry->d_name);

        if (file_name.size() <= 3 || file_name.find(".svg") != file_name.size() - 4) {
            continue;
        }

        std::cout << "Found frakking SVG:" << file_name << std::endl;

        std::string full_name(locations_folder + "/" + file_name);
    }

    return 0;
}
