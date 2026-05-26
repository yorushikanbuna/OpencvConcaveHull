#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

void SaveHullToFile(const std::vector<cv::Point2d>& hull,
                    const std::string& path,
                    int inputCount, int k);

void Visualize(const std::vector<cv::Point2d>& points,
               const std::vector<cv::Point2d>& hull,
               const std::string& outputPath);
