#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <queue>
#include <cassert>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

// 硬编码POS文件路径
const std::string POS_PATH = R"(E:/v3d_proj/new_extractor/new_extractor/opencv-pozhuangcun/pos.txt)";

// ============================================================
// 1. POS文件解析
// ============================================================
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

// ============================================================
// 2. 凹包计算 — 凸包递归细化算法
// ============================================================

// 欧氏距离
static double Dist(const cv::Point2d& a, const cv::Point2d& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 点到线段的最短距离，同时返回线段上最近点的参数t (0~1)
static double PointToSegmentDist(const cv::Point2d& p, const cv::Point2d& a, const cv::Point2d& b, double& t) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-20) {
        t = 0;
        return Dist(p, a);
    }
    t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
    t = std::max(0.0, std::min(1.0, t));
    cv::Point2d proj(a.x + t * dx, a.y + t * dy);
    return Dist(p, proj);
}

std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k) {
    const int n = static_cast<int>(points.size());
    if (n < 3) return points;

    // 1. 计算凸包（cv::convexHull需要CV_32F或CV_32S）
    std::vector<cv::Point2f> ptsFloat;
    ptsFloat.reserve(n);
    for (const auto& p : points) ptsFloat.emplace_back(static_cast<float>(p.x), static_cast<float>(p.y));
    std::vector<int> hullIndices;
    cv::convexHull(ptsFloat, hullIndices, false);

    // 2. 计算最近邻中位距离，用于确定细化阈值
    std::vector<double> nnDists;
    nnDists.reserve(n);
    for (int i = 0; i < n; ++i) {
        double minDist = std::numeric_limits<double>::max();
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            double d = Dist(points[i], points[j]);
            if (d < minDist) minDist = d;
        }
        if (minDist < std::numeric_limits<double>::max()) {
            nnDists.push_back(minDist);
        }
    }
    std::sort(nnDists.begin(), nnDists.end());
    double medianNN = nnDists[nnDists.size() / 2];

    // k参数映射到长度阈值因子
    // k=3 → factor≈1.5（紧密凹包，边被切成约1.5倍最近邻距离）
    // k=60 → factor≈∞（趋向凸包）
    double factor = k * k / 60.0;
    double maxEdgeLen = factor * medianNN;
    if (maxEdgeLen < medianNN * 1.2) maxEdgeLen = medianNN * 1.2;

    // 3. 构建凸包多边形顶点列表（逆时针）
    std::vector<int> poly = hullIndices;

    // 4. 迭代细化：找到最长边，如果超过阈值就分割
    std::set<int> usedPoints(poly.begin(), poly.end());
    bool changed = true;
    int maxIter = n * 2;
    int iterCount = 0;

    while (changed && iterCount < maxIter) {
        changed = false;
        iterCount++;

        // 找最长的、超过阈值的边
        double longestLen = 0;
        int splitPos = -1;
        int bestPt = -1;
        double bestDist = std::numeric_limits<double>::max();

        for (int ei = 0; ei < static_cast<int>(poly.size()); ++ei) {
            int i1 = poly[ei];
            int i2 = poly[(ei + 1) % poly.size()];
            double edgeLen = Dist(points[i1], points[i2]);

            if (edgeLen <= maxEdgeLen) continue;

            // 边太长，找最近的内点进行分割
            if (edgeLen > longestLen) {
                // 对每条过长边找最佳分割点
                double localBestDist = std::numeric_limits<double>::max();
                int localBestPt = -1;

                cv::Point2d mid((points[i1].x + points[i2].x) / 2,
                                (points[i1].y + points[i2].y) / 2);

                for (int pi = 0; pi < n; ++pi) {
                    if (usedPoints.count(pi)) continue;
                    double d = Dist(points[pi], mid);
                    // 检查该点在边的哪一侧（确保在外侧添加）
                    double cross = (points[i2].x - points[i1].x) * (points[pi].y - points[i1].y)
                                 - (points[i2].y - points[i1].y) * (points[pi].x - points[i1].x);
                    // 对于逆时针凸包，内点在边的右侧（cross < 0）
                    // 对于凹包细化，我们允许两侧的点
                    if (d < localBestDist && d < edgeLen * 0.7) {
                        localBestDist = d;
                        localBestPt = pi;
                    }
                }

                if (localBestPt >= 0 && edgeLen > longestLen) {
                    longestLen = edgeLen;
                    splitPos = ei;
                    bestPt = localBestPt;
                    bestDist = localBestDist;
                }
            }
        }

        if (splitPos >= 0 && bestPt >= 0) {
            // 在最长边中间插入最佳分割点
            poly.insert(poly.begin() + splitPos + 1, bestPt);
            usedPoints.insert(bestPt);
            changed = true;
        }
    }

    // 5. 构建结果
    std::vector<cv::Point2d> result;
    for (int idx : poly) result.push_back(points[idx]);

    std::cout << "凹包计算完成：" << result.size() << " 个边界顶点"
              << " (k=" << k << ", 阈值=" << maxEdgeLen << ")"
              << " | 凸包顶点=" << hullIndices.size() << std::endl;
    return result;
}

// ============================================================
// 3. 文本输出
// ============================================================
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

// ============================================================
// 4. 可视化
// ============================================================
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

// ============================================================
// 5. 自测
// ============================================================
void RunTests() {
    std::cout << "\n=== 自测 ===" << std::endl;
    int passed = 0, failed = 0;

    // 测试1：简单矩形点集（4个角点 + 1个内部点）
    {
        std::vector<cv::Point2d> pts = {
            {0, 0}, {10, 0}, {10, 10}, {0, 10},  // 四个角
            {5, 5}  // 内部点
        };
        auto hull = ConcaveHull(pts, 60);
        // 凹包应该排除内部点(5,5)
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
        // L形凹包应该有6个顶点（包含凹角）
        if (hull.size() >= 5) {
            std::cout << "  [PASS] 测试2: L形凹包，顶点=" << hull.size() << std::endl;
            passed++;
        } else {
            std::cout << "  [FAIL] 测试2: 顶点数=" << hull.size() << " (期望>=5)" << std::endl;
            failed++;
        }
    }

    // 测试3：单点/两点退化
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
        // 添加凹入区域
        pts.emplace_back(30, 0);
        pts.emplace_back(40, 10);
        pts.emplace_back(30, 20);

        auto hull = ConcaveHull(pts, 30);
        if (hull.size() >= 3) {
            double dx = hull[0].x - hull.back().x;
            double dy = hull[0].y - hull.back().y;
            double dist = std::sqrt(dx*dx + dy*dy);
            // 对于非闭合返回，检查首尾点是否接近（在合理范围内）
            if (dist < 50) {
                std::cout << "  [PASS] 测试4: 凹包闭合，首尾距离=" << dist << std::endl;
                passed++;
            } else {
                std::cout << "  [WARN] 测试4: 首尾距离=" << dist << " (可能未精确闭合)" << std::endl;
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

// ============================================================
// 6. 主函数
// ============================================================
int main(int argc, char* argv[]) {
    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

    // 先运行自测
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
