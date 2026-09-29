# sfrcpp5 中文使用说明

中文 | [English](README.md)

`sfrcpp5` 是 MATLAB `sfrmat5` 的 C++17 重构版本，用于按照 ISO 12233
斜边法计算图像的空间频率响应（SFR/MTF）。程序可计算 SFR50、SFR30、
采样效率、边缘角度、ESF 以及完整的频率/SFR 数据，并支持输出 JSON 或
YAML，便于后续批处理和统计。

## 1. 环境要求

- 支持 C++17 的 `g++`
- 项目自带的 Eigen 头文件：`third_party/eigen-3.4.0`
- 可选：OpenCV 4，用于鼠标框选 ROI
- 可选：`pkg-config`，用于构建时自动检测 OpenCV 4

图像读取使用项目内置的 `stb_image`。本项目示例中常用 BMP、PGM、PNG
或 JPEG 图像。

## 2. 编译

在项目根目录执行：

```bash
cd /home/hylin/projects/sfrcpp5
./run_tests.sh
```

脚本会：

1. 使用 C++17 编译 `cpp/sfrmat5.cpp` 和 `cpp/test_sfrmat5.cpp`；
2. 生成可执行文件 `bin/sfr5`；
3. 运行内置回归测试。

如果系统能通过 `pkg-config opencv4` 找到 OpenCV 4，构建会自动启用鼠标
ROI 功能；否则仍可使用整幅图像或手动输入 ROI 坐标。

## 3. 命令行帮助

查看程序内置帮助：

```bash
bin/sfr5 --help
```

帮助内容对应以下命令格式：

```text
Usage:
  bin/sfr5
  bin/sfr5 --selftest
  bin/sfr5 --interactive [tukey|hamming]
  bin/sfr5 image [-o output.json|output.yaml]
  bin/sfr5 image --roi [npol [del [tukey|hamming]]] [-o output.json|output.yaml]
  bin/sfr5 image --full [npol [del [tukey|hamming]]] [-o output.json|output.yaml]
  bin/sfr5 image x1 y1 x2 y2 [npol [del [tukey|hamming]]] [-o output.json|output.yaml]
```

ROI 坐标采用 **1-based** 编号，并且包含右下角端点。例如
`1 1 100 80` 表示从第 1 列、第 1 行到第 100 列、第 80 行，ROI 大小为
100 × 80 像素。ROI 的宽和高均不得小于 4 像素。

## 4. 参数说明

| 参数 | 含义 | 默认值/限制 |
| --- | --- | --- |
| `image` | 输入图像路径 | 必填，内置测试除外 |
| `x1 y1 x2 y2` | ROI 左上角与右下角坐标 | 1-based，包含端点 |
| `--roi` | 使用鼠标选择 ROI | 需要启用 OpenCV；否则退回终端输入 |
| `--full` | 显式使用整幅图像 | 可省略 |
| `npol` | 边缘位置多项式拟合阶数 | 默认 `5`，有效范围 `1`–`5` |
| `del` | 采样间隔 | 默认 `1`，必须大于 `0` |
| `tukey` / `hamming` | 窗口函数 | 默认 `tukey` |
| `-o` / `--output` | 机器可读结果文件 | 支持 `.json`、`.yaml`、`.yml` |

`npol`、`del`、窗口函数是按顺序排列的位置参数。若要指定后一个参数，
需要同时写出前面的参数。例如使用 Hamming 窗时写成 `5 1 hamming`。

`del=1` 时，频率和 SFR50/SFR30 的单位为 cycles/pixel。当 `del>1` 时，
程序将其解释为 DPI，并用 `25.4/del` 换算像素间距，结果单位为
cycles/mm。

## 5. 常用示例

### 5.1 运行内置测试

不带参数或使用 `--selftest` 均会运行内置回归测试：

```bash
bin/sfr5
bin/sfr5 --selftest
```

### 5.2 使用整幅图像和默认参数

```bash
bin/sfr5 Example_Images/m0000100.pgm
```

等价的显式写法：

```bash
bin/sfr5 Example_Images/m0000100.pgm --full
```

默认计算参数为 `npol=5`、`del=1`、`window=tukey`。

### 5.3 使用整幅图像并指定参数

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --full 3 1 hamming
```

### 5.4 使用指定 ROI

```bash
bin/sfr5 Example_Images/m0000100.pgm \
  316 164 334 172 \
  5 1 tukey
```

其中：

- 左上点为 `(316, 164)`；
- 右下点为 `(334, 172)`；
- 多项式阶数为 `5`；
- 采样间隔为 `1`；
- 窗口函数为 Tukey。

### 5.5 输出 JSON

```bash
bin/sfr5 Example_Images/m0000100.pgm \
  316 164 334 172 \
  5 1 tukey \
  -o result.json
```

`-o` 也可以放在图像路径之后的其他位置，但每次只能指定一次。

### 5.6 输出 YAML

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --full 5 1 tukey \
  -o result.yaml
```

### 5.7 鼠标选择 ROI

构建时启用 OpenCV 后，可执行：

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --roi
bin/sfr5 Example_Images/Test_edge1.bmp --roi 3 1 hamming
```

操作方式：

- 拖动鼠标框选矩形；
- Enter 或空格：确认；
- `C`：清除并重新选择；
- Esc：取消。

大图在界面中可能会缩放显示，但计算仍使用原始分辨率中的像素。若没有
OpenCV 或图形环境，程序会改为从终端读取坐标。也可以强制使用终端输入：

```bash
SFRMAT5_FORCE_CONSOLE_ROI=1 \
  bin/sfr5 Example_Images/Test_edge1.bmp --roi
```

### 5.8 交互模式

```bash
bin/sfr5 --interactive
```

程序会依次询问图像路径、ROI、多项式阶数、采样间隔和窗口函数。也可以
预先指定窗口：

```bash
bin/sfr5 --interactive hamming
```

## 6. 输出结果

终端会显示输入图像、实际 ROI、通道数、计算参数、SFR50、SFR30、边缘
角度和采样效率等信息。

使用 `-o` 时，JSON/YAML 文件还会包含：

- 输入图像的绝对路径和原始尺寸；
- 实际 ROI 坐标和尺寸；
- 通道数及 `npol`、`del`、窗口函数；
- 状态码 `status`；
- `sfr50` 和 `sfr30`；
- 相对垂直方向的有符号边缘角度 `edge_angle_degrees`；
- 过采样倍数 `nbin` 和有效采样间隔 `del2`；
- 采样效率 `sampling_efficiency`；
- 多项式拟合系数 `fit_coefficients`；
- 边缘扩散函数 `esf`；
- 完整的频率/SFR 表 `sfr_data`。

对于 RGB 图像，SFR50 和 SFR30 使用第一个通道（R 通道）的曲线。边缘
角度以垂直方向为基准；随着图像行号增加，边缘向右倾斜时角度为正。

## 7. 故障排查

### `ROI must be at least 4 x 4 pixels`

ROI 太小，请增大 `x2-x1+1` 或 `y2-y1+1`。

### `npol must be between 1 and 5`

将 `npol` 设置为 1 到 5 之间的整数。

### `sampling interval must be positive`

`del` 必须大于 0。

### `window must be tukey or hamming`

窗口名称只能使用小写的 `tukey` 或 `hamming`。

### `Output file must use .json, .yaml, or .yml extension`

输出文件扩展名必须是 `.json`、`.yaml` 或 `.yml`。

### 无法使用鼠标选择 ROI

安装 OpenCV 4 和 `pkg-config` 后重新运行 `./run_tests.sh`。如果只需要手动
ROI，可以直接在命令行中传入 `x1 y1 x2 y2`，不依赖 OpenCV。
