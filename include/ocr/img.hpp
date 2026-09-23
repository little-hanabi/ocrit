#pragma once

#include <filesystem>
#include <span>

#include <opencv2/core.hpp>

namespace fs = std::filesystem;

namespace ocr::img {
    // 写图像
    void save(const cv::Mat& buff, const fs::path& file);

    // 读图像
    void load(const fs::path& file, cv::Mat& buff);

    // 裁剪矫正
    void crop(const cv::Mat& src, cv::Mat& dst, const cv::RotatedRect& r);

    // 标准化
    void norm(const cv::Mat& src, cv::Mat& dst, const cv::Scalar& mean, const cv::Scalar& std);

    // 布局转换
    [[nodiscard]] cv::Mat nchw(const cv::Mat& src);

    // 阅读排序
    [[nodiscard]] std::vector<uint64_t> sort(std::span<const cv::RotatedRect> roi);

    // 填充以适配模型输入
    void pad(const cv::Mat& src, cv::Mat& dst);

    // 分析文字区域
    [[nodiscard]] std::vector<cv::RotatedRect> det(std::span<const float> tsr, const cv::MatSize& dim, std::span<const float> arg);
}