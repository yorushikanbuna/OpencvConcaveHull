#pragma once

#include <opencv2/core.hpp>
#include <vector>

std::vector<cv::Point2d> ConcaveHull(const std::vector<cv::Point2d>& points, int k);
