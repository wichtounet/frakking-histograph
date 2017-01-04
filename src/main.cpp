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

namespace {

static constexpr bool debug = false;

struct point {
    float x;
    float y;

    point(float x, float y) : x(x), y(y) {}
};

} // end of anonymous namespace

int main(int argc, char* argv[]) {
    // Get the dataset path
    std::string dataset = "/home/wichtounet/datasets/histograph";
    if (argc > 1) {
        dataset = argv[1];
    }

    std::string data_target_folder = "/home/wichtounet/datasets/washington/data/";
    if (argc > 2) {
        dataset = argv[2];
    }

    auto locations_folder     = dataset + "/gt/locations/01/";
    auto raw_target_folder    = data_target_folder + "/word_raw/";
    auto gray_target_folder   = data_target_folder + "/word_gray/";
    auto binary_target_folder = data_target_folder + "/word_binary/";

    std::cout << "Start reading frakking SVG folder:" << locations_folder << std::endl;

    // TODO
    struct dirent* entry;
    auto dir = opendir(locations_folder.c_str());

    if (debug) {
        cv::namedWindow("image_window");
    }

    while ((entry = readdir(dir))) {
        std::string file_name(entry->d_name);

        if (file_name.size() <= 3 || file_name.find(".svg") != file_name.size() - 4) {
            continue;
        }

        auto image_name = file_name.substr(0, file_name.find(".svg"));

        std::cout << "Found frakking image:" << image_name << std::endl;

        std::string image_path = dataset + "/data/pages/" + image_name + ".jpg";
        auto image_mat         = cv::imread(image_path, CV_LOAD_IMAGE_ANYDEPTH);

        std::string full_name(locations_folder + "/" + file_name);

        std::ifstream stream(full_name);

        std::string line;
        while (std::getline(stream, line)) {
            if (line.find("<path ") == std::string::npos) {
                continue;
            }

            if (line.find("id=\"") == std::string::npos) {
                continue;
            }

            auto path_start = line.find("d=\"") + 3;
            auto path_end   = line.find("\"", path_start);
            auto path       = line.substr(path_start, path_end - path_start);

            auto id_start = line.find("id=\"") + 4;
            auto id_end   = line.find("\"", id_start);
            auto id       = line.substr(id_start, id_end - id_start);

            if (id == "null") {
                continue;
            }

            std::vector<point> points;

            std::istringstream ss(path);

            while (true) {
                char c;
                ss >> c;

                if (c == 'Z') {
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

            for (size_t i = 0; i < points.size(); ++i) {
                cv_points[i] = cv::Point(points[i].x, points[i].y);
            }

            std::vector<std::vector<cv::Point>> cv_points_points;
            cv_points_points.push_back(cv_points);

            if (debug) {
                cv::polylines(image_mat, cv_points_points, true, cv::Scalar(0), 1, 8);
            }

            auto rect = cv::minAreaRect(cv_points_points[0]);

            if (debug) {
                // Retarded OpenCV drawing rotated rect
                cv::Point2f rect_vertices[4];
                rect.points(rect_vertices);
                for (int i = 0; i < 4; i++) {
                    cv::line(image_mat, rect_vertices[i], rect_vertices[(i + 1) % 4], cv::Scalar(128));
                }
            }

            // Bounding Rect
            auto bounding_rect = rect.boundingRect();

            if (debug) {
                cv::rectangle(image_mat, bounding_rect, cv::Scalar(255));
            }

            cv::imwrite(raw_target_folder + "/" + id + ".png", image_mat(bounding_rect));

            if (debug) {
                cv::imshow("image_window", image_mat);
                cv::waitKey(-1);
            }

            cv::Mat resized = image_mat(bounding_rect);

            if (resized.size().height != 120) {
                auto ratio = (float)120 / resized.size().height;
                cv::resize(resized, resized, cv::Size(resized.size().width * ratio, 120), 0, 0, CV_INTER_CUBIC);
            }

            cv::imwrite(gray_target_folder + "/" + id + ".png", resized);
        }
    }

    return 0;
}
