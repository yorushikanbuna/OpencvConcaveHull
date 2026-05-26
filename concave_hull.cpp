#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

// 硬编码POS文件路径
const std::string POS_PATH = R"(E:\ImageDataset\pozhuangcun\pos.txt)";

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
        // 跳过空行
        if (line.empty()) continue;
        // 跳过注释行
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

int main(int argc, char* argv[]) {
    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

    // 1. 解析POS文件
    auto points = ParsePosFile(POS_PATH);
    std::cout << "点集大小: " << points.size() << std::endl;

    return 0;
}
