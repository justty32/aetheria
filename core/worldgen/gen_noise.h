#pragma once

// gen_noise.h 收斂 Region 生成器內部共用的 splitmix 格點噪聲與 fbm。

#include "core/worldgen/region_seed.h"

#include <algorithm>
#include <cstdint>

namespace aetheria::worldgen::detail {

class SplitMix64Stream {
    public:
    explicit SplitMix64Stream(std::uint64_t seed) noexcept : state_{seed} {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ = splitmix64(state_);
        return state_;
    }

    [[nodiscard]] std::uint64_t bounded(std::uint64_t bound) noexcept {
        return bound == 0 ? 0 : next() % bound;
    }

    private:
    std::uint64_t state_;
};

[[nodiscard]] inline double lattice_noise(std::uint64_t seed, std::uint32_t x,
                                          std::uint32_t y) noexcept {
    const auto mixed =
        splitmix64(seed ^ (static_cast<std::uint64_t>(x) * UINT64_C(0x9E3779B185EBCA87)) ^
                   (static_cast<std::uint64_t>(y) * UINT64_C(0xC2B2AE3D27D4EB4F)));
    constexpr double denominator = static_cast<double>(UINT64_C(1) << 53);
    const auto unit = static_cast<double>(mixed >> 11U) / denominator;
    return unit * 2.0 - 1.0;
}

[[nodiscard]] inline double smooth(double value) noexcept {
    return value * value * (3.0 - 2.0 * value);
}

[[nodiscard]] inline double interpolate(double lhs, double rhs, double amount) noexcept {
    return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] inline double value_noise(std::uint64_t seed, std::uint32_t x, std::uint32_t y,
                                        std::uint32_t wavelength) noexcept {
    const auto grid_x = x / wavelength;
    const auto grid_y = y / wavelength;
    const auto fraction_x = smooth(static_cast<double>(x % wavelength) / wavelength);
    const auto fraction_y = smooth(static_cast<double>(y % wavelength) / wavelength);
    const auto north = interpolate(lattice_noise(seed, grid_x, grid_y),
                                   lattice_noise(seed, grid_x + 1U, grid_y), fraction_x);
    const auto south = interpolate(lattice_noise(seed, grid_x, grid_y + 1U),
                                   lattice_noise(seed, grid_x + 1U, grid_y + 1U), fraction_x);
    return interpolate(north, south, fraction_y);
}

[[nodiscard]] inline double fbm(std::uint64_t seed, std::uint32_t x, std::uint32_t y,
                                std::uint8_t octaves) noexcept {
    double result{};
    double amplitude = 760.0;
    std::uint32_t wavelength = 64;
    for (std::uint8_t octave = 0; octave < octaves; ++octave) {
        result += value_noise(splitmix64(seed ^ octave), x, y, wavelength) * amplitude;
        amplitude *= 0.5;
        wavelength = std::max(UINT32_C(1), wavelength / 2U);
    }
    return result;
}

}  // namespace aetheria::worldgen::detail
