# POS文件凹包生成器 实现计划

> **对执行代理的要求**：使用 superpowers:subagent-driven-development 或 superpowers:executing-plans 按任务逐一实现。

**目标**：构建一个模块化的C++17程序，读取POS文件并提取XY坐标，使用凸包递归细化算法计算凹包，输出文本文件和可视化图像，包含自动化单元测试。

**架构**：多文件模块化结构（pos_reader、concave_hull、output、tests、main），CMake + vcpkg清单模式构建，依赖OpenCV4。

**技术栈**：C++17、OpenCV 4.7（cv::convexHull、cv::Mat绘图、cv::imwrite）、vcpkg

---

## 文件结构

```
ConcaveHullProj/
├── main.cpp              # 程序入口，流程编排
├── pos_reader.h/cpp      # POS文件解析（ParsePosFile）
├── concave_hull.h/cpp    # 凹包算法（ConcaveHull + Dist）
├── output.h/cpp          # 输出模块（SaveHullToFile + Visualize）
├── tests.h/cpp           # 自测模块（RunTests）
├── CMakeLists.txt        # 构建配置（多文件add_executable）
├── vcpkg.json            # vcpkg依赖清单
├── .gitignore
├── README.md
├── build/
└── docs/
```

各模块职责和接口：

| 模块 | 文件 | 接口 |
|------|------|------|
| pos_reader | pos_reader.h/cpp | `std::vector<cv::Point2d> ParsePosFile(const std::string& path)` |
| concave_hull | concave_hull.h/cpp | `std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k)` |
| output | output.h/cpp | `void SaveHullToFile(...)` / `void Visualize(...)` |
| tests | tests.h/cpp | `void RunTests()` |
| main | main.cpp | `int main(int argc, char* argv[])` |

---

### 任务 1：创建项目骨架

**文件**：
- 创建：`CMakeLists.txt`、`main.cpp`、`vcpkg.json`、`.gitignore`

**CMakeLists.txt**：
```cmake
cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(OpenCV REQUIRED)

add_executable(concave_hull
    main.cpp
    pos_reader.cpp
    concave_hull.cpp
    output.cpp
    tests.cpp
)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
```

**vcpkg.json**（与Virtuoso3dAt项目相同的baseline）：
```json
{
    "builtin-baseline": "5128d5b9e3a7737b220f25e6ce048c98db004001",
    "dependencies": [
        {
            "features": ["default-features"],
            "name": "opencv4",
            "version>=": "4.7.0"
        }
    ],
    "name": "concave-hull-proj",
    "overrides": [
        {
            "name": "opencv4",
            "port-version": 6,
            "version-string": "4.7.0"
        }
    ],
    "version-semver": "1.0.0"
}
```

**构建命令**：
```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=D:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

---

### 任务 2：实现 pos_reader 模块

**文件**：创建 `pos_reader.h`、`pos_reader.cpp`

**pos_reader.h**：
```cpp
#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>
std::vector<cv::Point2d> ParsePosFile(const std::string& path);
```

**pos_reader.cpp**：
- 实现 `ParsePosFile()`：逐行读取、空格分割、解析X/Y/Z、丢弃Z
- 错误处理：文件不存在→exit、列数不足→warn+skip、空文件→exit
- 去重：排序后 `std::unique`，浮点比较使用 `a.x==b.x && a.y==b.y`
- 输出解析统计到 stdout

**验证**：编译通过，使用实际POS文件运行验证点数和去重效果。

---

### 任务 3：实现 concave_hull 核心算法

**文件**：创建 `concave_hull.h`、`concave_hull.cpp`

**算法**：凸包递归细化（Convex Hull Edge Refinement）

1. 点数 < 3 → 直接返回
2. 转换为 `cv::Point2f`，调用 `cv::convexHull()` 获取凸包顶点索引
3. 对每个点计算最近邻距离（O(N²)暴力），取中位值 `medianNN`
4. 边长阈值：`maxEdgeLen = (k² / 60) × medianNN`，下限 `1.2 × medianNN`
5. `usedPoints` 集合初始为凸包顶点
6. 循环细化：
   - 遍历多边形每条边 `(poly[i], poly[i+1])`
   - 找长度超过阈值的最长边
   - 在该边中点附近找最近未使用点（条件：距离 < 边长×0.7）
   - 将找到的点插入该边之间，加入 `usedPoints`
   - 重复直到无满足条件的边或达到最大迭代次数 `2N`
7. 按 `poly` 索引顺序构建 `vector<cv::Point2d>` 结果

**k参数映射**：
- k=3 → maxEdgeLen ≈ 1.2×medianNN（紧密凹包）
- k=60 → maxEdgeLen ≈ 60×medianNN（趋近凸包）

**验证**：编译通过，运行自测和实际数据验证。

---

### 任务 4：实现 output 模块

**文件**：创建 `output.h`、`output.cpp`

**SaveHullToFile**：写入注释头（含输入点数、输出顶点数、k值），逐行写入 `x y`（12位精度）

**Visualize**：
- 计算点集包围盒 + 10%边距
- 按数据宽高比适配1920×1080画布
- 世界坐标→图像坐标（Y翻转）
- 黑色背景 → 蓝色输入点(r=2) → 红色凹包折线(w=2, 闭合) → 绿色边界顶点(r=4)
- `cv::imwrite()` 输出PNG

**验证**：编译通过，运行后检查生成的txt和png文件。

---

### 任务 5：实现 tests 自测模块 + main 集成

**文件**：创建 `tests.h`、`tests.cpp`，修改 `main.cpp`

**tests.cpp** — 5个测试用例：
1. 矩形+内部点：验证内部点(5,5)被排除，顶点数≥4
2. L形凹集：验证凹包顶点≥5
3a/3b：单点/两点退化
4. 圆形+凹入：验证凹包首尾闭合

**main.cpp**：
```cpp
int main(int argc, char* argv[]) {
    RunTests();
    auto points = ParsePosFile(POS_PATH);
    int k = std::clamp(int(sqrt(points.size())), 3, 60);
    if (argc >= 2) k = std::max(1, std::atoi(argv[1]));
    auto hull = ConcaveHull(points, k);
    SaveHullToFile(hull, txtPath, points.size(), k);
    Visualize(points, hull, imgPath);
    return 0;
}
```

**验证**：全部5项自测通过，使用不同k值运行验证输出合理性。

---

## 验证清单

- [ ] CMake配置 + vcpkg清单模式构建成功
- [ ] Debug和Release编译均无错误
- [ ] 5项自测全部通过
- [ ] k=auto 产出合理数量的边界顶点
- [ ] k=5 紧密凹包，k=35 接近凸包
- [ ] concave_hull.txt 格式正确、顶点有序
- [ ] concave_hull.png 可正常打开、可视化效果良好
- [ ] 所有源文件使用 `/utf-8` 编译（MSVC中文支持）
