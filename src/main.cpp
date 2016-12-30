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

#include <opencv2/opencv.hpp>

struct point {
    float x;
    float y;

    point(float x, float y) : x(x), y(y) {}
};

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

    cv::namedWindow("image_window");

    while ((entry = readdir(dir))) {
        std::string file_name(entry->d_name);

        if (file_name.size() <= 3 || file_name.find(".svg") != file_name.size() - 4) {
            continue;
        }

        auto image_name = file_name.substr(0, file_name.find(".svg"));

        std::cout << "Found frakking image:" << image_name << std::endl;

        std::string image_path = dataset + "/data/pages/" + image_name + ".jpg";
        auto image_mat = cv::imread(image_path, CV_LOAD_IMAGE_ANYDEPTH);

        std::string full_name(locations_folder + "/" + file_name);

        std::ifstream stream(full_name);

        std::string line;
        while (std::getline(stream, line)) {
            if(line.find("<path ") == std::string::npos){
                continue;
            }

            if(line.find("id=\"") == std::string::npos){
                continue;
            }

            auto path_start = line.find("d=\"") + 3;
            auto path_end = line.find("\"", path_start);
            auto path = line.substr(path_start, path_end - path_start);

            auto id_start = line.find("id=\"") + 4;
            auto id_end = line.find("\"", id_start);
            auto id = line.substr(id_start, id_end - id_start);

            if(id == "null"){
                continue;
            }

            std::vector<point> points;

            std::istringstream ss(path);

            while(true){
                char c;
                ss >> c;

                if(c == 'Z'){
                    break;
                }

                float x;
                ss >> x;

                float y;
                ss >> y;

                points.emplace_back(x, y);
            }

            std::cout << id << ":" << points.size() << std::endl;

            std::reverse(points.begin(), points.end());

            std::vector<cv::Point> cv_points(points.size());

            for(size_t i = 0; i < points.size(); ++i){
                cv_points[i] = cv::Point(points[i].x, points[i].y);
            }

            std::vector<std::vector<cv::Point>> cv_points_points;
            cv_points_points.push_back(cv_points);

            cv::fillPoly( image_mat, cv_points_points, cv::Scalar( 0 ));

            cv::imshow("image_window", image_mat);
            cv::waitKey(-1);
        }
    }

    return 0;
}
