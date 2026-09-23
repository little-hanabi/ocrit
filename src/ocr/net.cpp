#include <ocr/net.hpp>
#include <ocr/img.hpp>

#include <span>
#include <fstream>
#include <ranges>

namespace ocr::net {
    bool Model::load_dict() {
        const auto file = conf.dir / FILE_DICT;
        std::ifstream f(file, std::ios::binary);
        if (!f.is_open()) return false;
        const uint64_t len = fs::file_size(file);
        if (len != DICT_SIZE * 5) return false;
        dict.resize(DICT_SIZE);
        f.read(reinterpret_cast<char*>(dict.data()), static_cast<int64_t>(len));
        return true;
    }

    bool Model::load_onnx() {
        const std::array<fs::path, 2> file = {conf.dir / FILE_DET, conf.dir / FILE_REC};
        for (int i = 0; i < 2; ++i) {
            std::ifstream f(file[i], std::ios::binary);
            if (!f.is_open()) return false;
            std::vector<uint8_t> raw(fs::file_size(file[i]));
            f.read(reinterpret_cast<char*>(raw.data()), static_cast<int64_t>(raw.size()));
            sess[i].emplace(conf.env, raw.data(), raw.size(), conf.opt);
        }
        return true;
    }

    std::vector<float> Model::run(const cv::Mat& mat, const uint64_t idx) {
        const std::array<int64_t, 4> dim{1, 3, mat.size[2], mat.size[3]};
        const auto src = Ort::Value::CreateTensor<float>(conf.mem, reinterpret_cast<float*>(mat.data), mat.total(), dim.data(), dim.size());
        const auto dst = sess[idx]->Run({}, ONNX_NAME.data(), &src, 1, ONNX_NAME.data() + 1, 1);
        const auto spn = std::span(dst[0].GetTensorData<float>(), dst[0].GetTensorTypeAndShapeInfo().GetElementCount());
        return spn | std::ranges::to<std::vector<float>>();
    }

    std::string Model::ctc(const std::span<const float> tsr) const {
        const uint64_t len = tsr.size() / DICT_SIZE;
        std::string res;
        res.reserve(len << 2);

        uint64_t cnt = 0;
        float sum = 0.0f;
        for (uint64_t l = 0, pre = DICT_SIZE, off = 0; l < len; ++l, off += DICT_SIZE) {
            const auto spn = tsr.subspan(off, DICT_SIZE);
            const auto idx = std::ranges::max_element(spn) - spn.begin();
            if (idx != 0 && idx != pre) { // BLANK -> [0]
                ++cnt;
                sum += *(idx + spn.begin());
                res += dict[idx].data();
            }
            pre = idx;
        }

        return (cnt == 0 || sum / static_cast<float>(cnt) < conf.arg[4]) ? "" : res;
    }

    Model::Model(const fs::path& dir, const std::span<const float> arg) {
        if (!fs::is_directory(dir)) return;
        if (arg.size() == conf.arg.size()) std::ranges::copy(arg, conf.arg.begin());
        else if (!arg.empty()) return;
        conf.dir = dir;
        if (!load_dict()) return;
        if (!load_onnx()) return;
        good = true;
    }

    std::vector<std::string> Model::infer(const cv::Mat& img) {
        if (!good || img.empty()) return {};

        cv::Mat src, tmp;
        img::pad(img, src);
        img::norm(src, tmp, {0.485, 0.456, 0.406}, {0.229, 0.224, 0.225});
        auto roi = img::det(run(img::nchw(tmp), 0), tmp.size, conf.arg);

        std::vector<std::string> res;
        res.reserve(roi.size());
        for (const auto idx = img::sort(roi); const auto& i : idx) {
            for (auto _ : {0, 1}) {
                img::crop(src, tmp, roi[i]);
                img::norm(tmp, tmp, {0.5, 0.5, 0.5}, {0.5, 0.5, 0.5});
                const auto str = ctc(run(img::nchw(tmp), 1));
                if (!str.empty() || _) { res.push_back(str); break; }
                roi[i].angle += 180.0f;
            }
        }

        return res;
    }
}

// if (i == roi.size()) { res.emplace_back("\n"); continue; }
// std::vector<std::string> Model::ctc(const std::span<const float> tsr, const uint64_t num) const {
//     std::vector<std::string> res(num);
//     for (uint64_t b = 0, cnt = 0, len = tsr.size() / num / DICT_SIZE; b < num; ++b) {
//         res[b].reserve(len << 2);
//         float sum = 0.0f;
//         for (uint64_t l = 0, pre = DICT_SIZE; l < len; ++l) {
//             const auto spn = tsr.subspan((b * len + l) * DICT_SIZE, DICT_SIZE);
//             const auto idx = std::ranges::max_element(spn) - spn.begin();
//             if (idx != 0 && idx != pre) { // BLANK -> [0]
//                 ++cnt;
//                 sum += *(idx + spn.begin());
//                 res += dict[idx].data();
//             }
//             pre = idx;
//         }
//         if (cnt == 0 || sum / static_cast<float>(cnt) < conf.arg[4]) res[b].clear();
//     }
//     return res;
// }