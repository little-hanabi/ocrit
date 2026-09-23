#define NOMINMAX
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#include <windows.h>

#include <app/cap.hpp>

namespace cap {
    namespace {
        constexpr uint32_t px{}; // 黑像素点
        constexpr BLENDFUNCTION bf{.BlendOp = AC_SRC_OVER, .SourceConstantAlpha = 127}; // 混合配置

        struct Context {
            int vx = 0, vy = 0, vw = 0, vh = 0; // 虚拟屏幕坐标和宽高
            void*   p_ori = nullptr; // 原图数据指针
            HBITMAP b_ori = nullptr; // 原图位图句柄
            HBITMAP b_blk = nullptr; // 暗图位图句柄
            HDC     h_src = nullptr; // 设备上下文句柄
            cv::Point2i p_beg; // 鼠标起点坐标
            cv::Point2i p_cur; // 鼠标当前坐标
            cv::Rect2i  r_pre; // 上一帧矩形区域
            bool drag = false; // 是否正在拖拽
            bool done = false; // 是否完成截图
        };

        // 根据新旧两个矩形区域更新图像
        void draw(const cv::Rect2i& o, const cv::Rect2i& n, HBITMAP b_ori, HBITMAP b_blk, HDC h_src, HDC h_dst) {
            auto copy = [&](HBITMAP b, const cv::Rect2i& r) { // 将b的r区域绘制到h_dst
                if (r.empty()) return;
                SelectObject(h_src, b);
                BitBlt(h_dst, r.x, r.y, r.width, r.height, h_src, r.x, r.y, SRCCOPY);
            };

            cv::Rect2i ix = o & n;
            if (ix.empty()) { // 交集为空则全部更新
                copy(b_blk, o);
                copy(b_ori, n);
                return;
            }

            auto band = [&](HBITMAP b, const cv::Rect2i& r) { // 判断更新区域
                if (r.x < ix.x)           copy(b, {r.x, r.y, ix.x - r.x, r.height});
                if (r.br().x > ix.br().x) copy(b, {ix.br().x, r.y, r.br().x - ix.br().x, r.height});
                if (r.y < ix.y)           copy(b, {ix.x, r.y, ix.width, ix.y - r.y});
                if (r.br().y > ix.br().y) copy(b, {ix.x, ix.br().y, ix.width, r.br().y - ix.br().y});
            };

            band(b_blk, o); // 旧区域变暗图
            band(b_ori, n); // 新区域变原图
        }

        // 窗口消息处理回调
        LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
            if (msg == WM_CREATE) { // 保存Context指针
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return 0;
            }

            auto* ctx = reinterpret_cast<Context*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            if (ctx != nullptr) {
                if (msg == WM_PAINT) {
                    const auto r_cur = ctx->drag ? cv::Rect2i(ctx->p_beg, ctx->p_cur) : cv::Rect2i{};
                    PAINTSTRUCT ps{};
                    HDC h_dst = BeginPaint(hwnd, &ps);
                    draw(ctx->r_pre, r_cur, ctx->b_ori, ctx->b_blk, ctx->h_src, h_dst);
                    ctx->r_pre = r_cur;
                    EndPaint(hwnd, &ps);
                    return 0;
                }

                if (msg == WM_LBUTTONDOWN) {
                    ctx->p_beg = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    ctx->p_cur = ctx->p_beg;
                    ctx->drag = true;
                    SetCapture(hwnd); // 捕获鼠标，防止因移动到系统特权窗口丢失WM_LBUTTONUP
                    return 0;
                }

                if (msg == WM_MOUSEMOVE) {
                    if (!ctx->drag) return 0;
                    ctx->p_cur = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    InvalidateRect(hwnd, nullptr, FALSE); // 触发重绘
                    return 0;
                }

                if (msg == WM_LBUTTONUP) {
                    if (!ctx->drag) return 0;
                    ctx->p_cur = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
                    ctx->drag = false;
                    ReleaseCapture(); // 释放捕获
                    ctx->done  = cv::Rect2i(ctx->p_beg, ctx->p_cur).area() > 0;
                    DestroyWindow(hwnd);
                    return 0;
                }

                if (msg == WM_KEYDOWN) {
                    if (wp == VK_ESCAPE) DestroyWindow(hwnd); // ESC键中断截图
                    return 0;
                }

                if (msg == WM_DESTROY) {
                    PostQuitMessage(0);
                    return 0;
                }
            }

            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        // 截取当前全屏图像
        void shot(Context& ctx) {
            ctx.vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
            ctx.vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
            ctx.vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            ctx.vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

            ctx.r_pre = {0, 0, ctx.vw, ctx.vh}; // 首帧全屏暗色

            const BITMAPINFO bi{
                .bmiHeader{
                    .biSize        = sizeof(BITMAPINFOHEADER),
                    .biWidth       = ctx.vw,
                    .biHeight      = -ctx.vh,
                    .biPlanes      = 1,
                    .biBitCount    = 24,
                    .biCompression = BI_RGB,
                },
            };

            HDC h_scr = GetDC(nullptr);            // 屏幕上下文句柄
            HDC h_tmp = CreateCompatibleDC(h_scr); // 临时上下文句柄
            ctx.h_src = CreateCompatibleDC(h_scr);

            ctx.b_ori = CreateDIBSection(h_scr, &bi, DIB_RGB_COLORS, &ctx.p_ori, nullptr, 0);
            SelectObject(ctx.h_src, ctx.b_ori);
            BitBlt(ctx.h_src, 0, 0, ctx.vw, ctx.vh, h_scr, ctx.vx, ctx.vy, SRCCOPY);

            ctx.b_blk = CreateDIBSection(h_scr, &bi, DIB_RGB_COLORS, nullptr, nullptr, 0);
            SelectObject(h_tmp, ctx.b_blk);
            BitBlt(h_tmp, 0, 0, ctx.vw, ctx.vh, ctx.h_src, 0, 0, SRCCOPY);

            HBITMAP b_dot = CreateBitmap(1, 1, 1, 32, &px);
            SelectObject(ctx.h_src, b_dot);
            AlphaBlend(h_tmp, 0, 0, ctx.vw, ctx.vh, ctx.h_src, 0, 0, 1, 1, bf);

            SelectObject(ctx.h_src, ctx.b_ori); // 释放b_dot
            DeleteObject(b_dot);
            DeleteDC(h_tmp);
            ReleaseDC(nullptr, h_scr);
        }

        // 创建窗口等待交互完成
        void wait(Context& ctx) {
            HINSTANCE hi = GetModuleHandleW(nullptr);
            const WNDCLASSEXW wc{
                .cbSize        = sizeof(WNDCLASSEXW),
                .lpfnWndProc   = proc,
                .hInstance     = hi,
                .hCursor       = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)), // IDC_ARROW 标准指针
                .lpszClassName = L"S",
            };
            RegisterClassExW(&wc);

            HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"S", L"", WS_POPUP | WS_VISIBLE, ctx.vx, ctx.vy, ctx.vw, ctx.vh, nullptr, nullptr, hi, &ctx);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetForegroundWindow(hwnd);
            SetActiveWindow(hwnd);
            SetFocus(hwnd);
            UpdateWindow(hwnd); // 处理首帧

            MSG msg{};
            while (GetMessageW(&msg, nullptr, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            UnregisterClassW(L"S", hi);
        }

        // 截取框选区域的图像并写回
        void crop(const Context& ctx, cv::Mat& img) {
            if (!ctx.done) return;
            const int step = ((ctx.vw * 3 + 3) & ~3); // GDI内存对齐上取整至4倍数
            const auto roi = cv::Rect2i(ctx.p_beg, ctx.p_cur);
            img = cv::Mat(ctx.vh, ctx.vw, CV_8UC3, ctx.p_ori, step)(roi).clone();
        }

        // 清理Context中的句柄
        void drop(const Context& ctx) {
            DeleteDC(ctx.h_src);
            DeleteObject(ctx.b_blk);
            DeleteObject(ctx.b_ori);
        }
    }

    void show(cv::Mat& img) {
        Context ctx{};
        shot(ctx);
        wait(ctx);
        crop(ctx, img);
        drop(ctx);
    }

    void wrap(const std::function<void(const cv::Mat&)>& call) {
        SetProcessDPIAware(); // DPI感知

        RegisterHotKey(nullptr, 1, MOD_ALT, VK_OEM_5); // Alt+\ 开始截图
        RegisterHotKey(nullptr, 0, MOD_ALT, 'Q');      // Alt+Q 终止监听

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            if (msg.message == WM_HOTKEY) {
                if (msg.wParam == 0) break;
                cv::Mat img;
                show(img);
                call(img);
            } else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        UnregisterHotKey(nullptr, 1);
        UnregisterHotKey(nullptr, 0);
    }
}
