# OCRit

> [!IMPORTANT]
> - 非专业工具，使用风险自负。

基于 ONNX Runtime 和 OpenCV 的 Windows OCR 工具。支持屏幕捕获 OCR 到剪贴板，以及图像 OCR 输出文本。

## 特点

- 屏幕捕获 OCR，结果复制到剪贴板。
- 批量或单张图像 OCR，输出为 `.txt`。
- 使用 ONNX Runtime 推理。

## 使用

```
ocrit <model_dir> [<arg_1> ... <arg_5>]
```

- 捕获屏幕，OCR 结果复制到剪贴板。
- `<arg_1> ... <arg_5>`：可选 float 参数。

```
ocrit <model_dir> <path_in> <path_out> [<arg_1> ... <arg_5>]
```

- 若 `<path_in>` 是目录，处理其中所有图像并将 `.txt` 保存到 `<path_out>`；若 `<path_in>` 是文件，结果保存到 `<path_out>`。
- `<arg_1> ... <arg_5>`：可选 float 参数。

## 构建

```bash
cmake -B build -S . -G Ninja -D CMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 许可

见 `LICENSE`