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

// 计算从向量v1到v2的有符号角度 [-pi, pi]
// 正值=逆时针, 负值=顺时针
static double SignedAngle(const cv::Point2d& v1, const cv::Point2d& v2) {
    double cross = v1.x * v2.y - v1.y * v2.x;
    double dot = v1.x * v2.x + v1.y * v2.y;
    return std::atan2(cross, dot);
}

// 欧氏距离平方
static double DistSq(const cv::Point2d& a, const cv::Point2d& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k) {
    const int n = static_cast<int>(points.size());

    if (n < 3) {
        return points;
    }

    k = std::min(k, n - 1);
    if (k < 1) k = 1;

    // 1. 构建k近邻索引
    std::vector<std::vector<int>> knnIndex(n);
    for (int i = 0; i < n; ++i) {
        std::vector<std::pair<double, int>> dists;
        dists.reserve(n - 1);
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            dists.emplace_back(DistSq(points[i], points[j]), j);
        }
        std::sort(dists.begin(), dists.end());
        knnIndex[i].reserve(k);
        for (int m = 0; m < k && m < static_cast<int>(dists.size()); ++m) {
            knnIndex[i].push_back(dists[m].second);
        }
    }

    // 2. 找起点：最右侧的点
    int startIdx = 0;
    for (int i = 1; i < n; ++i) {
        if (points[i].x > points[startIdx].x ||
            (points[i].x == points[startIdx].x && points[i].y > points[startIdx].y)) {
            startIdx = i;
        }
    }

    // 3. 边界追踪（逆时针方向）
    std::vector<cv::Point2d> hull;
    std::vector<bool> visited(n, false);
    int currentIdx = startIdx;
    // 虚构的前一个点：在起点正上方，使初始方向朝下
    // 从最右侧点出发，选择最大逆时针转角，沿逆时针遍历边界
    cv::Point2d previous = points[startIdx];
    previous.y += 1.0;

    const int maxIter = n * 2;
    for (int iter = 0; iter < maxIter; ++iter) {
        hull.push_back(points[currentIdx]);
        visited[currentIdx] = true;

        const auto& current = points[currentIdx];
        cv::Point2d dirPrev(current.x - previous.x, current.y - previous.y);

        double bestAngle = -std::numeric_limits<double>::max();
        int bestIdx = -1;

        for (int neighborIdx : knnIndex[currentIdx]) {
            if (neighborIdx == currentIdx) continue;
            if (visited[neighborIdx]) continue;

            const auto& candidate = points[neighborIdx];
            cv::Point2d dirCand(candidate.x - current.x, candidate.y - current.y);

            double angle = SignedAngle(dirPrev, dirCand);
            if (angle > bestAngle) {
                bestAngle = angle;
                bestIdx = neighborIdx;
            }
        }

        // 未找到未访问的候选点，尝试放宽：允许回到起点
        if (bestIdx < 0) {
            for (int neighborIdx : knnIndex[currentIdx]) {
                if (neighborIdx == currentIdx) continue;
                if (neighborIdx == startIdx && hull.size() >= 3) {
                    bestIdx = neighborIdx;
                    break;
                }
            }
        }

        if (bestIdx < 0) break;
        if (bestIdx == startIdx) break;

        previous = current;
        currentIdx = bestIdx;
    }

    std::cout << "凹包计算完成：" << hull.size() << " 个边界顶点 (k=" << k << ")" << std::endl;
    return hull;
}

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

    // 计算包围盒
    double minX = points[0].x, maxX = points[0].x;
    double minY = points[0].y, maxY = points[0].y;
    for (const auto& p : points) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    // 10%边距
    double marginX = (maxX - minX) * 0.10;
    double marginY = (maxY - minY) * 0.10;
    if (marginX < 1.0) marginX = 1.0;
    if (marginY < 1.0) marginY = 1.0;
    minX -= marginX; maxX += marginX;
    minY -= marginY; maxY += marginY;

    double dataW = maxX - minX;
    double dataH = maxY - minY;

    // 图像尺寸，按数据宽高比适配
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

    // 世界坐标 -> 图像坐标
    auto worldToImg = [&](const cv::Point2d& wp) -> cv::Point {
        int ix = static_cast<int>((wp.x - minX) / dataW * imgW);
        int iy = static_cast<int>((maxY - wp.y) / dataH * imgH);
        return cv::Point(ix, iy);
    };

    cv::Mat image(imgH, imgW, CV_8UC3, cv::Scalar(0, 0, 0));

    // 输入点：蓝色小圆
    for (const auto& p : points) {
        cv::circle(image, worldToImg(p), 2, cv::Scalar(255, 0, 0), -1);
    }

    // 凹包边界：红色折线
    if (hull.size() >= 2) {
        std::vector<cv::Point> imgHull;
        imgHull.reserve(hull.size() + 1);
        for (const auto& p : hull) {
            imgHull.push_back(worldToImg(p));
        }
        imgHull.push_back(worldToImg(hull[0]));
        cv::polylines(image, imgHull, false, cv::Scalar(0, 0, 255), 2);
    }

    // 凹包顶点：绿色填充圆
    for (const auto& p : hull) {
        cv::circle(image, worldToImg(p), 4, cv::Scalar(0, 255, 0), -1);
    }

    cv::imwrite(outputPath, image);
    std::cout << "可视化图像已保存至: " << outputPath
              << " (" << imgW << "x" << imgH << ")" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

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
    std::string txtPath = "E:/ImageDataset/pozhuangcun/concave_hull.txt";
    SaveHullToFile(hull, txtPath, static_cast<int>(points.size()), k);

    // 5. 可视化
    std::string imgPath = "E:/ImageDataset/pozhuangcun/concave_hull.png";
    Visualize(points, hull, imgPath);

    return 0;
}
