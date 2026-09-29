#include "sfrmat5.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef SFRMAT5_HAVE_OPENCV
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace {

using Scalar = double;

struct Image {
    int rows = 0;
    int cols = 0;
    int channels = 0;
    std::vector<sfrmat5::Matrix<Scalar>> planes;

    /// Constructs an empty test image container.
    Image() = default;

    /// Constructs a test image with channel planes initialized to a constant value.
    Image(int r, int c, int ch, Scalar value = static_cast<Scalar>(0))
        : rows(r), cols(c), channels(ch), planes(ch, sfrmat5::Matrix<Scalar>(r, c)) {
        for (int i = 0; i < ch; ++i) {
            planes[i].setConstant(value);
        }
    }
};

/// Returns true when a value is numerically close to zero.
bool nearly_zero(double v) {
    return std::abs(v) < 1e-9;
}

/// Returns true when two finite values are within the requested tolerance.
bool nearly_equal(double actual, double expected, double tol) {
    return std::isfinite(actual) && std::abs(actual - expected) <= tol;
}

/// Verifies that the first SFR data column is a strictly increasing frequency axis.
bool check_frequency_axis(const sfrmat5::Matrix<Scalar>& dat) {
    if (dat.rows() < 2 || dat.cols() < 2) {
        return false;
    }
    double prev = dat(0, 0);
    for (int i = 1; i < dat.rows(); ++i) {
        double cur = dat(i, 0);
        if (!(cur > prev)) {
            return false;
        }
        prev = cur;
    }
    return true;
}

/// Checks a scalar value against an expected value and reports failures.
bool check_value(const char* label, double actual, double expected, double tol) {
    if (nearly_equal(actual, expected, tol)) {
        return true;
    }
    std::cerr << label << " mismatch: expected " << expected << ", got " << actual
              << ", tolerance " << tol << "\n";
    return false;
}

/// Checks one matrix element against an expected value and reports failures.
bool check_matrix_value(const char* label, const sfrmat5::Matrix<Scalar>& m, int row, int col,
                        double expected, double tol) {
    if (row >= m.rows() || col >= m.cols()) {
        std::cerr << label << " index out of range at (" << row << ", " << col << ")\n";
        return false;
    }
    return check_value(label, m(row, col), expected, tol);
}

/// Loads an image file into planar scalar channels using stb_image.
Image load_image(const std::string& path) {
    int width = 0;
    int height = 0;
    int source_channels = 0;
    if (!stbi_info(path.c_str(), &width, &height, &source_channels)) {
        throw std::runtime_error(std::string("Failed to inspect image: ") + stbi_failure_reason());
    }

    const int output_channels = (source_channels == 1) ? 1 : 3;
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> data(
        stbi_load(path.c_str(), &width, &height, &source_channels, output_channels),
        stbi_image_free);
    if (!data) {
        throw std::runtime_error(std::string("Failed to load image: ") + stbi_failure_reason());
    }

    Image img(height, width, output_channels, static_cast<Scalar>(0));
    for (int row = 0; row < img.rows; ++row) {
        for (int col = 0; col < img.cols; ++col) {
            const size_t pixel_offset =
                (static_cast<size_t>(row) * img.cols + static_cast<size_t>(col)) * img.channels;
            for (int ch = 0; ch < img.channels; ++ch) {
                img.planes[ch](row, col) = static_cast<Scalar>(data.get()[pixel_offset + ch]);
            }
        }
    }
    return img;
}

/// Flattens image planes into the public SfrMat5 planar pixel layout.
std::vector<Scalar> extract_planar_pixels(const Image& img) {
    std::vector<Scalar> pixels(static_cast<size_t>(img.rows) * static_cast<size_t>(img.cols) *
                               static_cast<size_t>(img.channels));
    const size_t plane_size = static_cast<size_t>(img.rows) * static_cast<size_t>(img.cols);
    for (int ch = 0; ch < img.channels; ++ch) {
        const size_t channel_offset = static_cast<size_t>(ch) * plane_size;
        for (int row = 0; row < img.rows; ++row) {
            const size_t row_offset = channel_offset + static_cast<size_t>(row) * img.cols;
            for (int col = 0; col < img.cols; ++col) {
                pixels[row_offset + col] = img.planes[ch](row, col);
            }
        }
    }
    return pixels;
}

/// Returns a 1-based, inclusive ROI from an image.
Image crop_image(const Image& image, const std::array<int, 4>& roi) {
    const int x1 = roi[0];
    const int y1 = roi[1];
    const int x2 = roi[2];
    const int y2 = roi[3];
    if (x1 < 1 || y1 < 1 || x2 < x1 || y2 < y1 || x2 > image.cols ||
        y2 > image.rows) {
        throw std::invalid_argument("ROI is outside the image or has invalid coordinates");
    }

    Image cropped(y2 - y1 + 1, x2 - x1 + 1, image.channels);
    for (int ch = 0; ch < image.channels; ++ch) {
        cropped.planes[ch] =
            image.planes[ch].block(y1 - 1, x1 - 1, cropped.rows, cropped.cols);
    }
    return cropped;
}

bool parse_window(const std::string& value, sfrmat5::WindowFlag& window) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "tukey" || normalized == "0") {
        window = sfrmat5::WindowFlag::Tukey;
        return true;
    }
    if (normalized == "hamming" || normalized == "1") {
        window = sfrmat5::WindowFlag::Hamming;
        return true;
    }
    return false;
}

int parse_int(const std::string& value, const char* name) {
    size_t used = 0;
    int parsed = 0;
    try {
        parsed = std::stoi(value, &used);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + value);
    }
    if (used != value.size()) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + value);
    }
    return parsed;
}

double parse_double(const std::string& value, const char* name) {
    size_t used = 0;
    double parsed = 0;
    try {
        parsed = std::stod(value, &used);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + value);
    }
    if (used != value.size() || !std::isfinite(parsed)) {
        throw std::invalid_argument(std::string("Invalid ") + name + ": " + value);
    }
    return parsed;
}

void print_result(const sfrmat5::SfrResult<Scalar>& result, bool print_sfr_rows) {
    std::cout << "SFR50: " << result.sfr50 << "\n";
    std::cout << "SFR30: " << result.sfr30 << "\n";
    if (result.e.rows() > 0 && result.e.cols() > 0) {
        std::cout << "Sampling efficiency (10%): ";
        for (int c = 0; c < result.e.cols(); ++c) {
            if (c != 0) {
                std::cout << ", ";
            }
            std::cout << result.e(0, c);
        }
        std::cout << "\n";
    }
    if (!print_sfr_rows) {
        return;
    }
    std::cout << "First " << result.dat.rows() << " SFR rows (freq, mtf...):\n";
    for (int row = 0; row < result.dat.rows(); ++row) {
        for (int col = 0; col < result.dat.cols(); ++col) {
            if (col != 0) {
                std::cout << ", ";
            }
            std::cout << result.dat(row, col);
        }
        std::cout << "\n";
    }
}

std::string quoted_string(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (unsigned char ch : value) {
        switch (ch) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (ch < 0x20) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<int>(ch) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(ch);
            }
        }
    }
    output << '"';
    return output.str();
}

std::string lowercase_extension(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    std::string extension = (dot == std::string::npos) ? "" : path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension;
}

void write_json_matrix(std::ostream& output, const sfrmat5::Matrix<Scalar>& matrix,
                       const std::string& indent) {
    output << "[\n";
    for (int row = 0; row < matrix.rows(); ++row) {
        output << indent << "  [";
        for (int col = 0; col < matrix.cols(); ++col) {
            if (col != 0) output << ", ";
            output << matrix(row, col);
        }
        output << "]" << (row + 1 == matrix.rows() ? "\n" : ",\n");
    }
    output << indent << "]";
}

void write_yaml_matrix(std::ostream& output, const sfrmat5::Matrix<Scalar>& matrix,
                       const std::string& indent) {
    if (matrix.rows() == 0) {
        output << " []\n";
        return;
    }
    output << "\n";
    for (int row = 0; row < matrix.rows(); ++row) {
        output << indent << "- [";
        for (int col = 0; col < matrix.cols(); ++col) {
            if (col != 0) output << ", ";
            output << matrix(row, col);
        }
        output << "]\n";
    }
}

std::vector<std::string> sfr_column_names(int channels) {
    if (channels == 1) return {"frequency", "gray"};
    if (channels == 3) return {"frequency", "red", "green", "blue", "luminance"};
    std::vector<std::string> names = {"frequency"};
    for (int channel = 0; channel < channels; ++channel) {
        names.push_back("channel_" + std::to_string(channel + 1));
    }
    return names;
}

void write_result_file(const std::string& output_path, const std::string& image_path,
                       int source_width, int source_height, int roi_width, int roi_height,
                       int channels, const std::array<int, 4>& roi, int npol, double del,
                       sfrmat5::WindowFlag window, const sfrmat5::SfrResult<Scalar>& result) {
    const std::string extension = lowercase_extension(output_path);
    if (extension != ".json" && extension != ".yaml" && extension != ".yml") {
        throw std::invalid_argument("Output file must use .json, .yaml, or .yml extension");
    }
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("Failed to open output file: " + output_path);
    }
    output << std::setprecision(std::numeric_limits<Scalar>::max_digits10);
    const std::string window_name =
        window == sfrmat5::WindowFlag::Tukey ? "tukey" : "hamming";
    const std::vector<std::string> columns = sfr_column_names(channels);
    const std::string absolute_image_path =
        std::filesystem::absolute(image_path).lexically_normal().string();

    if (extension == ".json") {
        output << "{\n"
               << "  \"image\": " << quoted_string(absolute_image_path) << ",\n"
               << "  \"source_size\": {\"width\": " << source_width
               << ", \"height\": " << source_height << "},\n"
               << "  \"roi\": {\"x1\": " << roi[0] << ", \"y1\": " << roi[1]
               << ", \"x2\": " << roi[2] << ", \"y2\": " << roi[3]
               << ", \"width\": " << roi_width << ", \"height\": " << roi_height << "},\n"
               << "  \"channels\": " << channels << ",\n"
               << "  \"parameters\": {\"npol\": " << npol << ", \"del\": " << del
               << ", \"window\": " << quoted_string(window_name) << "},\n"
               << "  \"results\": {\n"
               << "    \"status\": " << result.status << ",\n"
               << "    \"sfr50\": " << result.sfr50 << ",\n"
               << "    \"sfr30\": " << result.sfr30 << ",\n"
               << "    \"edge_angle_degrees\": " << result.edge_angle_degrees << ",\n"
               << "    \"nbin\": " << result.nbin << ",\n"
               << "    \"del2\": " << result.del2 << ",\n"
               << "    \"sfr_columns\": [";
        for (size_t index = 0; index < columns.size(); ++index) {
            if (index != 0) output << ", ";
            output << quoted_string(columns[index]);
        }
        output << "],\n    \"sampling_efficiency\": ";
        write_json_matrix(output, result.e, "    ");
        output << ",\n    \"fit_coefficients\": ";
        write_json_matrix(output, result.fitme, "    ");
        output << ",\n    \"esf\": [";
        for (size_t index = 0; index < result.esf.size(); ++index) {
            if (index != 0) output << ", ";
            output << result.esf[index];
        }
        output << "],\n    \"sfr_data\": ";
        write_json_matrix(output, result.dat, "    ");
        output << "\n  }\n}\n";
    } else {
        output << "image: " << quoted_string(absolute_image_path) << "\n"
               << "source_size:\n  width: " << source_width << "\n  height: " << source_height
               << "\nroi:\n  x1: " << roi[0] << "\n  y1: " << roi[1] << "\n  x2: " << roi[2]
               << "\n  y2: " << roi[3] << "\n  width: " << roi_width << "\n  height: " << roi_height
               << "\nchannels: " << channels << "\nparameters:\n  npol: " << npol
               << "\n  del: " << del << "\n  window: " << quoted_string(window_name)
               << "\nresults:\n  status: " << result.status << "\n  sfr50: " << result.sfr50
               << "\n  sfr30: " << result.sfr30
               << "\n  edge_angle_degrees: " << result.edge_angle_degrees
               << "\n  nbin: " << result.nbin
               << "\n  del2: " << result.del2 << "\n  sfr_columns: [";
        for (size_t index = 0; index < columns.size(); ++index) {
            if (index != 0) output << ", ";
            output << quoted_string(columns[index]);
        }
        output << "]\n  sampling_efficiency:";
        write_yaml_matrix(output, result.e, "    ");
        output << "  fit_coefficients:";
        write_yaml_matrix(output, result.fitme, "    ");
        output << "  esf: [";
        for (size_t index = 0; index < result.esf.size(); ++index) {
            if (index != 0) output << ", ";
            output << result.esf[index];
        }
        output << "]\n  sfr_data:";
        write_yaml_matrix(output, result.dat, "    ");
    }
    if (!output) {
        throw std::runtime_error("Failed to write output file: " + output_path);
    }
}

std::array<int, 4> select_roi(const Image& image) {
#ifdef SFRMAT5_HAVE_OPENCV
    const char* force = std::getenv("SFRMAT5_FORCE_CONSOLE_ROI");
    bool gui = !(force && std::string(force) != "0");
#ifdef __linux__
    const char* display = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    gui = gui && ((display && *display) || (wayland && *wayland));
#endif
    if (gui) {
        const std::string title = "Select ROI - Enter/Space: accept, Esc: cancel";
        cv::Rect box;
        cv::Mat view;
        try {
            cv::Mat rgb(image.rows, image.cols, image.channels == 1 ? CV_8UC1 : CV_8UC3);
            for (int y = 0; y < image.rows; ++y) {
                for (int x = 0; x < image.cols; ++x) {
                    if (image.channels == 1) {
                        rgb.at<unsigned char>(y, x) = cv::saturate_cast<unsigned char>(image.planes[0](y, x));
                    } else {
                        rgb.at<cv::Vec3b>(y, x) = cv::Vec3b(
                            cv::saturate_cast<unsigned char>(image.planes[2](y, x)),
                            cv::saturate_cast<unsigned char>(image.planes[1](y, x)),
                            cv::saturate_cast<unsigned char>(image.planes[0](y, x)));
                    }
                }
            }
            const double scale = std::min({1.0, 1400.0 / image.cols, 900.0 / image.rows});
            cv::resize(rgb, view, cv::Size(std::max(1, static_cast<int>(image.cols * scale)),
                                         std::max(1, static_cast<int>(image.rows * scale))));
            std::cout << "Drag a rectangle; Enter/Space accepts, C resets, Esc cancels.\n";
            box = cv::selectROI(title, view, true, false);
            cv::destroyWindow(title);
        } catch (const cv::Exception& error) {
            try { cv::destroyWindow(title); } catch (const cv::Exception&) {}
            throw std::runtime_error(std::string("ROI window failed: ") + error.what());
        }
        if (box.empty()) {
            throw std::runtime_error("ROI selection cancelled");
        }
        const double sx = static_cast<double>(image.cols) / view.cols;
        const double sy = static_cast<double>(image.rows) / view.rows;
        return {static_cast<int>(std::floor(box.x * sx)) + 1,
                static_cast<int>(std::floor(box.y * sy)) + 1,
                std::min(image.cols, static_cast<int>(std::ceil((box.x + box.width) * sx))),
                std::min(image.rows, static_cast<int>(std::ceil((box.y + box.height) * sy)))};
    }
#endif
    std::cout << "Mouse ROI unavailable. Image: " << image.cols << " x " << image.rows
              << ". Enter x1 y1 x2 y2 (1-based, inclusive): " << std::flush;
    std::string line, extra;
    std::array<int, 4> roi{};
    if (!std::getline(std::cin, line)) throw std::runtime_error("ROI selection cancelled");
    std::istringstream input(line);
    if (!(input >> roi[0] >> roi[1] >> roi[2] >> roi[3]) || input >> extra)
        throw std::invalid_argument("ROI requires exactly four integer coordinates");
    return roi;
}

int compute_file(const std::string& path, const std::array<int, 4>* roi, int npol, double del,
                 sfrmat5::WindowFlag window, bool mouse_roi = false,
                 const std::string& output_path = "") {
    if (npol < 1 || npol > 5) {
        throw std::invalid_argument("npol must be between 1 and 5");
    }
    if (!(del > 0)) {
        throw std::invalid_argument("sampling interval must be positive");
    }

    Image image = load_image(path);
    const int source_width = image.cols;
    const int source_height = image.rows;
    std::array<int, 4> selected{};
    if (mouse_roi) {
        selected = select_roi(image);
        roi = &selected;
    }
    if (roi != nullptr) {
        image = crop_image(image, *roi);
        std::cout << "ROI: " << (*roi)[0] << " " << (*roi)[1] << " " << (*roi)[2]
                  << " " << (*roi)[3] << "\n";
    }
    const std::array<int, 4> effective_roi =
        roi != nullptr ? *roi : std::array<int, 4>{1, 1, source_width, source_height};
    if (image.cols < 4 || image.rows < 4)
        throw std::invalid_argument("ROI must be at least 4 x 4 pixels");
    auto pixels = std::make_unique<std::vector<Scalar>>(extract_planar_pixels(image));
    sfrmat5::SfrMat5<Scalar> sfr;
    sfr.set_npol(npol);
    sfr.set_del(del);
    sfr.set_wflag(window);
    const sfrmat5::SfrResult<Scalar> result =
        sfr.compute(std::move(pixels), image.cols, image.rows, image.channels);
    if (result.status != 0 || result.dat.rows() == 0) {
        std::cerr << "sfrmat5 failed for " << path << "\n";
        return 1;
    }

    std::cout << "Image: " << path << "\n"
              << "Size: " << image.cols << " x " << image.rows << ", channels: "
              << image.channels << "\n"
              << "Edge fit order: " << npol << "\n"
              << "Sampling interval: " << del << "\n"
              << "Window: "
              << (window == sfrmat5::WindowFlag::Tukey ? "Tukey" : "Hamming") << "\n";
    print_result(result, false);
    if (!output_path.empty()) {
        write_result_file(output_path, path, source_width, source_height, image.cols, image.rows,
                          image.channels, effective_roi, npol, del, window, result);
        std::cout << "Output: " << output_path << "\n";
    }
    return 0;
}

} // namespace

/// Runs the regression test against the example edge image.
int run_selftest() {
    std::string path = "Example_Images/Test_edge1.bmp";
    Image img = load_image(path);
    auto pixels = std::make_unique<std::vector<Scalar>>(extract_planar_pixels(img));
    sfrmat5::SfrMat5<Scalar> sfr;
    sfrmat5::SfrResult<Scalar> result =
        sfr.compute(std::move(pixels), img.cols, img.rows, img.channels);

    if (result.dat.rows() == 0 || result.dat.cols() < 2) {
        std::cerr << "SFR data missing\n";
        return 1;
    }
    if (result.dat.rows() != 125 || result.dat.cols() != 5) {
        std::cerr << "Unexpected SFR data dimensions: " << result.dat.rows() << "x"
                  << result.dat.cols() << "\n";
        return 1;
    }
    if (!check_frequency_axis(result.dat)) {
        std::cerr << "Frequency axis not increasing\n";
        return 1;
    }
    if (nearly_zero(result.sfr50) || std::isnan(result.sfr50)) {
        std::cerr << "SFR50 invalid\n";
        return 1;
    }
    if (result.e.rows() == 0 || result.e.cols() == 0) {
        std::cerr << "Sampling efficiency missing\n";
        return 1;
    }
    if (result.e.rows() != 2 || result.e.cols() != 4) {
        std::cerr << "Unexpected sampling efficiency dimensions: " << result.e.rows() << "x"
                  << result.e.cols() << "\n";
        return 1;
    }

    const double value_tol = 1e-5;
    const double freq_tol = 1e-6;
    bool numerical_ok = true;
    // Independently interpolate the first channel's 30% crossing.
    double expected30 = 0.0;
    bool crossed30 = false;
    for (int row = 1; row < result.dat.rows(); ++row) {
        if (result.dat(row, 1) < 0.3) {
            const double y0 = result.dat(row - 1, 1);
            const double y1 = result.dat(row, 1);
            expected30 = result.dat(row - 1, 0) + (0.3 - y0) / (y1 - y0) *
                (result.dat(row, 0) - result.dat(row - 1, 0));
            crossed30 = true;
            break;
        }
    }
    numerical_ok &= crossed30 && check_value("SFR30", result.sfr30, expected30, 1e-12);
    numerical_ok &= check_value("SFR50", result.sfr50, 0.269805, value_tol);
    numerical_ok &=
        check_value("edge angle", result.edge_angle_degrees, -5.48564, value_tol);
    numerical_ok &= check_value("del2", result.del2, 0.248855, value_tol);

    numerical_ok &= check_matrix_value("sampling efficiency 10% R", result.e, 0, 0, 85.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 10% G", result.e, 0, 1, 85.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 10% B", result.e, 0, 2, 86.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 10% L", result.e, 0, 3, 85.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 50% R", result.e, 1, 0, 55.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 50% G", result.e, 1, 1, 55.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 50% B", result.e, 1, 2, 56.0, 0.0);
    numerical_ok &= check_matrix_value("sampling efficiency 50% L", result.e, 1, 3, 55.0, 0.0);

    numerical_ok &= check_matrix_value("dat[0,0]", result.dat, 0, 0, 0.0, freq_tol);
    numerical_ok &= check_matrix_value("dat[0,1]", result.dat, 0, 1, 1.0, value_tol);
    numerical_ok &= check_matrix_value("dat[1,0]", result.dat, 1, 0, 0.00810162, freq_tol);
    numerical_ok &= check_matrix_value("dat[1,4]", result.dat, 1, 4, 0.994307, value_tol);
    numerical_ok &= check_matrix_value("dat[33,0]", result.dat, 33, 0, 0.267353, freq_tol);
    numerical_ok &= check_matrix_value("dat[33,4]", result.dat, 33, 4, 0.511463, value_tol);
    numerical_ok &= check_matrix_value("dat[61,0]", result.dat, 61, 0, 0.494199, freq_tol);
    numerical_ok &= check_matrix_value("dat[61,4]", result.dat, 61, 4, 0.0146672, value_tol);
    numerical_ok &= check_matrix_value("dat[124,0]", result.dat, 124, 0, 1.0046, value_tol);
    numerical_ok &= check_matrix_value("dat[124,4]", result.dat, 124, 4, 0.0797956, value_tol);
    if (!numerical_ok) {
        return 1;
    }

    std::cout << "sfrcpp5 basic test passed\n";
    // print_result(result, true);
    return 0;
}

void print_usage(const char* program) {
    std::cout << "Usage:\n"
              << "  " << program << "\n"
              << "  " << program << " --selftest\n"
              << "  " << program << " --interactive [tukey|hamming]\n"
              << "  " << program << " image [-o output.json|output.yaml]\n"
              << "  " << program << " image --roi [npol [del [tukey|hamming]]] [-o output.json|output.yaml]\n"
              << "  " << program << " image --full [npol [del [tukey|hamming]]] [-o output.json|output.yaml]\n"
              << "  " << program
              << " image x1 y1 x2 y2 [npol [del [tukey|hamming]]] [-o output.json|output.yaml]\n"
              << "ROI coordinates are 1-based and inclusive.\n";
}

int run_file(int argc, char** argv) {
    const std::string path = argv[1];
    std::vector<std::string> arguments;
    std::string output_path;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "-o" || argument == "--output") {
            if (!output_path.empty()) {
                throw std::invalid_argument("output option may only be specified once");
            }
            if (index + 1 >= argc) {
                throw std::invalid_argument("-o requires an output file path");
            }
            output_path = argv[++index];
        } else {
            arguments.push_back(argument);
        }
    }
    std::array<int, 4> roi{};
    const std::array<int, 4>* roi_ptr = nullptr;
    int npol = 5;
    double del = 1.0;
    sfrmat5::WindowFlag window = sfrmat5::WindowFlag::Tukey;
    size_t option = 0;
    bool mouse_roi = false;

    if (arguments.size() > option && arguments[option] == "--roi") {
        mouse_roi = true;
        ++option;
    } else if (arguments.size() > option && arguments[option] == "--full") {
        ++option;
    } else if (arguments.size() > option) {
        if (arguments.size() - option < 4) {
            throw std::invalid_argument("ROI requires four coordinates: x1 y1 x2 y2");
        }
        for (int index = 0; index < 4; ++index) {
            roi[index] = parse_int(arguments[option + index], "ROI coordinate");
        }
        roi_ptr = &roi;
        option += 4;
    }

    if (arguments.size() > option) {
        npol = parse_int(arguments[option++], "npol");
    }
    if (arguments.size() > option) {
        del = parse_double(arguments[option++], "sampling interval");
    }
    if (arguments.size() > option && !parse_window(arguments[option++], window)) {
        throw std::invalid_argument("window must be tukey or hamming");
    }
    if (arguments.size() > option) {
        throw std::invalid_argument("too many arguments");
    }
    return compute_file(path, roi_ptr, npol, del, window, mouse_roi, output_path);
}

int run_interactive(int argc, char** argv) {
    sfrmat5::WindowFlag window = sfrmat5::WindowFlag::Tukey;
    if (argc >= 3 && !parse_window(argv[2], window)) {
        throw std::invalid_argument("window must be tukey or hamming");
    }
    if (argc > 3) {
        throw std::invalid_argument("too many arguments for --interactive");
    }

    std::string path;
    std::cout << "Image path: ";
    if (!std::getline(std::cin, path) || path.empty()) {
        std::cerr << "No image selected\n";
        return 1;
    }

    std::array<int, 4> roi{};
    const std::array<int, 4>* roi_ptr = nullptr;
    std::string line;
    std::cout << "ROI: mouse, x1 y1 x2 y2, or Enter for full image: ";
    if (!std::getline(std::cin, line)) {
        return 1;
    }
    const bool mouse_roi = line == "mouse" || line == "--roi";
    if (!line.empty() && !mouse_roi) {
        std::istringstream input(line);
        std::string extra;
        if (!(input >> roi[0] >> roi[1] >> roi[2] >> roi[3]) || input >> extra) {
            throw std::invalid_argument("ROI must contain exactly four integers");
        }
        roi_ptr = &roi;
    }

    int npol = 5;
    std::cout << "Edge fit order [5]: ";
    if (!std::getline(std::cin, line)) {
        return 1;
    }
    if (!line.empty()) {
        npol = parse_int(line, "npol");
    }

    double del = 1.0;
    std::cout << "Sampling interval [1]: ";
    if (!std::getline(std::cin, line)) {
        return 1;
    }
    if (!line.empty()) {
        del = parse_double(line, "sampling interval");
    }

    if (argc < 3) {
        std::cout << "Window, tukey or hamming [tukey]: ";
        if (!std::getline(std::cin, line)) {
            return 1;
        }
        if (!line.empty() && !parse_window(line, window)) {
            throw std::invalid_argument("window must be tukey or hamming");
        }
    }
    return compute_file(path, roi_ptr, npol, del, window, mouse_roi);
}

int main(int argc, char** argv) {
    try {
        if (argc == 1 || std::string(argv[1]) == "--selftest") {
            return run_selftest();
        }
        if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
            print_usage(argv[0]);
            return 0;
        }
        if (std::string(argv[1]) == "--interactive") {
            return run_interactive(argc, argv);
        }
        if (std::string(argv[1]).rfind("--", 0) == 0) {
            std::cerr << "Unknown option: " << argv[1] << "\n";
            print_usage(argv[0]);
            return 2;
        }
        return run_file(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
