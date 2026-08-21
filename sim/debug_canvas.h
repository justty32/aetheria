#pragma once

// debug_canvas.h 提供 sim 檢視器用的 RGB 畫布與無外部依賴 PNG 輸出。

#include <cstdint>
#include <filesystem>
#include <vector>

namespace aetheria::sim {

struct DebugColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
};

class DebugCanvas {
  public:
    DebugCanvas(std::uint32_t width, std::uint32_t height, DebugColor background);

    void fill_rect(std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height,
                   DebugColor color);
    void draw_line(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1,
                   std::int32_t thickness, DebugColor color);
    void write_png(const std::filesystem::path& path) const;

  private:
    std::uint32_t width_{};
    std::uint32_t height_{};
    std::vector<DebugColor> pixels_;
};

} // namespace aetheria::sim
