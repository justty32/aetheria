#include "sim/debug_canvas.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>

namespace aetheria::sim {
namespace {

void append_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}

[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
    auto crc = UINT32_C(0xFFFFFFFF);
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) &
                                 static_cast<std::uint32_t>(-static_cast<std::int32_t>(crc & 1U)));
        }
    }
    return ~crc;
}

[[nodiscard]] std::uint32_t adler32(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const auto byte : bytes) {
        a = (a + byte) % 65521U;
        b = (b + a) % 65521U;
    }
    return (b << 16U) | a;
}

void append_chunk(std::vector<std::uint8_t>& png, std::string_view type,
                  std::span<const std::uint8_t> data) {
    append_u32(png, static_cast<std::uint32_t>(data.size()));
    const auto crc_begin = png.size();
    png.insert(png.end(), type.begin(), type.end());
    png.insert(png.end(), data.begin(), data.end());
    append_u32(png, crc32(std::span{png}.subspan(crc_begin)));
}

[[nodiscard]] std::vector<std::uint8_t> deflate_uncompressed(std::span<const std::uint8_t> raw) {
    std::vector<std::uint8_t> output{0x78, 0x01};
    std::size_t offset{};
    while (offset < raw.size()) {
        const auto length = static_cast<std::uint16_t>(
            std::min<std::size_t>(raw.size() - offset, std::numeric_limits<std::uint16_t>::max()));
        const bool final = offset + length == raw.size();
        output.push_back(final ? 1U : 0U);
        output.push_back(static_cast<std::uint8_t>(length));
        output.push_back(static_cast<std::uint8_t>(length >> 8U));
        const auto inverted = static_cast<std::uint16_t>(~length);
        output.push_back(static_cast<std::uint8_t>(inverted));
        output.push_back(static_cast<std::uint8_t>(inverted >> 8U));
        output.insert(output.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                      raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
        offset += length;
    }
    append_u32(output, adler32(raw));
    return output;
}

} // namespace

DebugCanvas::DebugCanvas(std::uint32_t width, std::uint32_t height, DebugColor background)
    : width_{width}, height_{height},
      pixels_(static_cast<std::size_t>(width) * height, background) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument{"除錯畫布尺寸必須大於零"};
    }
}

void DebugCanvas::fill_rect(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
                            DebugColor color) {
    const auto left = std::clamp(x, 0, static_cast<std::int32_t>(width_));
    const auto top = std::clamp(y, 0, static_cast<std::int32_t>(height_));
    const auto right = std::clamp(x + width, 0, static_cast<std::int32_t>(width_));
    const auto bottom = std::clamp(y + height, 0, static_cast<std::int32_t>(height_));
    for (auto py = top; py < bottom; ++py) {
        for (auto px = left; px < right; ++px) {
            pixels_[static_cast<std::size_t>(py) * width_ + static_cast<std::size_t>(px)] = color;
        }
    }
}

void DebugCanvas::draw_line(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
                            std::int32_t thickness, DebugColor color) {
    if (x0 == x1) {
        fill_rect(x0 - thickness / 2, std::min(y0, y1), thickness, std::abs(y1 - y0) + 1, color);
    } else if (y0 == y1) {
        fill_rect(std::min(x0, x1), y0 - thickness / 2, std::abs(x1 - x0) + 1, thickness, color);
    } else {
        throw std::invalid_argument{"除錯畫布目前只接受水平或垂直線"};
    }
}

void DebugCanvas::write_png(const std::filesystem::path& path) const {
    std::vector<std::uint8_t> raw;
    raw.reserve((static_cast<std::size_t>(width_) * 3U + 1U) * height_);
    for (std::uint32_t y = 0; y < height_; ++y) {
        raw.push_back(0);
        for (std::uint32_t x = 0; x < width_; ++x) {
            const auto color = pixels_[static_cast<std::size_t>(y) * width_ + x];
            raw.insert(raw.end(), {color.red, color.green, color.blue});
        }
    }

    std::vector<std::uint8_t> png{137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<std::uint8_t> header;
    append_u32(header, width_);
    append_u32(header, height_);
    header.insert(header.end(), {8, 2, 0, 0, 0});
    append_chunk(png, "IHDR", header);
    const auto compressed = deflate_uncompressed(raw);
    append_chunk(png, "IDAT", compressed);
    append_chunk(png, "IEND", {});

    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream{path, std::ios::binary};
    if (!stream.is_open()) {
        throw std::runtime_error{"無法建立 PNG：" + path.string()};
    }
    stream.write(reinterpret_cast<const char*>(png.data()),
                 static_cast<std::streamsize>(png.size()));
    if (!stream.good()) {
        throw std::runtime_error{"寫入 PNG 失敗：" + path.string()};
    }
}

} // namespace aetheria::sim
