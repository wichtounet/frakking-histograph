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

// The binarization code is from Christian Wolf:
// http://liris.cnrs.fr/christian.wolf/software/binarize/
// If you use this code, you need to cite their ICPR 2002 Paper!

enum NiblackVersion {
    NIBLACK=0,
    SAUVOLA,
    WOLFJOLION,
};

#define BINARIZEWOLF_VERSION    "2.4 (August 1st, 2014)"

#define uget(x,y)    at<unsigned char>(y,x)
#define uset(x,y,v)  at<unsigned char>(y,x)=v;
#define fget(x,y)    at<float>(y,x)
#define fset(x,y,v)  at<float>(y,x)=v;

double calcLocalStats(cv::Mat& im, cv::Mat& map_m, cv::Mat& map_s, int winx, int winy) {
    cv::Mat im_sum, im_sum_sq;
    cv::integral(im, im_sum, im_sum_sq, CV_64F);

    double m, s, max_s, sum, sum_sq;
    int wxh        = winx / 2;
    int wyh        = winy / 2;
    int x_firstth  = wxh;
    int y_lastth   = im.rows - wyh - 1;
    int y_firstth  = wyh;
    double winarea = winx * winy;

    max_s = 0;
    for (int j = y_firstth; j <= y_lastth; j++) {
        sum = sum_sq = 0;

        sum    = im_sum.at<double>(j - wyh + winy, winx) - im_sum.at<double>(j - wyh, winx) - im_sum.at<double>(j - wyh + winy, 0) + im_sum.at<double>(j - wyh, 0);
        sum_sq = im_sum_sq.at<double>(j - wyh + winy, winx) - im_sum_sq.at<double>(j - wyh, winx) - im_sum_sq.at<double>(j - wyh + winy, 0) + im_sum_sq.at<double>(j - wyh, 0);

        m = sum / winarea;
        s = sqrt((sum_sq - m * sum) / winarea);
        if (s > max_s)
            max_s = s;

        map_m.fset(x_firstth, j, m);
        map_s.fset(x_firstth, j, s);

        // Shift the window, add and remove new/old values to the histogram
        for (int i = 1; i <= im.cols - winx; i++) {
            // Remove the left old column and add the right new column
            sum -= im_sum.at<double>(j - wyh + winy, i) - im_sum.at<double>(j - wyh, i) - im_sum.at<double>(j - wyh + winy, i - 1) + im_sum.at<double>(j - wyh, i - 1);
            sum += im_sum.at<double>(j - wyh + winy, i + winx) - im_sum.at<double>(j - wyh, i + winx) - im_sum.at<double>(j - wyh + winy, i + winx - 1) + im_sum.at<double>(j - wyh, i + winx - 1);

            sum_sq -= im_sum_sq.at<double>(j - wyh + winy, i) - im_sum_sq.at<double>(j - wyh, i) - im_sum_sq.at<double>(j - wyh + winy, i - 1) + im_sum_sq.at<double>(j - wyh, i - 1);
            sum_sq += im_sum_sq.at<double>(j - wyh + winy, i + winx) - im_sum_sq.at<double>(j - wyh, i + winx) - im_sum_sq.at<double>(j - wyh + winy, i + winx - 1) + im_sum_sq.at<double>(j - wyh, i + winx - 1);

            m = sum / winarea;
            s = sqrt((sum_sq - m * sum) / winarea);
            if (s > max_s)
                max_s = s;

            map_m.fset(i + wxh, j, m);
            map_s.fset(i + wxh, j, s);
        }
    }

    return max_s;
}
void NiblackSauvolaWolfJolion (cv::Mat im, cv::Mat output, NiblackVersion version, int winx, int winy, double k, double dR) {
    double m, s, max_s;
    double th = 0;
    double min_I, max_I;
    int wxh       = winx / 2;
    int wyh       = winy / 2;
    int x_firstth = wxh;
    int x_lastth  = im.cols - wxh - 1;
    int y_lastth  = im.rows - wyh - 1;
    int y_firstth = wyh;

    // Create local statistics and store them in a double matrices
    cv::Mat map_m = cv::Mat::zeros(im.rows, im.cols, CV_32F);
    cv::Mat map_s = cv::Mat::zeros(im.rows, im.cols, CV_32F);
    max_s         = calcLocalStats(im, map_m, map_s, winx, winy);

    minMaxLoc(im, &min_I, &max_I);

    cv::Mat thsurf(im.rows, im.cols, CV_32F);

    // Create the threshold surface, including border processing
    // ----------------------------------------------------

    for (int j = y_firstth; j <= y_lastth; j++) {
        // NORMAL, NON-BORDER AREA IN THE MIDDLE OF THE WINDOW:
        for (int i = 0; i <= im.cols - winx; i++) {
            m = map_m.fget(i + wxh, j);
            s = map_s.fget(i + wxh, j);

            // Calculate the threshold
            switch (version) {
                case NIBLACK:
                    th = m + k * s;
                    break;

                case SAUVOLA:
                    th = m * (1 + k * (s / dR - 1));
                    break;

                case WOLFJOLION:
                    th = m + k * (s / max_s - 1) * (m - min_I);
                    break;

                default:
                    std::cerr << "Unknown threshold type in ImageThresholder::surfaceNiblackImproved()\n";
                    exit(1);
            }

            thsurf.fset(i + wxh, j, th);

            if (i == 0) {
                // LEFT BORDER
                for (int i = 0; i <= x_firstth; ++i)
                    thsurf.fset(i, j, th);

                // LEFT-UPPER CORNER
                if (j == y_firstth)
                    for (int u = 0; u < y_firstth; ++u)
                        for (int i = 0; i <= x_firstth; ++i)
                            thsurf.fset(i, u, th);

                // LEFT-LOWER CORNER
                if (j == y_lastth)
                    for (int u = y_lastth + 1; u < im.rows; ++u)
                        for (int i = 0; i <= x_firstth; ++i)
                            thsurf.fset(i, u, th);
            }

            // UPPER BORDER
            if (j == y_firstth)
                for (int u = 0; u < y_firstth; ++u)
                    thsurf.fset(i + wxh, u, th);

            // LOWER BORDER
            if (j == y_lastth)
                for (int u = y_lastth + 1; u < im.rows; ++u)
                    thsurf.fset(i + wxh, u, th);
        }

        // RIGHT BORDER
        for (int i = x_lastth; i < im.cols; ++i)
            thsurf.fset(i, j, th);

        // RIGHT-UPPER CORNER
        if (j == y_firstth)
            for (int u = 0; u < y_firstth; ++u)
                for (int i = x_lastth; i < im.cols; ++i)
                    thsurf.fset(i, u, th);

        // RIGHT-LOWER CORNER
        if (j == y_lastth)
            for (int u = y_lastth + 1; u < im.rows; ++u)
                for (int i = x_lastth; i < im.cols; ++i)
                    thsurf.fset(i, u, th);
    }

    for (int y = 0; y < im.rows; ++y)
        for (int x = 0; x < im.cols; ++x) {
            if (im.uget(x, y) >= thsurf.fget(x, y)) {
                output.uset(x, y, 255);
            } else {
                output.uset(x, y, 0);
            }
        }
}


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
        cv::namedWindow("binary_window");
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
        auto debug_mat         = image_mat.clone();

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
                cv::polylines(debug_mat, cv_points_points, true, cv::Scalar(0), 1, 8);
            }

            auto rect = cv::minAreaRect(cv_points_points[0]);

            if (debug) {
                // Retarded OpenCV drawing rotated rect
                cv::Point2f rect_vertices[4];
                rect.points(rect_vertices);
                for (int i = 0; i < 4; i++) {
                    cv::line(debug_mat, rect_vertices[i], rect_vertices[(i + 1) % 4], cv::Scalar(128));
                }
            }

            // Bounding Rect
            auto bounding_rect = rect.boundingRect();

            if (debug) {
                cv::rectangle(debug_mat, bounding_rect, cv::Scalar(255));
            }

            cv::imwrite(raw_target_folder + "/" + id + ".png", image_mat(bounding_rect));

            if (debug) {
                cv::imshow("image_window", debug_mat);
            }

            cv::Mat resized = image_mat(bounding_rect);

            if (resized.size().height != 120) {
                auto ratio = (float)120 / resized.size().height;
                cv::resize(resized, resized, cv::Size(resized.size().width * ratio, 120), 0, 0, CV_INTER_CUBIC);
            }

            cv::imwrite(gray_target_folder + "/" + id + ".png", resized);

            cv::Mat binary = resized.clone();

            NiblackSauvolaWolfJolion (resized, binary, WOLFJOLION, 20, 20, 0.5, 128);

            if (debug) {
                cv::imshow("binary_window", binary);
                cv::waitKey(-1);
            }

            cv::imwrite(binary_target_folder + "/" + id + ".png", binary);
        }
    }

    return 0;
}
