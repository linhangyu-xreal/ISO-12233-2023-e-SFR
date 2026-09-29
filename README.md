# sfrcpp5

[中文说明](README_zh.md) | English

`sfrcpp5` is a C++17 refactor of the MATLAB `sfrmat5` implementation of the
ISO 12233 slanted-edge spatial frequency response (SFR/MTF) algorithm. It
computes SFR50, SFR30, sampling efficiency, edge angle, ESF, and the complete
frequency/SFR data table. Results can be written as JSON or YAML for batch
processing and further analysis.

## 1. Requirements

- A C++17-compatible `g++`
- The vendored Eigen headers in `third_party/eigen-3.4.0`
- Optional: OpenCV 4 for mouse-based ROI selection
- Optional: `pkg-config` for automatic OpenCV 4 detection during the build

Images are loaded with the vendored `stb_image`. Common choices for this
project include BMP, PGM, PNG, and JPEG files.

## 2. Build

From the repository root, run:

```bash
cd /home/hylin/projects/sfrcpp5
./run_tests.sh
```

The script:

1. compiles `cpp/sfrmat5.cpp` and `cpp/test_sfrmat5.cpp` with C++17;
2. creates the executable `bin/sfr5`;
3. runs the built-in regression test.

If OpenCV 4 is available through `pkg-config opencv4`, mouse-based ROI
selection is enabled automatically. Otherwise, full-image processing and
coordinate-based ROI selection remain available.

## 3. Command-line help

Display the built-in help with:

```bash
bin/sfr5 --help
```

The supported command forms are:

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

ROI coordinates are **1-based and inclusive**. For example, `1 1 100 80`
selects columns 1 through 100 and rows 1 through 80, producing a 100 × 80
pixel ROI. Both ROI dimensions must be at least 4 pixels.

## 4. Arguments

| Argument | Meaning | Default or constraint |
| --- | --- | --- |
| `image` | Input image path | Required except for the built-in test |
| `x1 y1 x2 y2` | Top-left and bottom-right ROI coordinates | 1-based and inclusive |
| `--roi` | Select an ROI with the mouse | Requires OpenCV; otherwise falls back to console input |
| `--full` | Explicitly process the full image | Optional |
| `npol` | Polynomial order for edge-location fitting | Default `5`; valid range `1`–`5` |
| `del` | Sampling interval | Default `1`; must be greater than `0` |
| `tukey` / `hamming` | Window function | Default `tukey` |
| `-o` / `--output` | Machine-readable result file | Supports `.json`, `.yaml`, and `.yml` |

`npol`, `del`, and the window name are ordered positional arguments. To set a
later argument, include the preceding values as well. For example, select the
Hamming window with `5 1 hamming`.

When `del=1`, frequency values and SFR50/SFR30 are expressed in cycles/pixel.
When `del>1`, the value is interpreted as DPI and converted to pixel spacing
with `25.4/del`; results are then expressed in cycles/mm.

## 5. Examples

### 5.1 Run the built-in test

Running the executable without arguments is equivalent to `--selftest`:

```bash
bin/sfr5
bin/sfr5 --selftest
```

### 5.2 Process the full image with default parameters

```bash
bin/sfr5 Example_Images/m0000100.pgm
```

The explicit equivalent is:

```bash
bin/sfr5 Example_Images/m0000100.pgm --full
```

The default computation parameters are `npol=5`, `del=1`, and
`window=tukey`.

### 5.3 Process the full image with custom parameters

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --full 3 1 hamming
```

### 5.4 Process a coordinate-based ROI

```bash
bin/sfr5 Example_Images/m0000100.pgm \
  316 164 334 172 \
  5 1 tukey
```

In this example:

- the top-left point is `(316, 164)`;
- the bottom-right point is `(334, 172)`;
- the polynomial order is `5`;
- the sampling interval is `1`;
- the Tukey window is selected.

### 5.5 Write JSON output

```bash
bin/sfr5 Example_Images/m0000100.pgm \
  316 164 334 172 \
  5 1 tukey \
  -o result.json
```

The `-o` option may appear anywhere after the image path, but it may only be
specified once.

### 5.6 Write YAML output

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --full 5 1 tukey \
  -o result.yaml
```

### 5.7 Select an ROI with the mouse

When the executable was built with OpenCV support, run:

```bash
bin/sfr5 Example_Images/Test_edge1.bmp --roi
bin/sfr5 Example_Images/Test_edge1.bmp --roi 3 1 hamming
```

Controls:

- drag to select a rectangle;
- press Enter or Space to accept;
- press `C` to clear and select again;
- press Esc to cancel.

Large images may be scaled for display, but processing always uses pixels from
the original resolution. Without OpenCV or a graphical session, the program
falls back to reading ROI coordinates from the terminal. To force console ROI
input, use:

```bash
SFRMAT5_FORCE_CONSOLE_ROI=1 \
  bin/sfr5 Example_Images/Test_edge1.bmp --roi
```

### 5.8 Interactive mode

```bash
bin/sfr5 --interactive
```

The program prompts for the image path, ROI, polynomial order, sampling
interval, and window. The window may also be selected in advance:

```bash
bin/sfr5 --interactive hamming
```

## 6. Output

The terminal output includes the input image, effective ROI, channel count,
computation parameters, SFR50, SFR30, edge angle, and sampling efficiency.

With `-o`, the JSON or YAML file also contains:

- the absolute input image path and original dimensions;
- effective ROI coordinates and dimensions;
- the channel count and `npol`, `del`, and window settings;
- the `status` code;
- `sfr50` and `sfr30`;
- the signed edge angle from vertical in `edge_angle_degrees`;
- the oversampling factor `nbin` and effective interval `del2`;
- `sampling_efficiency`;
- polynomial `fit_coefficients`;
- the edge-spread function `esf`;
- the complete frequency/SFR table in `sfr_data`.

For RGB images, SFR50 and SFR30 use the first channel (R) curve. Edge angle is
measured relative to vertical; it is positive when the edge leans to the right
as image row numbers increase.

## 7. Troubleshooting

### `ROI must be at least 4 x 4 pixels`

Increase `x2-x1+1` or `y2-y1+1` so both ROI dimensions are at least 4.

### `npol must be between 1 and 5`

Set `npol` to an integer from 1 through 5.

### `sampling interval must be positive`

Set `del` to a value greater than 0.

### `window must be tukey or hamming`

Use the lowercase window name `tukey` or `hamming`.

### `Output file must use .json, .yaml, or .yml extension`

Use `.json`, `.yaml`, or `.yml` as the output filename extension.

### Mouse ROI selection is unavailable

Install OpenCV 4 and `pkg-config`, then rebuild with `./run_tests.sh`. If mouse
selection is not required, pass `x1 y1 x2 y2` directly on the command line;
coordinate-based ROI selection does not depend on OpenCV.
