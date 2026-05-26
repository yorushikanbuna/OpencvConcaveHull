#include "pos_reader.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

std::vector<cv::Point2d> ParsePosFile(const std::string& path) {
    std::vector<cv::Point2d> points;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "错误：无法打开文件 " << path << std::endl;
        exit(1);
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        if (line.empty()) continue;
        if (line[0] == '#') continue;

        std::istringstream iss(line);
        std::string imageName;
        double x = 0.0, y = 0.0, z = 0.0;

        if (!(iss >> imageName >> x >> y >> z)) {
            std::cerr << "警告：第" << lineNum << "行格式异常，跳过" << std::endl;
            continue;
        }

        points.emplace_back(x, y);
    }

    if (points.empty()) {
        std::cerr << "错误：文件中没有有效数据行" << std::endl;
        exit(1);
    }

    // 去重
    std::sort(points.begin(), points.end(),
        [](const cv::Point2d& a, const cv::Point2d& b) {
            if (a.x != b.x) return a.x < b.x;
            return a.y < b.y;
        });
    auto last = std::unique(points.begin(), points.end(),
        [](const cv::Point2d& a, const cv::Point2d& b) {
            return a.x == b.x && a.y == b.y;
        });
    points.erase(last, points.end());

    std::cout << "解析完成：" << points.size() << " 个有效点（已去重）" << std::endl;
    return points;
}
