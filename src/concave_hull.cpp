#include "concave_hull.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <set>
#include <opencv2/imgproc.hpp>

static double Dist(const cv::Point2d& a, const cv::Point2d& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
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

    // k参数映射到边长阈值因子
    // k=3 → factor≈0.15（紧密凹包），k=60 → factor≈60（趋向凸包）
    double factor = k * k / 60.0;
    double maxEdgeLen = factor * medianNN;
    if (maxEdgeLen < medianNN * 1.2) maxEdgeLen = medianNN * 1.2;

    // 3. 构建凸包多边形顶点列表
    std::vector<int> poly = hullIndices;

    // 4. 迭代细化：找到超过阈值的最长边，插入最近内部点分割
    std::set<int> usedPoints(poly.begin(), poly.end());
    bool changed = true;
    int maxIter = n * 2;
    int iterCount = 0;

    while (changed && iterCount < maxIter) {
        changed = false;
        iterCount++;

        double longestLen = 0;
        int splitPos = -1;
        int bestPt = -1;

        for (int ei = 0; ei < static_cast<int>(poly.size()); ++ei) {
            int i1 = poly[ei];
            int i2 = poly[(ei + 1) % poly.size()];
            double edgeLen = Dist(points[i1], points[i2]);

            if (edgeLen <= maxEdgeLen) continue;

            if (edgeLen > longestLen) {
                double localBestDist = std::numeric_limits<double>::max();
                int localBestPt = -1;

                cv::Point2d mid((points[i1].x + points[i2].x) / 2,
                                (points[i1].y + points[i2].y) / 2);

                for (int pi = 0; pi < n; ++pi) {
                    if (usedPoints.count(pi)) continue;
                    double d = Dist(points[pi], mid);
                    if (d < localBestDist && d < edgeLen * 0.7) {
                        localBestDist = d;
                        localBestPt = pi;
                    }
                }

                if (localBestPt >= 0) {
                    longestLen = edgeLen;
                    splitPos = ei;
                    bestPt = localBestPt;
                }
            }
        }

        if (splitPos >= 0 && bestPt >= 0) {
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
