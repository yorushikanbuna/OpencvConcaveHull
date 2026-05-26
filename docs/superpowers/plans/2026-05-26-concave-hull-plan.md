# POS文件凹包生成器 实现计划

> **对执行代理的要求**：必须使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 按任务逐一实现。步骤使用 checkbox (`- [ ]`) 语法追踪。

**目标**：构建一个C++17程序，读取POS文件，提取XY坐标，使用k近邻边界追踪算法计算凹包，输出文本文件和可视化图像。

**架构**：单文件 `concave_hull.cpp`，内部分为 ParsePosFile、ConcaveHull、SaveHullToFile、Visualize 四个函数，由 main() 串联调用。CMake 构建系统，依赖仅 OpenCV。

**技术栈**：C++17、OpenCV（仅 cv::Point2d、cv::Mat 绘图和 imwrite）、C++ 标准库

---

## 文件结构

```
F:\ClaudeProj\ConcaveHullProj\
├── CMakeLists.txt          # 构建配置
├── concave_hull.cpp        # 全部源代码（单文件）
├── docs\
│   ├── specs\
│   │   └── 2026-05-26-concave-hull-design.md
│   └── superpowers\
│       └── plans\
│           └── 2026-05-26-concave-hull-plan.md
└── build\                  # 构建产物目录
```

各函数职责和接口：

| 函数 | 签名 | 职责 |
|------|------|------|
| ParsePosFile | `std::vector<cv::Point2d> ParsePosFile(const std::string& path)` | 解析POS文件 |
| ConcaveHull | `std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k)` | 凹包计算 |
| SaveHullToFile | `void SaveHullToFile(const std::vector<cv::Point2d>& hull, const std::string& path, int inputCount, int k)` | 文本输出 |
| Visualize | `void Visualize(const std::vector<cv::Point2d>& points, const std::vector<cv::Point2d>& hull, const std::string& outputPath)` | 可视化 |
| main | `int main(int argc, char* argv[])` | 流程串联 |

---

### 任务 1：创建项目骨架

**文件**：
- 创建：`F:\ClaudeProj\ConcaveHullProj\CMakeLists.txt`
- 创建：`F:\ClaudeProj\ConcaveHullProj\concave_hull.cpp`

- [ ] **步骤 1：编写 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(concave_hull concave_hull.cpp)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
```

- [ ] **步骤 2：编写带空 main 的 concave_hull.cpp**

```cpp
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Concave Hull - POS File Processor" << std::endl;
    return 0;
}
```

- [ ] **步骤 3：构建验证骨架可编译**

```bash
cd F:\ClaudeProj\ConcaveHullProj && mkdir -p build && cd build && cmake .. && cmake --build .
```

预期：编译成功，无警告，无错误。

- [ ] **步骤 4：运行验证**

```bash
./build/Debug/concave_hull.exe
```

预期输出：`Concave Hull - POS File Processor`

- [ ] **步骤 5：提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git init
git add CMakeLists.txt concave_hull.cpp .gitignore
git commit -m "feat: project skeleton with CMake and empty main"
```

---

### 任务 2：实现 ParsePosFile 函数

**文件**：
- 修改：`F:\ClaudeProj\ConcaveHullProj\concave_hull.cpp`

- [ ] **步骤 1：添加头文件和 ParsePosFile 函数实现**

替换 `concave_hull.cpp` 内容为：

```cpp
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
```

- [ ] **步骤 2：编译并运行验证**

```bash
cd F:\ClaudeProj\ConcaveHullProj\build && cmake --build . && ./Debug/concave_hull.exe
```

预期输出：
```
=== POS文件凹包生成器 ===
解析完成：XXXXX 个有效点（已去重）
点集大小: XXXXX
```

如果POS文件不可访问，程序应输出错误信息并退出。

- [ ] **步骤 3：提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git add concave_hull.cpp
git commit -m "feat: implement ParsePosFile with deduplication"
```

---

### 任务 3：实现 ConcaveHull 核心算法

**文件**：
- 修改：`F:\ClaudeProj\ConcaveHullProj\concave_hull.cpp`

- [ ] **步骤 1：在 main 函数之前添加辅助函数和 ConcaveHull**

在 `ParsePosFile` 函数之后、`main` 函数之前，插入以下代码：

```cpp
// 计算从向量v1到v2的有符号角度 [-π, π]
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

    // 点数不足3个，直接返回
    if (n < 3) {
        return points;
    }

    // 确保k不超过n-1
    k = std::min(k, n - 1);
    if (k < 1) k = 1;

    // 1. 为每个点构建k近邻索引
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

    // 3. 边界追踪
    std::vector<bool> visited(n, false);
    std::vector<cv::Point2d> hull;

    int currentIdx = startIdx;
    // 虚构的前一个点：在起点正左方
    cv::Point2d previous = points[startIdx];
    previous.x -= 1.0;

    const int maxIter = n * 2; // 防止死循环
    for (int iter = 0; iter < maxIter; ++iter) {
        hull.push_back(points[currentIdx]);

        const auto& current = points[currentIdx];
        cv::Point2d dirPrev(current.x - previous.x, current.y - previous.y);

        // 在k近邻中找最顺时针转的点
        double bestAngle = std::numeric_limits<double>::max();
        int bestIdx = -1;

        for (int neighborIdx : knnIndex[currentIdx]) {
            if (neighborIdx == currentIdx) continue;

            const auto& candidate = points[neighborIdx];
            cv::Point2d dirCand(candidate.x - current.x, candidate.y - current.y);

            double angle = SignedAngle(dirPrev, dirCand);

            if (angle < bestAngle) {
                bestAngle = angle;
                bestIdx = neighborIdx;
            }
        }

        if (bestIdx < 0) {
            // 找不到下一个点，强制闭合
            break;
        }

        // 回到起点，闭合完成
        if (bestIdx == startIdx) {
            break;
        }

        previous = current;
        currentIdx = bestIdx;
    }

    return hull;
}
```

- [ ] **步骤 2：更新 main 函数以调用 ConcaveHull**

将 main 函数替换为：

```cpp
int main(int argc, char* argv[]) {
    std::cout << "=== POS文件凹包生成器 ===" << std::endl;

    // 1. 解析POS文件
    auto points = ParsePosFile(POS_PATH);
    std::cout << "点集大小: " << points.size() << std::endl;

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
    std::cout << "凹包顶点数: " << hull.size() << std::endl;

    return 0;
}
```

- [ ] **步骤 3：编译并验证**

```bash
cd F:\ClaudeProj\ConcaveHullProj\build && cmake --build .
```

预期：编译成功。然后运行：

```bash
./Debug/concave_hull.exe
```

预期输出包含凹包顶点数。

- [ ] **步骤 4：提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git add concave_hull.cpp
git commit -m "feat: implement ConcaveHull with k-NN boundary tracing"
```

---

### 任务 4：实现 SaveHullToFile 文本输出

**文件**：
- 修改：`F:\ClaudeProj\ConcaveHullProj\concave_hull.cpp`

- [ ] **步骤 1：在 ConcaveHull 之后添加 SaveHullToFile**

```cpp
void SaveHullToFile(const std::vector<cv::Point2d>& hull,
                    const std::string& path,
                    int inputCount, int k) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "错误：无法创建输出文件 " << path << std::endl;
        exit(1);
    }

    // 写入注释头
    file << "# 凹包结果 - 输入" << inputCount
         << "个点 -> 输出" << hull.size()
         << "个边界点 (k=" << k << ")" << std::endl;

    // 写入凹包顶点
    file.precision(12);
    for (const auto& p : hull) {
        file << std::fixed << p.x << " " << p.y << std::endl;
    }

    std::cout << "凹包文本已保存至: " << path << std::endl;
}
```

- [ ] **步骤 2：更新 main 函数，在 return 前添加调用**

在 `main` 函数的 `return 0;` 之前添加：

```cpp
    // 4. 保存凹包文本
    std::string txtPath = std::string(POS_PATH).substr(0, POS_PATH.find_last_of("\\/") + 1) + "concave_hull.txt";
    SaveHullToFile(hull, txtPath, static_cast<int>(points.size()), k);
```

- [ ] **步骤 3：编译并验证**

```bash
cd F:\ClaudeProj\ConcaveHullProj\build && cmake --build .
```

编译通过后运行，检查输出目录下是否生成了 `concave_hull.txt` 文件。

- [ ] **步骤 4：提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git add concave_hull.cpp
git commit -m "feat: implement SaveHullToFile text output"
```

---

### 任务 5：实现 Visualize 可视化

**文件**：
- 修改：`F:\ClaudeProj\ConcaveHullProj\concave_hull.cpp`

- [ ] **步骤 1：在 SaveHullToFile 之后添加 Visualize**

```cpp
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

    // 世界坐标 → 图像坐标的变换
    auto worldToImg = [&](const cv::Point2d& wp) -> cv::Point {
        int ix = static_cast<int>((wp.x - minX) / dataW * imgW);
        int iy = static_cast<int>((maxY - wp.y) / dataH * imgH); // Y翻转
        return cv::Point(ix, iy);
    };

    // 创建图像
    cv::Mat image(imgH, imgW, CV_8UC3, cv::Scalar(0, 0, 0));

    // 1. 绘制输入点：蓝色小圆
    for (const auto& p : points) {
        cv::circle(image, worldToImg(p), 2, cv::Scalar(255, 0, 0), -1);
    }

    // 2. 绘制凹包边界：红色折线
    if (hull.size() >= 2) {
        std::vector<cv::Point> imgHull;
        imgHull.reserve(hull.size());
        for (const auto& p : hull) {
            imgHull.push_back(worldToImg(p));
        }
        // 闭合到第一个点
        imgHull.push_back(worldToImg(hull[0]));
        cv::polylines(image, imgHull, false, cv::Scalar(0, 0, 255), 2);
    }

    // 3. 绘制凹包顶点：绿色填充圆
    for (const auto& p : hull) {
        cv::circle(image, worldToImg(p), 4, cv::Scalar(0, 255, 0), -1);
    }

    cv::imwrite(outputPath, image);
    std::cout << "可视化图像已保存至: " << outputPath
              << " (" << imgW << "x" << imgH << ")" << std::endl;
}
```

- [ ] **步骤 2：更新 main 函数，在 return 前添加调用**

在 `main` 函数的 `return 0;` 之前添加：

```cpp
    // 5. 可视化
    std::string imgPath = std::string(POS_PATH).substr(0, POS_PATH.find_last_of("\\/") + 1) + "concave_hull.png";
    Visualize(points, hull, imgPath);
```

- [ ] **步骤 3：编译并验证**

```bash
cd F:\ClaudeProj\ConcaveHullProj\build && cmake --build .
```

编译通过后运行，检查是否生成 `concave_hull.png`。

- [ ] **步骤 4：提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git add concave_hull.cpp
git commit -m "feat: implement Visualize with OpenCV rendering"
```

---

### 任务 6：最终集成验证

**文件**：无修改，仅验证

- [ ] **步骤 1：完整编译**

```bash
cd F:\ClaudeProj\ConcaveHullProj\build && cmake --build . --config Release
```

- [ ] **步骤 2：运行完整流程**

```bash
./Release/concave_hull.exe
```

验证：
- 解析点集数量正确
- 凹包顶点数合理（应远小于12530）
- `concave_hull.txt` 生成，格式正确
- `concave_hull.png` 生成，可正常打开查看

- [ ] **步骤 3：测试命令行k值覆盖**

```bash
./Release/concave_hull.exe 20
```

验证输出中显示 "使用命令行参数 k=20"，且凹包顶点数相比默认k=50有所变化。

- [ ] **步骤 4：最终提交**

```bash
cd F:\ClaudeProj\ConcaveHullProj
git add -A
git commit -m "feat: complete concave hull POS processor"
```

---

## 最终文件完整内容参考

完整的 `concave_hull.cpp` 应包含以下结构（从顶到底）：

```
#include 头文件区域
   <iostream>, <fstream>, <sstream>, <string>, <vector>, <cmath>, <algorithm>
   <opencv2/core.hpp>, <opencv2/imgproc.hpp>, <opencv2/imgcodecs.hpp>, <opencv2/highgui.hpp>

常量定义
   POS_PATH = R"(E:\ImageDataset\pozhuangcun\pos.txt)"

函数实现（按调用顺序）
   ParsePosFile(path)         → vector<cv::Point2d>
   SignedAngle(v1, v2)        → double        [static, ConcaveHull内部使用]
   DistSq(a, b)               → double        [static, ConcaveHull内部使用]
   ConcaveHull(points, k)     → vector<cv::Point2d>
   SaveHullToFile(hull, ...)  → void
   Visualize(points, hull, ...) → void
   main(argc, argv)           → int
```
