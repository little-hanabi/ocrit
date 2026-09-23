#define NOMINMAX
#include <windows.h>

#include <ocr/ocr.hpp>
#include <app/cap.hpp>
#include <app/cli.hpp>

#include <ranges>
#include <format>
#include <fstream>

namespace ocr::cli {
namespace {
    void init() {
        PROCESS_POWER_THROTTLING_STATE s{
            .Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION,
            .ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED,
            .StateMask = 0
        };
        SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &s, sizeof(s));
    }

    void info(const std::string& src, const uint32_t type) {
        const auto s = fs::path(std::u8string{src.begin(), src.end()}).wstring();
        const auto n = fs::path(NAME | std::views::transform(::toupper) | std::ranges::to<std::string>()).wstring();
        MessageBoxW(nullptr, s.c_str(), n.c_str(), type);
    }

    void excp(const std::string& src) { info(src, MB_OK | MB_ICONERROR); }

    void help() {
        info(std::format("Version: {}\n\nUsage:\n"
        "{} <model_dir> [<arg_1> ... <arg_5>]\n"
        "{} <model_dir> <path_in> <path_out> [<arg_1> ... <arg_5>]",
        VERSION, NAME, NAME), MB_OK | MB_ICONINFORMATION);
    }

    std::string join(const std::vector<std::string>& src) {
        std::string out;
        uint64_t len = src.size();
        for (const auto& s : src) len += s.size();
        out.reserve(len);
        for (const auto& s : src) (out += s) += '\n';
        return out;
    }

    void clip(const std::string& src) {
        const auto s = fs::path(std::u8string{src.begin(), src.end()}).u16string();
        const auto h = GlobalAlloc(GMEM_MOVEABLE, (s.size() + 1) * sizeof(char16_t));
        if (h == nullptr) return;
        const auto p = static_cast<char16_t *>(GlobalLock(h));
        if (p == nullptr) { GlobalFree(h); return; }
        std::ranges::copy(std::span(s.c_str(), s.size() + 1), p);
        GlobalUnlock(h);
        if (!OpenClipboard(nullptr)) { GlobalFree(h); return; }
        EmptyClipboard();
        if (SetClipboardData(CF_UNICODETEXT, h) == nullptr) { GlobalFree(h); return; }
        CloseClipboard();
    }

    void save(const std::string& src, const fs::path& dst) {
        std::ofstream f(dst, std::ios::binary);
        if (!f.is_open()) return;
        f.write(src.data(), static_cast<int64_t>(src.size()));
    }
}
}

namespace ocr::cli {
    int run(const int argc, wchar_t* argv[]) {
        init();
        std::vector<fs::path> args;
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        const auto cfg = args | std::views::drop(argc == 7 || argc == 9 ? argc - 6 : args.size()) | std::views::take(5)
                              | std::views::transform([](const fs::path& p) { return std::stof(p.string()); })
                              | std::ranges::to<std::vector<float>>();

        try {
            if (argc == 2 || argc == 7) {
                net::Model model(args[0], cfg);
                if (!model) { excp(std::format("Exception:\n\nCan't load: {}", args[0].string())); return 1; }
                cap::wrap([&](const cv::Mat& img) {
                    if (img.empty()) return;
                    clip(join(model.infer(img)));
                });
            }
            else if (argc == 4 || argc == 9) {
                net::Model model(args[0], cfg);
                if (!model) { excp(std::format("Exception:\n\nCan't load: {}", args[0].string())); return 1; }
                if (fs::is_directory(args[1]) && fs::is_directory(args[2])) {
                    fs::create_directories(args[2]);
                    for (const auto& e : fs::directory_iterator(args[1])) {
                        if (!e.is_regular_file()) continue;
                        cv::Mat img;
                        img::load(e.path(), img);
                        if (img.empty()) continue;
                        save(join(model.infer(img)), args[2] / e.path().stem().replace_extension(".txt"));
                    }
                }
                else if (fs::is_regular_file(args[1]) && !fs::is_directory(args[2])) {
                    cv::Mat img;
                    img::load(args[1], img);
                    if (img.empty()) { excp(std::format("Exception:\n\nNot support: {}", args[1].string())); return 1; }
                    save(join(model.infer(img)), args[2]);
                }
                else help();
            }
            else help();
        } catch (const std::exception& e) {
            excp(std::format("Exception:\n\n{}", e.what()));
            return 1;
        }
        return 0;
    }
}