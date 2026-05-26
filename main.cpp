#include <iostream>
#include <cmath>
#include <string>
#include <algorithm>

#include "pos_reader.h"
#include "concave_hull.h"
#include "output.h"
#include "tests.h"

// 硬编码POS文件路径
const std::string POS_PATH = R"(E:\v3d_proj\new_extractor\new_extractor\fenghuo\pos.txt)";

int main(int argc, char* argv[]) {
    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

    // 自测
    RunTests();

    // 1. 解析POS文件
    auto points = ParsePosFile(POS_PATH);

    // 2. 确定k值：auto + CLI覆盖
    int k = static_cast<int>(std::sqrt(points.size()));
    k = std::clamp(k, 3, 60);
    if (argc >= 2) {
        k = std::atoi(argv[1]);
        k = std::max(1, k);
        std::cout << "使用命令行参数 k=" << k << std::endl;
    } else {
        std::cout << "自动确定 k=" << k << " (sqrt(N))" << std::endl;
    }

    // 3. 计算凹包
    auto hull = ConcaveHull(points, k);

    // 4. 保存凹包文本
    std::string parentDir = POS_PATH.substr(0, POS_PATH.find_last_of("\\/"));
    std::string txtPath = parentDir + "/concave_hull.txt";
    SaveHullToFile(hull, txtPath, static_cast<int>(points.size()), k);

    // 5. 可视化
    std::string imgPath = parentDir + "/concave_hull.png";
    Visualize(points, hull, imgPath);

    return 0;
}
