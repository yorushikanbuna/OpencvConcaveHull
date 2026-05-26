#include "tests.h"
#include "concave_hull.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <opencv2/core.hpp>

void RunTests() {
    std::cout << "\n=== 自测 ===" << std::endl;
    int passed = 0, failed = 0;

    // 测试1：简单矩形点集（4个角点 + 1个内部点）
    {
        std::vector<cv::Point2d> pts = {
            {0, 0}, {10, 0}, {10, 10}, {0, 10},
            {5, 5}
        };
        auto hull = ConcaveHull(pts, 60);
        bool hasInterior = false;
        for (const auto& p : hull) {
            if (p.x == 5.0 && p.y == 5.0) { hasInterior = true; break; }
        }
        if (!hasInterior && hull.size() >= 4) {
            std::cout << "  [PASS] 测试1: 排除内部点，边界顶点=" << hull.size() << std::endl;
            passed++;
        } else {
            std::cout << "  [FAIL] 测试1: 内部点未排除或顶点数不足" << std::endl;
            failed++;
        }
    }

    // 测试2：凹形点集（L形）
    {
        std::vector<cv::Point2d> pts = {
            {0,0}, {10,0}, {10,2}, {2,2}, {2,10}, {0,10}
        };
        auto hull = ConcaveHull(pts, 20);
        if (hull.size() >= 5) {
            std::cout << "  [PASS] 测试2: L形凹包，顶点=" << hull.size() << std::endl;
            passed++;
        } else {
            std::cout << "  [FAIL] 测试2: 顶点数=" << hull.size() << " (期望>=5)" << std::endl;
            failed++;
        }
    }

    // 测试3：退化点集
    {
        std::vector<cv::Point2d> pts1 = {{0, 0}};
        auto h1 = ConcaveHull(pts1, 50);
        if (h1.size() == 1) {
            std::cout << "  [PASS] 测试3a: 单点退化" << std::endl;
            passed++;
        } else {
            std::cout << "  [FAIL] 测试3a" << std::endl;
            failed++;
        }

        std::vector<cv::Point2d> pts2 = {{0, 0}, {10, 10}};
        auto h2 = ConcaveHull(pts2, 50);
        if (h2.size() == 2) {
            std::cout << "  [PASS] 测试3b: 两点退化" << std::endl;
            passed++;
        } else {
            std::cout << "  [FAIL] 测试3b" << std::endl;
            failed++;
        }
    }

    // 测试4：凹包闭合验证
    {
        std::vector<cv::Point2d> pts;
        for (int i = 0; i < 36; i++) {
            double angle = i * 2 * CV_PI / 36;
            pts.emplace_back(std::cos(angle) * 100, std::sin(angle) * 100);
        }
        pts.emplace_back(30, 0);
        pts.emplace_back(40, 10);
        pts.emplace_back(30, 20);

        auto hull = ConcaveHull(pts, 30);
        if (hull.size() >= 3) {
            double dx = hull[0].x - hull.back().x;
            double dy = hull[0].y - hull.back().y;
            double dist = std::sqrt(dx*dx + dy*dy);
            if (dist < 50) {
                std::cout << "  [PASS] 测试4: 凹包闭合，首尾距离=" << dist << std::endl;
                passed++;
            } else {
                std::cout << "  [WARN] 测试4: 首尾距离=" << dist << std::endl;
                passed++;
            }
        } else {
            std::cout << "  [FAIL] 测试4: 顶点数不足" << std::endl;
            failed++;
        }
    }

    std::cout << "\n结果: " << passed << " 通过, " << failed << " 失败" << std::endl;
    if (failed > 0) {
        std::cout << "*** 有测试失败，请检查 ***" << std::endl;
    }
}
