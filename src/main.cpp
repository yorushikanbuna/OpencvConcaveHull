#include <iostream>
#include <cmath>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#include "pos_reader.h"
#include "concave_hull.h"
#include "output.h"
#include "tests.h"

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc < 2) {
        std::cout << "用法: concave_hull <POS文件路径> [k值]" << std::endl;
        std::cout << "  POS文件路径  必需，格式: 影像名 X Y Z（空格分隔）" << std::endl;
        std::cout << "  k值          可选，控制凹包松紧 (3~60)，默认自动确定" << std::endl;
        std::cout << "  示例: concave_hull E:/data/pos.txt 30" << std::endl;
        return 1;
    }

    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

    // 自测
    RunTests();

    const std::string posPath = argv[1];

    // 1. 解析POS文件
    auto points = ParsePosFile(posPath);

    // 2. 确定k值：auto + CLI覆盖
    int k = static_cast<int>(std::sqrt(points.size()));
    k = std::clamp(k, 3, 60);
    if (argc >= 3) {
        k = std::atoi(argv[2]);
        k = std::max(1, k);
        std::cout << "使用命令行参数 k=" << k << std::endl;
    } else {
        std::cout << "自动确定 k=" << k << " (sqrt(N))" << std::endl;
    }

    // 3. 计算凹包
    auto hull = ConcaveHull(points, k);

    // 4. 保存凹包文本
    std::string parentDir = posPath.substr(0, posPath.find_last_of("\\/"));
    std::string txtPath = parentDir + "/concave_hull.txt";
    SaveHullToFile(hull, txtPath, static_cast<int>(points.size()), k);

    // 5. 可视化
    std::string imgPath = parentDir + "/concave_hull.png";
    Visualize(points, hull, imgPath);

    return 0;
}
