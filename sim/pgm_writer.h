// PGM（P5）灰階影像輸出：worldgen 各階段的除錯 dump 用。
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace aetheria::sim {

void write_pgm(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
               const std::vector<std::uint8_t>& pixels);

}  // namespace aetheria::sim
