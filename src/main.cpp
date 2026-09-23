#include <app/cli.hpp>

int wmain(const int argc, wchar_t* argv[]) {
    return ocr::cli::run(argc, argv);
}