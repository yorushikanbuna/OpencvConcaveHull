# POS文件凹包生成器 - 设计文档

**日期**：2026-05-26
**状态**：定稿

## 概述

读取包含影像位置（X, Y, Z）的POS文件，提取XY坐标，计算点集的凹包（concave hull），并输出边界顶点文本文件和可视化图像。

## 需求汇总

| 编号 | 需求 | 详情 |
|------|------|------|
| R1 | 输入格式 | POS文件：`影像名 X Y Z`，空格分隔，浮点坐标 |
| R2 | 坐标类型 | `cv::Point2d`（双精度浮点） |
| R3 | 凹包算法 | 自实现的k近邻边界追踪算法 |
| R4 | k参数 | 默认自动确定（`clamp(sqrt(N), 3, 60)`），可通过命令行参数覆盖 |
| R5 | 输出一 | 文本文件：每行一个 `x y`，首行为注释 |
| R6 | 输出二 | 可视化图像：原始点 + 凹包边界 |
| R7 | 文件路径 | 源代码中硬编码 |
| R8 | 依赖 | 仅OpenCV + C++标准库 |
| R9 | 项目结构 | 单个 `.cpp` 文件，独立CMake项目 |

## 架构

单文件结构，内部按函数分段组织：

```
main()
  ├── ParsePosFile(路径)           → std::vector<cv::Point2d>
  ├── ConcaveHull(点集, k)        → std::vector<cv::Point2d>
  ├── SaveHullToFile(凹包, 路径)   → void
  └── Visualize(点集, 凹包, 路径)   → void
```

## 数据流

```
E:\...\pos.txt ──ParsePosFile──▶ vector<cv::Point2d> ──ConcaveHull──▶ vector<cv::Point2d>
                                        │                                    │
                                        │                            ┌───────┴───────┐
                                        │                            ▼               ▼
                                        │                     concave_hull.txt  concave_hull.png
                                        │
                                        └── （点集同时传给Visualize用于绘制）
```

## 模块设计

### 1. ParsePosFile — POS文件解析

```
函数签名：std::vector<cv::Point2d> ParsePosFile(const std::string& path)
```

**处理流程**：
- 打开文件，逐行读取
- 按空白字符（空格/制表符）分割每行
- 列顺序：`[0]=影像名, [1]=X, [2]=Y, [3]=Z`（丢弃Z）
- 使用 `std::stod()` 解析X/Y
- 以 `cv::Point2d(x, y)` 存入结果向量

**错误处理**：

| 情况 | 处理方式 |
|------|----------|
| 文件不存在 | 打印错误信息，`exit(1)` |
| 某行列数不足3列 | 打印警告（含行号），跳过该行 |
| 文件为空或无有效数据行 | 打印错误信息，`exit(1)` |

**边界情况**：
- 以 `#` 开头的行视为注释，跳过
- 空行跳过
- 重复XY坐标去重

### 2. ConcaveHull — 凹包计算

```
函数签名：std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k)
```

**算法**：k近邻边界追踪

1. **守护检查**：若 `points.size() < 3`，直接返回所有点
2. **去重**：移除完全重复的坐标点
3. **构建k近邻**：对每个点，通过暴力计算欧氏距离找出其k个最近邻点
4. **寻找起点**：取最右侧的点（X最大，X相同则取Y最大）
5. **初始化**：`previous` = 起点左侧的虚拟点 `(start.x - 1, start.y)`
6. **边界追踪**：
   - 从 `current` 的k个近邻中，选择使 `∠(current→previous, current→candidate)` 顺时针转角最大的候选点
   - 该策略等价于"最右转"——沿边界顺时针行走
   - 若选中的候选点 == 起点，闭合完成
   - 否则：`previous = current`，`current = 选中点`，继续循环
7. 返回凹包顶点序列（逆时针顺序，闭合多边形——最后一个顶点与第一个顶点相连）

**复杂度**：
- k近邻构建：O(N²) 次距离计算
- 边界追踪：O(H × k)，H为凹包顶点数
- N=12500、k=50时：约 1.5×10⁸ 次距离计算，单线程预估 2-5 秒

**k参数说明**：
- 默认值：`k = std::clamp(static_cast<int>(std::sqrt(N)), 3, 60)`
- N=12500 时，k ≈ 50
- k越小 → 凹包越紧贴点集（更凹），k越大 → 越接近凸包（更平滑）

**边界情况**：
- 所有点共线：返回两端点
- 点数不足3个：返回所有输入点

### 3. SaveHullToFile — 文本输出

```
函数签名：void SaveHullToFile(const std::vector<cv::Point2d>& hull,
                              const std::string& path,
                              int inputCount, int k)
```

- 首行写入包含元数据的注释
- 之后每行写入一对 `x y`，保留完整 double 精度
- 默认输出路径：与输入POS文件同目录，文件名 `concave_hull.txt`

**输出格式**：
```
# 凹包结果 - 输入12530个点 -> 输出247个边界点 (k=50)
528862.590974 3802421.315441
528863.287560 3802422.171764
...
```

### 4. Visualize — 可视化

```
函数签名：void Visualize(const std::vector<cv::Point2d>& points,
                          const std::vector<cv::Point2d>& hull,
                          const std::string& outputPath)
```

- 根据点集计算包围盒，留10%边距
- 通过仿射变换将世界坐标映射到图像坐标
- 图像尺寸：1920 × 1080（按数据宽高比等比缩放）
- 渲染层次（从背景到前景）：
  1. 黑色背景
  2. 输入点集：蓝色圆点，半径 2px
  3. 凹包边界：红色折线，线宽 2px
  4. 凹包顶点：绿色填充圆，半径 4px
- 输出格式：PNG，默认文件名 `concave_hull.png`

### 5. Main — 主函数

```
函数签名：int main(int argc, char* argv[])
```

**执行流程**：
1. 硬编码POS文件路径
2. `ParsePosFile()` → 点集
3. 确定k值（自动确定；可接受 `argv[1]` 作为整数k值覆盖）
4. `ConcaveHull(点集, k)` → 凹包
5. `SaveHullToFile(凹包, ...)`
6. `Visualize(点集, 凹包, ...)`
7. 打印汇总统计信息

## 项目结构

```
F:\ClaudeProj\ConcaveHullProj\
├── CMakeLists.txt
├── concave_hull.cpp
├── docs\
│   └── specs\
│       └── 2026-05-26-concave-hull-design.md
└── build\
```

## CMake 配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(ConcaveHullProj LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED)

add_executable(concave_hull concave_hull.cpp)
target_link_libraries(concave_hull PRIVATE ${OpenCV_LIBS})
```

## 测试策略

人工验证：
1. 使用已知POS文件（E:\ImageDataset\pozhuangcun\pos.txt，12530个点）运行
2. 验证 `concave_hull.txt` 包含有效的有序边界顶点
3. 验证 `concave_hull.png` 可视化显示凹包包围所有点
4. 验证凹包闭合（第一个顶点 ≈ 最后一个顶点，在容差范围内）
5. 边界情况：小型合成数据集（3、5、10个点）

## 遗留问题

无。所有需求已澄清。
