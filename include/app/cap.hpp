#pragma once

#include <opencv2/core.hpp>

namespace cap {
    // 启动截图并写回所截取的图像
    void show(cv::Mat& img);

    // 启动热键监听并调用回调函数
    void wrap(const std::function<void(const cv::Mat&)>& call);
}
