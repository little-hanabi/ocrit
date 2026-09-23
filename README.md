# OCRit

> [!IMPORTANT]
> - Not a professional tool. Use at your own risk.

A Windows OCR tool using ONNX Runtime and OpenCV. It supports screen capture OCR to clipboard and image OCR to text.

## Features

- Screen capture OCR, result copied to clipboard.
- Batch or single image OCR, output as `.txt`.
- ONNX Runtime inference.

## Usage

```
ocrit <model_dir> [<arg_1> ... <arg_5>]
```

- capture screen and copy OCR result to clipboard.
- `<arg_1> ... <arg_5>`: optional float parameters.

```
ocrit <model_dir> <path_in> <path_out> [<arg_1> ... <arg_5>]
```

- if `<path_in>` is a directory, process all images and save `.txt` files to `<path_out>`; if `<path_in>` is a file, save result to `<path_out>`.
- `<arg_1> ... <arg_5>`: optional float parameters.

## Build

```bash
cmake -B build -S . -G Ninja -D CMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## License

See `LICENSE`