#pragma once

#include <span>
#include <string>
#include <optional>
#include <filesystem>

#include <opencv2/core.hpp>
#include <onnxruntime_cxx_api.h>

namespace fs = std::filesystem;

namespace ocr::net {
    constexpr std::string FILE_DET  = "det.onnx";
    constexpr std::string FILE_REC  = "rec.onnx";
    constexpr std::string FILE_DICT = "dict.bin";

    constexpr uint64_t DICT_SIZE = 18710; // small and medium

    constexpr std::array<const char*, 2> ONNX_NAME = {"x", "fetch_name_0"};
}

namespace ocr::net {
    class Model {
    public:
        explicit Model(const fs::path& dir, std::span<const float> arg = {});
        explicit operator bool() const { return good; }
        [[nodiscard]] std::vector<std::string> infer(const cv::Mat& img);
    private:
        struct Config {
            std::array<float, 5> arg;
            fs::path dir;
            Ort::Env env;
            Ort::SessionOptions opt;
            Ort::MemoryInfo mem;
        };

        bool good = false;
        Config conf{
            .arg = {0.3f, 3.0f, 0.6f, 1.5f, 0.7f},
            .dir = "./",
            .env = {},
            .opt = [] {
                Ort::SessionOptions opt;
                opt.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
                opt.DisableMemPattern();
                opt.DisableCpuMemArena();
                return opt;
            }(),
            .mem = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU)
        };
        std::vector<std::array<char, 5>> dict;
        std::array<std::optional<Ort::Session>, 2> sess;

        [[nodiscard]] bool load_dict();
        [[nodiscard]] bool load_onnx();
        [[nodiscard]] std::vector<float> run(const cv::Mat& mat, uint64_t idx);
        [[nodiscard]] std::string ctc(std::span<const float> tsr) const;
    };
}