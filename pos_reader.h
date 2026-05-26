#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

std::vector<cv::Point2d> ParsePosFile(const std::string& path);
