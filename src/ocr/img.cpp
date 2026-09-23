#include <ocr/img.hpp>

#include <fstream>
#include <ranges>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace ocr::img {
    void save(const cv::Mat& buff, const fs::path& file) {
        std::vector<uint8_t> raw;
        if (!cv::imencode(file.extension().string(), buff, raw, {cv::IMWRITE_PNG_COMPRESSION, 4})) return;
        std::ofstream f(file, std::ios::binary);
        if (!f.is_open()) return;
        f.write(reinterpret_cast<const char*>(raw.data()), static_cast<int64_t>(raw.size()));
    }

    void load(const fs::path& file, cv::Mat& buff) {
        std::ifstream f(file, std::ios::binary);
        if (!f.is_open()) return;
        std::vector<uint8_t> raw(fs::file_size(file));
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<int64_t>(raw.size()));
        buff = cv::imdecode(raw, cv::IMREAD_COLOR_BGR); // 固定三通道图像
    }

    void crop(const cv::Mat& src, cv::Mat& dst, const cv::RotatedRect& r) {
        CV_DbgAssert(!src.empty() && src.type() == CV_8UC3 && src.dims == 2);
        cv::Mat tmp, M = cv::getRotationMatrix2D(r.center, r.angle, 1.0);
        M.at<double>(0, 2) += r.size.width  / 2.0 - r.center.x;
        M.at<double>(1, 2) += r.size.height / 2.0 - r.center.y;
        cv::warpAffine(src, tmp, M, r.size, cv::INTER_LINEAR, cv::BORDER_CONSTANT, {127, 127, 127});
        cv::resize(tmp, dst, cv::Size(cvRound(tmp.cols * 48.0 / tmp.rows), 48), 0, 0, cv::INTER_AREA);
    }

    void norm(const cv::Mat& src, cv::Mat& dst, const cv::Scalar& mean, const cv::Scalar& std) {
        CV_DbgAssert(!src.empty() && src.type() == CV_8UC3 && src.dims == 2);
        cv::Mat img;
        src.convertTo(img, CV_32FC3, 1.0 / 255.0);
        cv::subtract(img, mean, img);
        cv::divide(img, std, dst);
    }

    cv::Mat nchw(const cv::Mat& src) {
        CV_DbgAssert(!src.empty() && src.type() == CV_32FC3 && src.dims == 2);
        const int s[] = {1, 3, src.rows, src.cols};
        cv::Mat dst(4, s, CV_32F);
        cv::Mat chs[] = {
            cv::Mat(src.rows, src.cols, CV_32F, dst.ptr(0, 0)),
            cv::Mat(src.rows, src.cols, CV_32F, dst.ptr(0, 1)),
            cv::Mat(src.rows, src.cols, CV_32F, dst.ptr(0, 2))
        };
        cv::split(src, chs);
        return dst;
    }

    std::vector<uint64_t> sort(const std::span<const cv::RotatedRect> roi) {
        if (roi.empty()) return {};
        const auto box = roi | std::views::transform(&cv::RotatedRect::boundingRect2f) | std::ranges::to<std::vector>();
        std::vector<uint64_t> res;
        res.reserve(box.size());

        constexpr auto gap = [](const std::vector<std::pair<float, float>>& iv) {
            auto s = iv;
            std::ranges::sort(s);
            std::pair<float, float> out{0.0f, 0.0f};
            for (float end = s.front().second; const auto& [lo, hi] : s | std::views::drop(1)) {
                out = std::max(out, std::pair{lo - end, (lo + end) / 2.0f});
                end = std::max(end, hi);
            }
            return out;
        };

        constexpr auto lh = [](const cv::Rect2f& r, const uint64_t axis) {
            return axis == 0 ? std::pair{r.x, r.x + r.width} : std::pair{r.y, r.y + r.height};
        };

        const auto recurse = [&](this const auto& self, const std::span<const uint64_t> sub) -> void {
            const auto iv = [&](const uint64_t axis) {
                return sub | std::views::transform([&](const uint64_t i) { return lh(box[i], axis); }) | std::ranges::to<std::vector>();
            };
            const auto gx = gap(iv(0));
            const auto gy = gap(iv(1));
            const auto& [len, cut] = gx.first < gy.first ? gy : gx;
            if (len <= 0.0f) { res.append_range(sub); return; }
            const auto side = [&](const bool left) {
                return sub | std::views::filter([&](const uint64_t i) {
                    const auto [lo, hi] = lh(box[i], gx.first < gy.first);
                    return left ? hi <= cut : lo >= cut;
                }) | std::ranges::to<std::vector>();
            };
            self(side(true));
            self(side(false));
        };

        recurse(std::views::iota(0ULL, box.size()) | std::ranges::to<std::vector>());
        return res;
    }

    void pad(const cv::Mat &src, cv::Mat &dst) {
        CV_DbgAssert(!src.empty() && src.type() == CV_8UC3 && src.dims == 2);
        const int h = (-src.rows) & 31;
        const int w = (-src.cols) & 31;
        cv::copyMakeBorder(src, dst, 0, h, 0, w, cv::BORDER_CONSTANT, {255, 255, 255});
    }

    void fix(cv::RotatedRect& r, const float s) {
        CV_DbgAssert(!r.size.empty());
        if (r.size.width < r.size.height) {
            std::swap(r.size.width, r.size.height);
            r.angle = std::remainder(r.angle + 90.f, 180.f);
        }
        const float d = r.size.area() * s / (2.0f * (r.size.width + r.size.height));
        r.size.width  += 2.0f * d;
        r.size.height += 2.0f * d;
    }

    std::vector<cv::RotatedRect> det(const std::span<const float> tsr, const cv::MatSize& dim, const std::span<const float> arg) {
        std::vector<cv::RotatedRect> res;

        const cv::Mat prob(dim[0], dim[1], CV_32FC1, const_cast<float*>(tsr.data()));
        cv::Mat mask = cv::Mat::zeros(dim[0], dim[1], CV_8UC1);

        std::vector<std::vector<cv::Point>> ctr;
        cv::findContours(prob > arg[0], ctr, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        for (const auto& c : ctr) {
            if (c.size() < 4) continue;

            cv::RotatedRect r = cv::minAreaRect(c); // OpenCV >= 4.13.0
            if (std::min(r.size.width, r.size.height) < arg[1]) continue;
            cv::Rect2i roi = r.boundingRect() & cv::Rect2i(0, 0, dim[1], dim[0]);
            if (roi.empty()) continue;

            mask(roi).setTo(0);
            cv::Point2f v[4]; r.points(v);
            cv::fillConvexPoly(mask, std::vector<cv::Point>{v[0], v[1], v[2], v[3]}, 255);
            if (cv::mean(prob(roi), mask(roi))[0] < arg[2]) continue;

            fix(r, arg[3]);
            res.push_back(r);
        }

        return res;
    }
}
