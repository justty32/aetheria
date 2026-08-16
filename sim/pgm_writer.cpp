#include "sim/pgm_writer.h"

#include <fstream>
#include <stdexcept>

namespace aetheria::sim {

void write_pgm(const std::filesystem::path& path, std::uint32_t width, std::uint32_t height,
               const std::vector<std::uint8_t>& pixels) {
    if (pixels.size() != static_cast<std::size_t>(width) * height) {
        throw std::runtime_error{"PGM pixel 數與尺寸不符"};
    }
    std::ofstream stream{path, std::ios::binary};
    if (!stream.is_open()) {
        throw std::runtime_error{"無法建立 stage dump：" + path.string()};
    }
    stream << "P5\n" << width << ' ' << height << "\n255\n";
    stream.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!stream.good()) {
        throw std::runtime_error{"寫入 stage dump 失敗：" + path.string()};
    }
}

}  // namespace aetheria::sim
