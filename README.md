# sfrmat5

This repository contains a C++ refactor of the MATLAB `sfrmat5` ISO 12233 slanted-edge SFR algorithm, along with original MATLAB implementation.

## how to run test

```Shell
$ ./run_tests.sh
```

The generated test executable also provides command-line and interactive modes:

```Shell
# Built-in regression test
/tmp/test_sfrmat5 --selftest

# Compute the whole image with default parameters
/tmp/test_sfrmat5 Example_Images/Test_edge1.bmp

# Whole image: polynomial order, sampling interval, window
/tmp/test_sfrmat5 Example_Images/Test_edge1.bmp --full 3 1 hamming

# 1-based inclusive ROI followed by optional parameters
/tmp/test_sfrmat5 Example_Images/Test_edge1.bmp 1 1 343 124 5 1 tukey

# Prompt for the image, ROI, polynomial order, sampling interval and window
/tmp/test_sfrmat5 --interactive
```

Run `/tmp/test_sfrmat5 --help` for the complete command syntax.

All modes print MTF30 as well as SFR50. `SfrResult<T>::sfr30` contains the
linearly interpolated frequency at 30% SFR, using the same first channel as
`sfr50` (R for RGB images). Units follow the frequency column in `dat`:
cycles/pixel for `del = 1`, otherwise cycles/mm. Like SFR50, if no crossing
is found, the existing frequency-search routine returns the last frequency.

### Mouse ROI selection

When OpenCV is available through `pkg-config opencv4`, `run_tests.sh` enables
mouse selection automatically (Ubuntu: `sudo apt install libopencv-dev pkg-config`).

```sh
/tmp/test_sfrmat5 Example_Images/Test_edge1.bmp --roi
/tmp/test_sfrmat5 Example_Images/Test_edge1.bmp --roi 3 1 hamming
```

Drag a rectangle with the mouse and press Enter or Space to accept. C resets the
rectangle; Esc cancels the run. Large images are scaled for display; computation
uses the selected original-resolution pixels. The ROI must be at least 4 x 4 pixels.
In `--interactive` mode, enter `mouse` at the ROI prompt to open the selector.
Without OpenCV or a graphical session, coordinates are requested in the terminal.
Set `SFRMAT5_FORCE_CONSOLE_ROI=1` to explicitly use terminal coordinates.
