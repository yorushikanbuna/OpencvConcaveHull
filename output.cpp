#include "output.h"

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

void SaveHullToFile(const std::vector<cv::Point2d>& hull,
                    const std::string& path,
                    int inputCount, int k) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "错误：无法创建输出文件 " << path << std::endl;
        exit(1);
    }

    file << "# 凹包结果 - 输入" << inputCount
         << "个点 -> 输出" << hull.size()
         << "个边界点 (k=" << k << ")" << std::endl;

    file.precision(12);
    for (const auto& p : hull) {
        file << std::fixed << p.x << " " << p.y << std::endl;
    }

    std::cout << "凹包文本已保存至: " << path << std::endl;
}

void Visualize(const std::vector<cv::Point2d>& points,
               const std::vector<cv::Point2d>& hull,
               const std::string& outputPath) {
    if (points.empty()) return;

    double minX = points[0].x, maxX = points[0].x;
    double minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    double marginX = (maxX - minX) * 0.10;
    double marginY = (maxY - minY) * 0.10;
    if (marginX < 1.0) marginX = 1.0;
    if (marginY < 1.0) marginY = 1.0;
    minX -= marginX; maxX += marginX;
    minY -= marginY; maxY += marginY;

    double dataW = maxX - minX;
    double dataH = maxY - minY;

    int imgW = 1920;
    int imgH = 1080;
    double dataAspect = dataW / dataH;
    double imgAspect = static_cast<double>(imgW) / imgH;
    if (dataAspect > imgAspect) {
        imgH = static_cast<int>(imgW / dataAspect);
    } else {
        imgW = static_cast<int>(imgH * dataAspect);
    }
    imgW = std::max(1, imgW);
    imgH = std::max(1, imgH);

    auto worldToImg = [&](const cv::Point2d& wp) -> cv::Point {
        int ix = static_cast<int>((wp.x - minX) / dataW * imgW);
        int iy = static_cast<int>((maxY - wp.y) / dataH * imgH);
        return cv::Point(ix, iy);
    };

    cv::Mat image(imgH, imgW, CV_8UC3, cv::Scalar(0, 0, 0));

    for (const auto& p : points) {
        cv::circle(image, worldToImg(p), 2, cv::Scalar(255, 0, 0), -1);
    }

    if (hull.size() >= 2) {
        std::vector<cv::Point> imgHull;
        imgHull.reserve(hull.size() + 1);
        for (const auto& p : hull) {
            imgHull.push_back(worldToImg(p));
        }
        imgHull.push_back(worldToImg(hull[0]));
        cv::polylines(image, imgHull, false, cv::Scalar(0, 0, 255), 2);
    }

    for (const auto& p : hull) {
        cv::circle(image, worldToImg(p), 4, cv::Scalar(0, 255, 0), -1);
    }

    cv::imwrite(outputPath, image);
    std::cout << "可视化图像已保存至: " << outputPath
              << " (" << imgW << "x" << imgH << ")" << std::endl;
}
