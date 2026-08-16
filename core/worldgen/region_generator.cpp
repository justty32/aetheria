#include "core/worldgen/region_generator.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace aetheria::worldgen {
namespace {

inline constexpr std::uint64_t kRegionSalt = UINT64_C(0xA0761D6478BD642F);
inline constexpr std::uint64_t kPlateStageId = UINT64_C(0x504C415445000001);
inline constexpr std::uint64_t kHeightStageId = UINT64_C(0x4845494748540002);
inline constexpr std::uint64_t kErosionStageId = UINT64_C(0x45524F53494F4E03);
inline constexpr std::uint64_t kClimateStageId = UINT64_C(0x434C494D41544504);
inline constexpr std::uint64_t kRiverStageId = UINT64_C(0x5249564552530005);
inline constexpr std::uint64_t kBiomeStageId = UINT64_C(0x42494F4D45530006);
inline constexpr std::uint64_t kFeatureStageId = UINT64_C(0x4645415455524507);
inline constexpr double kMinWorldElevation = -4096.0;
inline constexpr double kMaxWorldElevation = 61439.0;

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

[[nodiscard]] std::size_t checked_count(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > UINT16_MAX || height > UINT16_MAX) {
        throw std::invalid_argument{"Region 生成尺寸必須落在 1..65535"};
    }
    const auto count64 = static_cast<std::uint64_t>(width) * height;
    if (count64 > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument{"Region 生成尺寸超出可表達範圍"};
    }
    return static_cast<std::size_t>(count64);
}

[[nodiscard]] std::array<std::size_t, 4> neighbors(std::size_t index, std::uint32_t width,
                                                   std::uint32_t height) noexcept {
    const auto x = index % width;
    const auto y = index / width;
    constexpr auto missing = std::numeric_limits<std::size_t>::max();
    return {y > 0 ? index - width : missing, x + 1 < width ? index + 1 : missing,
            y + 1 < height ? index + width : missing, x > 0 ? index - 1 : missing};
}

struct BoundaryInteraction {
    PlateBoundaryType type;
    std::int16_t effect;
};

[[nodiscard]] BoundaryInteraction boundary_interaction(const Plate& lhs,
                                                       const Plate& rhs) noexcept {
    const auto separation_x = static_cast<std::int32_t>(rhs.x) - lhs.x;
    const auto separation_y = static_cast<std::int32_t>(rhs.y) - lhs.y;
    const auto relative_x = static_cast<std::int32_t>(rhs.drift_x) - lhs.drift_x;
    const auto relative_y = static_cast<std::int32_t>(rhs.drift_y) - lhs.drift_y;
    const auto dot = relative_x * separation_x + relative_y * separation_y;
    if (dot < -8) {
        return {PlateBoundaryType::Convergent, 1200};
    }
    if (dot > 8) {
        return {PlateBoundaryType::Divergent,
                static_cast<std::int16_t>(lhs.is_oceanic && rhs.is_oceanic ? 320 : -650)};
    }
    return {PlateBoundaryType::Transform, 0};
}

void write_stronger(PlateBoundaryType& target_type, std::int16_t& target_effect,
                    BoundaryInteraction candidate) noexcept {
    if (target_type == PlateBoundaryType::None ||
        std::abs(static_cast<int>(candidate.effect)) > std::abs(static_cast<int>(target_effect))) {
        target_type = candidate.type;
        target_effect = candidate.effect;
    }
}

[[nodiscard]] double lattice_noise(std::uint64_t seed, std::uint32_t x, std::uint32_t y) noexcept {
    const auto mixed =
        splitmix64(seed ^ (static_cast<std::uint64_t>(x) * UINT64_C(0x9E3779B185EBCA87)) ^
                   (static_cast<std::uint64_t>(y) * UINT64_C(0xC2B2AE3D27D4EB4F)));
    constexpr double denominator = static_cast<double>(UINT64_C(1) << 53);
    const auto unit = static_cast<double>(mixed >> 11U) / denominator;
    return unit * 2.0 - 1.0;
}

[[nodiscard]] double smooth(double value) noexcept { return value * value * (3.0 - 2.0 * value); }

[[nodiscard]] double interpolate(double lhs, double rhs, double amount) noexcept {
    return lhs + (rhs - lhs) * amount;
}

[[nodiscard]] double value_noise(std::uint64_t seed, std::uint32_t x, std::uint32_t y,
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

[[nodiscard]] double fbm(std::uint64_t seed, std::uint32_t x, std::uint32_t y,
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

[[nodiscard]] std::vector<std::size_t> largest_land_component(const std::vector<std::uint8_t>& land,
                                                              std::uint32_t width,
                                                              std::uint32_t height) {
    std::vector<std::uint8_t> visited(land.size());
    std::vector<std::size_t> largest;
    std::queue<std::size_t> pending;
    for (std::size_t start = 0; start < land.size(); ++start) {
        if (land[start] == 0 || visited[start] != 0) {
            continue;
        }
        std::vector<std::size_t> component;
        visited[start] = 1;
        pending.push(start);
        while (!pending.empty()) {
            const auto current = pending.front();
            pending.pop();
            component.push_back(current);
            for (const auto neighbor : neighbors(current, width, height)) {
                if (neighbor < land.size() && land[neighbor] != 0 && visited[neighbor] == 0) {
                    visited[neighbor] = 1;
                    pending.push(neighbor);
                }
            }
        }
        if (component.size() > largest.size()) {
            largest = std::move(component);
        }
    }
    return largest;
}

[[nodiscard]] std::vector<std::uint8_t>
repair_land_connectivity(const std::vector<double>& elevation,
                         const std::vector<std::uint8_t>& initial_land, std::uint32_t width,
                         std::uint32_t height, std::size_t target_count) {
    auto seed_component = largest_land_component(initial_land, width, height);
    if (seed_component.empty()) {
        seed_component.push_back(static_cast<std::size_t>(std::distance(
            elevation.begin(), std::max_element(elevation.begin(), elevation.end()))));
    }

    const auto best_seed = *std::max_element(
        seed_component.begin(), seed_component.end(),
        [&](std::size_t lhs, std::size_t rhs) { return elevation[lhs] < elevation[rhs]; });
    if (seed_component.size() > target_count) {
        seed_component.assign(1, best_seed);
    }

    std::vector<std::uint8_t> selected(elevation.size());
    std::vector<std::uint8_t> queued(elevation.size());
    using Candidate = std::pair<double, std::size_t>;
    std::priority_queue<Candidate> frontier;
    for (const auto index : seed_component) {
        selected[index] = 1;
    }
    auto queue_neighbors = [&](std::size_t index) {
        for (const auto neighbor : neighbors(index, width, height)) {
            if (neighbor < elevation.size() && selected[neighbor] == 0 && queued[neighbor] == 0) {
                queued[neighbor] = 1;
                frontier.emplace(elevation[neighbor], neighbor);
            }
        }
    };
    for (const auto index : seed_component) {
        queue_neighbors(index);
    }

    auto selected_count = seed_component.size();
    while (selected_count < target_count && !frontier.empty()) {
        const auto index = frontier.top().second;
        frontier.pop();
        if (selected[index] != 0) {
            continue;
        }
        selected[index] = 1;
        ++selected_count;
        queue_neighbors(index);
    }
    return selected;
}

[[nodiscard]] std::uint16_t quantize_meter(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error{"高度場含非有限浮點值"};
    }
    const auto clamped = std::clamp(value, kMinWorldElevation, kMaxWorldElevation);
    return static_cast<std::uint16_t>(std::llround(clamped) -
                                      static_cast<long long>(kMinWorldElevation));
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Value, bool = std::is_enum_v<Value>> struct HashBits {
    using Type = Value;
};

template <typename Value> struct HashBits<Value, true> {
    using Type = std::underlying_type_t<Value>;
};

template <typename Value> void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>);
    using Bits = typename HashBits<Value>::Type;
    using Unsigned = std::make_unsigned_t<Bits>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

void hash_double(std::uint64_t& hash, double value) noexcept {
    hash_scalar(hash, std::bit_cast<std::uint64_t>(value));
}

template <typename Value>
void hash_vector(std::uint64_t& hash, const std::vector<Value>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_scalar(hash, value);
    }
}

void hash_double_vector(std::uint64_t& hash, const std::vector<double>& values) noexcept {
    hash_scalar(hash, static_cast<std::uint64_t>(values.size()));
    for (const auto value : values) {
        hash_double(hash, value);
    }
}

[[nodiscard]] std::vector<std::uint8_t> grayscale_values(const std::vector<double>& values) {
    if (values.empty()) {
        return {};
    }
    const auto [minimum, maximum] = std::minmax_element(values.begin(), values.end());
    const auto span = *maximum - *minimum;
    std::vector<std::uint8_t> pixels;
    pixels.reserve(values.size());
    for (const auto value : values) {
        const auto normalized = span == 0.0 ? 0.0 : (value - *minimum) / span;
        pixels.push_back(static_cast<std::uint8_t>(std::llround(normalized * 255.0)));
    }
    return pixels;
}

template <typename Id, typename Finder>
[[nodiscard]] Id require_definition(Finder&& finder, std::string id) {
    const auto found = finder(id);
    if (!found.has_value()) {
        throw std::invalid_argument{"Region 生成缺少必要 definition：" + id};
    }
    return *found;
}

}  // namespace

std::uint64_t splitmix64(std::uint64_t value) noexcept {
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30U)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27U)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31U);
}

std::uint64_t derive_stage_seed(std::uint64_t world_seed, std::uint64_t stage_id) noexcept {
    return splitmix64(world_seed ^ stage_id);
}

std::uint64_t derive_region_seed(std::uint64_t world_seed, std::uint32_t region_id) noexcept {
    return splitmix64(world_seed ^ kRegionSalt ^ region_id);
}

std::uint64_t derive_region_stage_seed(std::uint64_t world_seed, std::uint32_t region_id,
                                       std::uint64_t stage_id) noexcept {
    return splitmix64(derive_stage_seed(world_seed, stage_id) ^
                      derive_region_seed(world_seed, region_id));
}

GenerationParameterHashes
generation_parameter_hashes(const RegionGenerationConfig& config) noexcept {
    GenerationParameterHashes result;
    auto begin_group = [] { return UINT64_C(14695981039346656037); };

    result.groups[0] = begin_group();
    hash_scalar(result.groups[0], config.plates.min_count);
    hash_scalar(result.groups[0], config.plates.max_count);
    result.groups[1] = begin_group();
    hash_scalar(result.groups[1], config.height.noise_octaves);
    hash_scalar(result.groups[1], config.height.target_land_percent);
    result.groups[2] = begin_group();
    hash_scalar(result.groups[2], config.erosion.iterations);
    hash_double(result.groups[2], config.erosion.talus);
    hash_double(result.groups[2], config.erosion.transfer_fraction);
    result.groups[3] = begin_group();
    hash_scalar(result.groups[3], config.climate.lapse_tenths_per_km);
    hash_scalar(result.groups[3], config.climate.air_decay);
    hash_scalar(result.groups[3], config.climate.uplift_rain);
    result.groups[4] = begin_group();
    hash_scalar(result.groups[4], config.rivers.stream_threshold);
    hash_scalar(result.groups[4], config.rivers.river_threshold);
    hash_scalar(result.groups[4], config.rivers.great_river_threshold);
    hash_scalar(result.groups[4], config.rivers.moisture_bonus);
    result.groups[5] = begin_group();
    hash_scalar(result.groups[5], config.biome.temperature_bias_tenths);
    hash_scalar(result.groups[5], config.biome.moisture_bias);
    result.groups[6] = begin_group();
    hash_scalar(result.groups[6], config.features.forest_density_scale);
    hash_scalar(result.groups[6], config.features.mine_chance);
    hash_scalar(result.groups[6], config.features.oasis_chance);
    hash_scalar(result.groups[6], config.features.landmark_chance);
    return result;
}

PlateStageOutput generate_plates(const RegionSlowVariables& slow, std::uint64_t stage_seed,
                                 const PlateGenerationConfig& config) {
    const auto count = checked_count(slow.width, slow.height);
    if (config.min_count < 8 || config.max_count > 16 || config.min_count > config.max_count) {
        throw std::invalid_argument{"板塊數範圍必須落在 8..16"};
    }

    PlateStageOutput output{slow.width, slow.height, {}, {}, {}, {}};
    SplitMix64Stream random{stage_seed};
    const auto plate_count = static_cast<std::size_t>(
        config.min_count + random.bounded(config.max_count - config.min_count + 1U));
    output.plates.reserve(plate_count);
    for (std::size_t index = 0; index < plate_count; ++index) {
        const auto oceanic = random.bounded(100) < 48;
        auto drift_x = static_cast<std::int8_t>(static_cast<int>(random.bounded(9)) - 4);
        auto drift_y = static_cast<std::int8_t>(static_cast<int>(random.bounded(9)) - 4);
        if (drift_x == 0 && drift_y == 0) {
            drift_x = 1;
        }
        const auto base = oceanic ? -1500 + static_cast<int>(random.bounded(900))
                                  : 250 + static_cast<int>(random.bounded(1350));
        output.plates.push_back({static_cast<std::uint16_t>(random.bounded(slow.width)),
                                 static_cast<std::uint16_t>(random.bounded(slow.height)), oceanic,
                                 drift_x, drift_y, static_cast<std::int16_t>(base)});
    }

    output.plate_index.resize(count);
    for (std::uint32_t y = 0; y < slow.height; ++y) {
        for (std::uint32_t x = 0; x < slow.width; ++x) {
            std::uint64_t closest_distance = std::numeric_limits<std::uint64_t>::max();
            std::uint8_t closest{};
            for (std::size_t plate_index = 0; plate_index < output.plates.size(); ++plate_index) {
                const auto dx = static_cast<std::int64_t>(x) - output.plates[plate_index].x;
                const auto dy = static_cast<std::int64_t>(y) - output.plates[plate_index].y;
                const auto distance = static_cast<std::uint64_t>(dx * dx + dy * dy);
                if (distance < closest_distance) {
                    closest_distance = distance;
                    closest = static_cast<std::uint8_t>(plate_index);
                }
            }
            output.plate_index[static_cast<std::size_t>(y) * slow.width + x] = closest;
        }
    }

    output.boundary_type.assign(count, PlateBoundaryType::None);
    output.boundary_effect.assign(count, 0);
    for (std::size_t index = 0; index < count; ++index) {
        for (const auto neighbor : neighbors(index, slow.width, slow.height)) {
            if (neighbor >= count || output.plate_index[index] == output.plate_index[neighbor]) {
                continue;
            }
            const auto effect = boundary_interaction(output.plates[output.plate_index[index]],
                                                     output.plates[output.plate_index[neighbor]]);
            write_stronger(output.boundary_type[index], output.boundary_effect[index], effect);
            write_stronger(output.boundary_type[neighbor], output.boundary_effect[neighbor],
                           effect);
        }
    }

    std::queue<std::size_t> frontier;
    std::vector<std::uint8_t> distance(count, UINT8_MAX);
    for (std::size_t index = 0; index < count; ++index) {
        if (output.boundary_effect[index] != 0) {
            distance[index] = 0;
            frontier.push(index);
        }
    }
    while (!frontier.empty()) {
        const auto current = frontier.front();
        frontier.pop();
        if (distance[current] >= 8) {
            continue;
        }
        for (const auto neighbor : neighbors(current, slow.width, slow.height)) {
            if (neighbor >= count || distance[neighbor] != UINT8_MAX) {
                continue;
            }
            distance[neighbor] = static_cast<std::uint8_t>(distance[current] + 1U);
            output.boundary_effect[neighbor] =
                static_cast<std::int16_t>((output.boundary_effect[current] * 3) / 4);
            frontier.push(neighbor);
        }
    }
    return output;
}

HeightStageOutput generate_height(const PlateStageOutput& plates, std::uint64_t stage_seed,
                                  const HeightGenerationConfig& config) {
    const auto count = checked_count(plates.width, plates.height);
    if (plates.plate_index.size() != count || plates.boundary_type.size() != count ||
        plates.boundary_effect.size() != count || plates.plates.empty()) {
        throw std::invalid_argument{"板塊階段輸出尺寸不一致"};
    }
    if (config.noise_octaves == 0 || config.noise_octaves > 8 || config.target_land_percent == 0 ||
        config.target_land_percent >= 100) {
        throw std::invalid_argument{"高度場參數超出範圍"};
    }

    HeightStageOutput output{plates.width, plates.height, {}, {}, 0.0};
    output.elevation.resize(count);
    for (std::uint32_t y = 0; y < plates.height; ++y) {
        for (std::uint32_t x = 0; x < plates.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * plates.width + x;
            const auto plate_index = plates.plate_index[index];
            if (plate_index >= plates.plates.size()) {
                throw std::invalid_argument{"板塊階段輸出含無效 plate index"};
            }
            output.elevation[index] = plates.plates[plate_index].base_elevation +
                                      plates.boundary_effect[index] +
                                      fbm(stage_seed, x, y, config.noise_octaves);
        }
    }

    const auto [minimum, maximum] =
        std::minmax_element(output.elevation.begin(), output.elevation.end());
    auto low = *minimum - 1.0;
    auto high = *maximum + 1.0;
    const auto target_count = std::max<std::size_t>(
        1, count * static_cast<std::size_t>(config.target_land_percent) / 100U);
    for (std::uint8_t iteration = 0; iteration < 48; ++iteration) {
        const auto middle = (low + high) * 0.5;
        const auto land_count =
            static_cast<std::size_t>(std::count_if(output.elevation.begin(), output.elevation.end(),
                                                   [&](double value) { return value >= middle; }));
        if (land_count > target_count) {
            low = middle;
        } else {
            high = middle;
        }
    }
    output.sea_level = (low + high) * 0.5;
    std::vector<std::uint8_t> initial_land(count);
    for (std::size_t index = 0; index < count; ++index) {
        initial_land[index] = output.elevation[index] >= output.sea_level ? UINT8_C(1) : UINT8_C(0);
    }
    output.land = repair_land_connectivity(output.elevation, initial_land, plates.width,
                                           plates.height, target_count);
    for (std::size_t index = 0; index < count; ++index) {
        if (output.land[index] != 0) {
            output.elevation[index] = std::max(output.elevation[index], output.sea_level + 1.0);
        } else {
            output.elevation[index] = std::min(output.elevation[index], output.sea_level - 1.0);
        }
    }
    return output;
}

ErosionStageOutput erode_height(const HeightStageOutput& height, std::uint64_t stage_seed,
                                const ErosionGenerationConfig& config) {
    const auto count = checked_count(height.width, height.height);
    if (height.elevation.size() != count || height.land.size() != count) {
        throw std::invalid_argument{"高度場階段輸出尺寸不一致"};
    }
    if (!std::isfinite(config.talus) || config.talus < 0.0 ||
        !std::isfinite(config.transfer_fraction) || config.transfer_fraction <= 0.0 ||
        config.transfer_fraction > 0.5) {
        throw std::invalid_argument{"侵蝕參數超出範圍"};
    }

    ErosionStageOutput output{height.width, height.height, height.elevation, height.land,
                              height.sea_level};
    std::vector<double> delta(count);
    for (std::uint16_t iteration = 0; iteration < config.iterations; ++iteration) {
        std::fill(delta.begin(), delta.end(), 0.0);
        for (std::size_t index = 0; index < count; ++index) {
            const auto adjacent = neighbors(index, height.width, height.height);
            const auto rotation = static_cast<std::size_t>(
                splitmix64(stage_seed ^ index ^ iteration) % adjacent.size());
            auto lowest = index;
            for (std::size_t offset = 0; offset < adjacent.size(); ++offset) {
                const auto neighbor = adjacent[(rotation + offset) % adjacent.size()];
                if (neighbor < count && output.elevation[neighbor] < output.elevation[lowest]) {
                    lowest = neighbor;
                }
            }
            const auto difference = output.elevation[index] - output.elevation[lowest];
            if (lowest != index && difference > config.talus) {
                const auto transfer = (difference - config.talus) * config.transfer_fraction;
                delta[index] -= transfer;
                delta[lowest] += transfer;
            }
        }
        for (std::size_t index = 0; index < count; ++index) {
            output.elevation[index] += delta[index];
        }
    }
    return output;
}

QuantizedElevation quantize_elevation(const ErosionStageOutput& erosion) {
    const auto count = checked_count(erosion.width, erosion.height);
    if (erosion.elevation.size() != count || erosion.land.size() != count ||
        !std::isfinite(erosion.sea_level)) {
        throw std::invalid_argument{"侵蝕階段輸出尺寸或海平面無效"};
    }

    QuantizedElevation output{erosion.width, erosion.height, {}, {}, 0};
    output.sea_level = quantize_meter(erosion.sea_level);
    output.land = erosion.land;
    output.meters.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto adjusted = erosion.land[index] != 0
                                  ? std::max(erosion.elevation[index], erosion.sea_level + 1.0)
                                  : std::min(erosion.elevation[index], erosion.sea_level - 1.0);
        output.meters.push_back(quantize_meter(adjusted));
    }
    return output;
}

ClimateStageOutput generate_climate(const RegionSlowVariables& slow,
                                    const QuantizedElevation& elevation, std::uint64_t stage_seed,
                                    const ClimateGenerationConfig& config) {
    const auto count = checked_count(elevation.width, elevation.height);
    if (slow.width != elevation.width || slow.height != elevation.height ||
        elevation.meters.size() != count || elevation.land.size() != count ||
        slow.latitude_degrees < -90 || slow.latitude_degrees > 90) {
        throw std::invalid_argument{"氣候階段輸入尺寸或緯度無效"};
    }
    constexpr std::array<std::int16_t, 7> latitude_temperature{300, 280, 230, 160, 70, -50, -180};
    ClimateStageOutput output{elevation.width, elevation.height, {}, {}, {}};
    output.temperature_tenths.resize(count);
    output.moisture.resize(count);
    output.prevailing_wind_x.resize(elevation.height);

    for (std::uint32_t y = 0; y < elevation.height; ++y) {
        const auto row_offset =
            elevation.height == 1 ? 0 : static_cast<int>(y * 20U / (elevation.height - 1U)) - 10;
        const auto latitude =
            std::clamp(static_cast<int>(slow.latitude_degrees) + row_offset, -90, 90);
        const auto absolute_latitude = std::abs(latitude);
        const auto band = static_cast<std::size_t>(absolute_latitude / 15);
        const auto remainder = absolute_latitude % 15;
        const auto next_band = std::min(band + 1U, latitude_temperature.size() - 1U);
        const auto base_temperature =
            static_cast<std::int32_t>((latitude_temperature[band] * (15 - remainder) +
                                       latitude_temperature[next_band] * remainder) /
                                      15);
        const auto wind = absolute_latitude < 30   ? INT8_C(-1)
                          : absolute_latitude < 60 ? INT8_C(1)
                                                   : INT8_C(-1);
        output.prevailing_wind_x[y] = wind;
        auto air =
            static_cast<std::uint32_t>(30000U + splitmix64(stage_seed ^ y) % UINT64_C(20001));
        auto previous_elevation = static_cast<std::int32_t>(elevation.sea_level);
        for (std::uint32_t step = 0; step < elevation.width; ++step) {
            const auto x = wind > 0 ? step : elevation.width - 1U - step;
            const auto index = static_cast<std::size_t>(y) * elevation.width + x;
            const auto encoded_elevation = static_cast<std::int32_t>(elevation.meters[index]);
            const auto meters_above_zero = std::max(0, encoded_elevation - 4096);
            const auto lapse = meters_above_zero * config.lapse_tenths_per_km / 1000;
            output.temperature_tenths[index] = static_cast<std::int16_t>(
                std::clamp(base_temperature - static_cast<std::int32_t>(lapse),
                           static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
                           static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
            if (elevation.land[index] == 0) {
                air = UINT16_MAX;
                output.moisture[index] = UINT16_MAX;
            } else {
                const auto rise = std::max(0, encoded_elevation - previous_elevation);
                const auto rain = std::min<std::uint32_t>(
                    air, 900U + static_cast<std::uint32_t>(rise) * config.uplift_rain);
                output.moisture[index] = static_cast<std::uint16_t>(rain);
                const auto spent = std::min<std::uint32_t>(air, rain + config.air_decay);
                air -= spent;
            }
            previous_elevation = encoded_elevation;
        }
    }
    return output;
}

RiverStageOutput generate_rivers(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, std::uint64_t stage_seed,
                                 const RiverGenerationConfig& config) {
    const auto count = checked_count(elevation.width, elevation.height);
    if (elevation.meters.size() != count || elevation.land.size() != count ||
        climate.width != elevation.width || climate.height != elevation.height ||
        climate.moisture.size() != count || config.stream_threshold == 0 ||
        config.stream_threshold > config.river_threshold ||
        config.river_threshold > config.great_river_threshold) {
        throw std::invalid_argument{"河流階段輸入尺寸或門檻無效"};
    }

    RiverStageOutput output{
        elevation.width, elevation.height, elevation.meters, {}, {}, {}, climate.moisture, {}};
    output.downstream.assign(count, -1);
    output.flow.resize(count);
    output.river_class.assign(count, 0);
    output.lake.assign(count, 0);
    std::vector<std::int32_t> bucket_head(UINT16_MAX + 1U, -1);
    std::vector<std::int32_t> bucket_next(count, -1);
    std::vector<std::uint8_t> visited(count);
    std::vector<std::size_t> order;
    order.reserve(count);

    auto push = [&](std::size_t index, std::uint16_t level) {
        bucket_next[index] = bucket_head[level];
        bucket_head[level] = static_cast<std::int32_t>(index);
        visited[index] = 1;
    };
    for (std::uint32_t y = 0; y < elevation.height; ++y) {
        for (std::uint32_t x = 0; x < elevation.width; ++x) {
            const auto index = static_cast<std::size_t>(y) * elevation.width + x;
            const bool boundary =
                x == 0 || y == 0 || x + 1U == elevation.width || y + 1U == elevation.height;
            if (elevation.land[index] == 0 || boundary) {
                push(index, output.filled_elevation[index]);
                output.lake[index] = elevation.land[index] != 0 ? UINT8_C(1) : UINT8_C(0);
            }
        }
    }

    std::size_t processed{};
    std::uint32_t level{};
    while (processed < count) {
        while (level <= UINT16_MAX && bucket_head[level] < 0) {
            ++level;
        }
        if (level > UINT16_MAX) {
            throw std::runtime_error{"priority-flood 未能覆蓋全圖"};
        }
        const auto index = static_cast<std::size_t>(bucket_head[level]);
        bucket_head[level] = bucket_next[index];
        order.push_back(index);
        ++processed;
        for (const auto neighbor : neighbors(index, elevation.width, elevation.height)) {
            if (neighbor >= count || visited[neighbor] != 0) {
                continue;
            }
            output.filled_elevation[neighbor] = std::max<std::uint16_t>(
                output.filled_elevation[neighbor], static_cast<std::uint16_t>(level));
            output.downstream[neighbor] = static_cast<std::int32_t>(index);
            push(neighbor, output.filled_elevation[neighbor]);
        }
    }

    // Priority-flood 的 parent 已替平地提供無環出口；若存在更低的填平後鄰格，
    // 則明確指向其中最低者。嚴格下降邊不可能成環，等高邊仍沿 flood 樹。
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0 || output.downstream[index] < 0) {
            continue;
        }
        const auto adjacent = neighbors(index, elevation.width, elevation.height);
        const auto rotation = static_cast<std::size_t>(
            splitmix64(stage_seed ^ static_cast<std::uint64_t>(index)) % adjacent.size());
        auto lowest = static_cast<std::size_t>(output.downstream[index]);
        for (std::size_t offset = 0; offset < adjacent.size(); ++offset) {
            const auto neighbor = adjacent[(rotation + offset) % adjacent.size()];
            if (neighbor < count &&
                output.filled_elevation[neighbor] < output.filled_elevation[lowest]) {
                lowest = neighbor;
            }
        }
        if (output.filled_elevation[lowest] < output.filled_elevation[index]) {
            output.downstream[index] = static_cast<std::int32_t>(lowest);
        }
    }

    for (std::size_t index = 0; index < count; ++index) {
        output.flow[index] = 1U + climate.moisture[index];
    }
    for (auto iterator = order.rbegin(); iterator != order.rend(); ++iterator) {
        const auto index = *iterator;
        const auto downstream = output.downstream[index];
        if (downstream >= 0) {
            auto& target = output.flow[static_cast<std::size_t>(downstream)];
            target =
                target > UINT32_MAX - output.flow[index] ? UINT32_MAX : target + output.flow[index];
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0 || output.downstream[index] < 0) {
            continue;
        }
        output.river_class[index] = output.flow[index] >= config.great_river_threshold ? UINT8_C(3)
                                    : output.flow[index] >= config.river_threshold     ? UINT8_C(2)
                                    : output.flow[index] >= config.stream_threshold    ? UINT8_C(1)
                                                                                       : UINT8_C(0);
        if (output.river_class[index] != 0) {
            for (const auto neighbor : neighbors(index, elevation.width, elevation.height)) {
                if (neighbor < count) {
                    output.moisture[neighbor] = static_cast<std::uint16_t>(std::min<std::uint32_t>(
                        UINT16_MAX, output.moisture[neighbor] + config.moisture_bonus));
                }
            }
        }
    }
    return output;
}

BiomeStageOutput generate_biomes(const QuantizedElevation& elevation,
                                 const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                                 const rules::Ruleset& ruleset,
                                 const RegionDefinitionIds& definitions, std::uint64_t stage_seed,
                                 const BiomeGenerationConfig& config) {
    static_cast<void>(stage_seed);
    const auto count = checked_count(elevation.width, elevation.height);
    if (elevation.meters.size() != count || elevation.land.size() != count ||
        climate.width != elevation.width || climate.height != elevation.height ||
        climate.temperature_tenths.size() != count || rivers.width != elevation.width ||
        rivers.height != elevation.height || rivers.moisture.size() != count ||
        ruleset.biome_rules().empty()) {
        throw std::invalid_argument{"biome 階段輸入尺寸不一致或缺少 biomes.toml 規則"};
    }
    BiomeStageOutput output{elevation.width, elevation.height, {}, {}};
    output.terrain.resize(count);
    output.relief.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            output.terrain[index] = definitions.ocean;
            output.relief[index] = definitions.plain;
            continue;
        }
        const auto x = static_cast<std::uint32_t>(index % elevation.width);
        const auto y = static_cast<std::uint32_t>(index / elevation.width);
        auto minimum = elevation.meters[index];
        auto maximum = elevation.meters[index];
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const auto nx = static_cast<int>(x) + dx;
                const auto ny = static_cast<int>(y) + dy;
                if (nx < 0 || ny < 0 || nx >= static_cast<int>(elevation.width) ||
                    ny >= static_cast<int>(elevation.height)) {
                    continue;
                }
                const auto neighbor =
                    static_cast<std::size_t>(ny) * elevation.width + static_cast<std::size_t>(nx);
                minimum = std::min(minimum, elevation.meters[neighbor]);
                maximum = std::max(maximum, elevation.meters[neighbor]);
            }
        }
        const auto ruggedness = static_cast<std::uint16_t>(maximum - minimum);
        const auto temperature = static_cast<std::int16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(climate.temperature_tenths[index]) +
                config.temperature_bias_tenths,
            std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
        const auto moisture = static_cast<std::uint16_t>(std::clamp<std::int32_t>(
            static_cast<std::int32_t>(rivers.moisture[index]) + config.moisture_bias, 0,
            UINT16_MAX));
        bool matched{};
        for (const auto& rule : ruleset.biome_rules()) {
            if (rule.fallback ||
                (temperature >= rule.min_temperature_tenths &&
                 temperature <= rule.max_temperature_tenths && moisture >= rule.min_moisture &&
                 moisture <= rule.max_moisture && elevation.meters[index] >= rule.min_elevation &&
                 elevation.meters[index] <= rule.max_elevation &&
                 ruggedness >= rule.min_ruggedness && ruggedness <= rule.max_ruggedness)) {
                output.terrain[index] = rule.terrain;
                output.relief[index] = rule.relief;
                matched = true;
                break;
            }
        }
        if (!matched) {
            throw std::runtime_error{"BiomeRule 沒有 fallback 命中"};
        }
    }
    return output;
}

FeatureStageOutput
generate_features(const PlateStageOutput& plates, const QuantizedElevation& elevation,
                  const ClimateStageOutput& climate, const RiverStageOutput& rivers,
                  const BiomeStageOutput& biome, const RegionDefinitionIds& definitions,
                  std::uint64_t stage_seed, const FeatureGenerationConfig& config) {
    const auto count = checked_count(elevation.width, elevation.height);
    if (plates.width != elevation.width || plates.height != elevation.height ||
        plates.boundary_effect.size() != count || climate.temperature_tenths.size() != count ||
        rivers.moisture.size() != count || biome.terrain.size() != count ||
        biome.relief.size() != count) {
        throw std::invalid_argument{"地物階段輸入尺寸不一致"};
    }
    FeatureStageOutput output{elevation.width, elevation.height, {}};
    output.feature.assign(count, definitions.no_feature);
    auto priority = [&](std::size_t index) {
        return splitmix64(stage_seed ^
                          (static_cast<std::uint64_t>(index) * UINT64_C(0xD6E8FEB86659FD93)));
    };
    for (std::size_t index = 0; index < count; ++index) {
        if (elevation.land[index] == 0) {
            continue;
        }
        const auto random = priority(index);
        if (static_cast<std::uint16_t>(random) < config.landmark_chance) {
            output.feature[index] = definitions.landmark;
            continue;
        }
        if (rivers.moisture[index] < 12500 &&
            static_cast<std::uint16_t>(random >> 16U) < config.oasis_chance) {
            output.feature[index] = definitions.oasis;
            continue;
        }
        if (std::abs(static_cast<int>(plates.boundary_effect[index])) >= 300 &&
            biome.relief[index] != definitions.plain &&
            static_cast<std::uint16_t>(random >> 32U) < config.mine_chance) {
            output.feature[index] = definitions.mine;
            continue;
        }
        if (climate.temperature_tenths[index] <= 40 || rivers.moisture[index] <= 12000) {
            continue;
        }
        const auto forest_chance =
            static_cast<std::uint16_t>(static_cast<std::uint32_t>(rivers.moisture[index]) *
                                       config.forest_density_scale / UINT16_MAX);
        if (static_cast<std::uint16_t>(random >> 48U) >= forest_chance) {
            continue;
        }
        bool local_priority = true;
        for (const auto neighbor : neighbors(index, elevation.width, elevation.height)) {
            if (neighbor < count && priority(neighbor) > random) {
                local_priority = false;
                break;
            }
        }
        if (local_priority) {
            output.feature[index] = definitions.forest;
        }
    }
    return output;
}

RegionBuildResult build_skeleton(const RegionSlowVariables& slow, std::uint64_t world_seed,
                                 const rules::Ruleset& ruleset,
                                 const RegionGenerationConfig& config) {
    RegionDefinitionIds definitions{
        require_definition<rules::TerrainId>(
            [&](const std::string& id) { return ruleset.find_terrain(id); }, "terrain.grassland"),
        require_definition<rules::TerrainId>(
            [&](const std::string& id) { return ruleset.find_terrain(id); }, "terrain.ocean"),
        require_definition<rules::ReliefId>(
            [&](const std::string& id) { return ruleset.find_relief(id); }, "relief.plain"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.none"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.forest"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.mine"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.oasis"),
        require_definition<rules::FeatureId>(
            [&](const std::string& id) { return ruleset.find_feature(id); }, "feature.landmark"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.none"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.stream"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.river"),
        require_definition<rules::EdgeId>(
            [&](const std::string& id) { return ruleset.find_edge(id); }, "edge.great_river")};

    auto plates = generate_plates(
        slow, derive_region_stage_seed(world_seed, slow.region_id, kPlateStageId), config.plates);
    auto height = generate_height(
        plates, derive_region_stage_seed(world_seed, slow.region_id, kHeightStageId),
        config.height);
    auto erosion =
        erode_height(height, derive_region_stage_seed(world_seed, slow.region_id, kErosionStageId),
                     config.erosion);
    auto elevation = quantize_elevation(erosion);
    auto climate = generate_climate(
        slow, elevation, derive_region_stage_seed(world_seed, slow.region_id, kClimateStageId),
        config.climate);
    auto rivers = generate_rivers(
        elevation, climate, derive_region_stage_seed(world_seed, slow.region_id, kRiverStageId),
        config.rivers);
    auto biome = generate_biomes(
        elevation, climate, rivers, ruleset, definitions,
        derive_region_stage_seed(world_seed, slow.region_id, kBiomeStageId), config.biome);
    auto features = generate_features(
        plates, elevation, climate, rivers, biome, definitions,
        derive_region_stage_seed(world_seed, slow.region_id, kFeatureStageId), config.features);
    RegionSkeleton skeleton{elevation, climate, rivers, biome, features, definitions};
    return {std::move(plates), std::move(height), std::move(erosion),  std::move(climate),
            std::move(rivers), std::move(biome),  std::move(features), std::move(skeleton)};
}

world::RegionTiles populate(const RegionSkeleton& skeleton, const RegionFastVariables& fast) {
    static_cast<void>(fast);
    const auto count = checked_count(skeleton.elevation.width, skeleton.elevation.height);
    if (skeleton.elevation.meters.size() != count || skeleton.elevation.land.size() != count ||
        skeleton.climate.temperature_tenths.size() != count ||
        skeleton.rivers.moisture.size() != count || skeleton.rivers.downstream.size() != count ||
        skeleton.rivers.river_class.size() != count || skeleton.biome.terrain.size() != count ||
        skeleton.biome.relief.size() != count || skeleton.features.feature.size() != count ||
        skeleton.elevation.width > static_cast<std::uint32_t>(INT16_MAX) ||
        skeleton.elevation.height > static_cast<std::uint32_t>(INT16_MAX)) {
        throw std::invalid_argument{"RegionSkeleton 尺寸不一致"};
    }
    world::RegionTiles tiles{skeleton.elevation.width, skeleton.elevation.height};
    tiles.elevation = skeleton.elevation.meters;
    for (std::size_t index = 0; index < count; ++index) {
        tiles.base[index] = skeleton.biome.terrain[index];
        tiles.relief[index] = skeleton.biome.relief[index];
        tiles.feature[index] = skeleton.features.feature[index];
        tiles.temperature[index] = static_cast<std::uint8_t>(std::clamp<std::int32_t>(
            (static_cast<std::int32_t>(skeleton.climate.temperature_tenths[index]) + 500) * 255 /
                1000,
            0, UINT8_MAX));
        tiles.moisture[index] = static_cast<std::uint8_t>(skeleton.rivers.moisture[index] / 257U);
    }
    std::fill(tiles.edges.begin(), tiles.edges.end(), skeleton.definitions.no_edge);
    for (std::size_t index = 0; index < count; ++index) {
        const auto downstream = skeleton.rivers.downstream[index];
        const auto river_class = skeleton.rivers.river_class[index];
        if (river_class == 0 || downstream < 0) {
            continue;
        }
        const auto target = static_cast<std::size_t>(downstream);
        const auto adjacent = neighbors(index, tiles.width, tiles.height);
        if (target >= count ||
            std::find(adjacent.begin(), adjacent.end(), target) == adjacent.end()) {
            throw std::runtime_error{"河流 downstream 不是四鄰接 tile"};
        }
        const auto edge = river_class == 3   ? skeleton.definitions.great_river
                          : river_class == 2 ? skeleton.definitions.river
                                             : skeleton.definitions.stream;
        const world::RegionXY from{static_cast<std::int16_t>(index % tiles.width),
                                   static_cast<std::int16_t>(index / tiles.width)};
        const world::RegionXY to{static_cast<std::int16_t>(target % tiles.width),
                                 static_cast<std::int16_t>(target / tiles.width)};
        tiles.set_edge(from, to, edge);
    }
    return tiles;
}

std::uint64_t hash_stage(const PlateStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_scalar(hash, static_cast<std::uint64_t>(stage.plates.size()));
    for (const auto& plate : stage.plates) {
        hash_scalar(hash, plate.x);
        hash_scalar(hash, plate.y);
        hash_scalar(hash, static_cast<std::uint8_t>(plate.is_oceanic));
        hash_scalar(hash, plate.drift_x);
        hash_scalar(hash, plate.drift_y);
        hash_scalar(hash, plate.base_elevation);
    }
    hash_vector(hash, stage.plate_index);
    hash_vector(hash, stage.boundary_type);
    hash_vector(hash, stage.boundary_effect);
    return hash;
}

std::uint64_t hash_stage(const HeightStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_double_vector(hash, stage.elevation);
    hash_vector(hash, stage.land);
    hash_double(hash, stage.sea_level);
    return hash;
}

std::uint64_t hash_stage(const ErosionStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_double_vector(hash, stage.elevation);
    hash_vector(hash, stage.land);
    hash_double(hash, stage.sea_level);
    return hash;
}

std::uint64_t hash_stage(const ClimateStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_vector(hash, stage.temperature_tenths);
    hash_vector(hash, stage.moisture);
    hash_vector(hash, stage.prevailing_wind_x);
    return hash;
}

std::uint64_t hash_stage(const RiverStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_vector(hash, stage.filled_elevation);
    hash_vector(hash, stage.downstream);
    hash_vector(hash, stage.flow);
    hash_vector(hash, stage.river_class);
    hash_vector(hash, stage.moisture);
    hash_vector(hash, stage.lake);
    return hash;
}

std::uint64_t hash_stage(const BiomeStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_vector(hash, stage.terrain);
    hash_vector(hash, stage.relief);
    return hash;
}

std::uint64_t hash_stage(const FeatureStageOutput& stage) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, stage.width);
    hash_scalar(hash, stage.height);
    hash_vector(hash, stage.feature);
    return hash;
}

std::uint64_t hash_skeleton(const RegionSkeleton& skeleton) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, skeleton.elevation.width);
    hash_scalar(hash, skeleton.elevation.height);
    hash_vector(hash, skeleton.elevation.meters);
    hash_vector(hash, skeleton.elevation.land);
    hash_scalar(hash, skeleton.elevation.sea_level);
    hash_scalar(hash, hash_stage(skeleton.climate));
    hash_scalar(hash, hash_stage(skeleton.rivers));
    hash_scalar(hash, hash_stage(skeleton.biome));
    hash_scalar(hash, hash_stage(skeleton.features));
    hash_scalar(hash, rules::value_of(skeleton.definitions.land));
    hash_scalar(hash, rules::value_of(skeleton.definitions.ocean));
    hash_scalar(hash, rules::value_of(skeleton.definitions.plain));
    hash_scalar(hash, rules::value_of(skeleton.definitions.no_feature));
    hash_scalar(hash, rules::value_of(skeleton.definitions.forest));
    hash_scalar(hash, rules::value_of(skeleton.definitions.mine));
    hash_scalar(hash, rules::value_of(skeleton.definitions.oasis));
    hash_scalar(hash, rules::value_of(skeleton.definitions.landmark));
    hash_scalar(hash, rules::value_of(skeleton.definitions.no_edge));
    hash_scalar(hash, rules::value_of(skeleton.definitions.stream));
    hash_scalar(hash, rules::value_of(skeleton.definitions.river));
    hash_scalar(hash, rules::value_of(skeleton.definitions.great_river));
    return hash;
}

std::uint64_t hash_tiles(const world::RegionTiles& tiles) noexcept {
    auto hash = UINT64_C(14695981039346656037);
    hash_scalar(hash, tiles.width);
    hash_scalar(hash, tiles.height);
    hash_vector(hash, tiles.base);
    hash_vector(hash, tiles.relief);
    hash_vector(hash, tiles.feature);
    hash_vector(hash, tiles.temperature);
    hash_vector(hash, tiles.moisture);
    hash_vector(hash, tiles.elevation);
    hash_vector(hash, tiles.edges);
    hash_vector(hash, tiles.owner);
    hash_scalar(hash, static_cast<std::uint64_t>(tiles.site.size()));
    for (const auto& site : tiles.site) {
        hash_scalar(hash, site.lod);
        hash_scalar(hash, static_cast<std::uint8_t>(site.ever_realized));
    }
    return hash;
}

double land_fraction(const RegionSkeleton& skeleton) noexcept {
    if (skeleton.elevation.land.empty()) {
        return 0.0;
    }
    const auto count = std::count_if(skeleton.elevation.land.begin(), skeleton.elevation.land.end(),
                                     [](std::uint8_t value) { return value != 0; });
    return static_cast<double>(count) / static_cast<double>(skeleton.elevation.land.size());
}

bool land_is_single_component(const RegionSkeleton& skeleton) {
    const auto count = checked_count(skeleton.elevation.width, skeleton.elevation.height);
    if (skeleton.elevation.land.size() != count) {
        return false;
    }
    const auto total_land = static_cast<std::size_t>(
        std::count_if(skeleton.elevation.land.begin(), skeleton.elevation.land.end(),
                      [](std::uint8_t value) { return value != 0; }));
    if (total_land == 0) {
        return false;
    }
    return largest_land_component(skeleton.elevation.land, skeleton.elevation.width,
                                  skeleton.elevation.height)
               .size() == total_land;
}

std::vector<std::uint8_t> grayscale(const PlateStageOutput& stage) {
    if (stage.plates.empty()) {
        return {};
    }
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.plate_index.size());
    for (const auto plate : stage.plate_index) {
        pixels.push_back(
            static_cast<std::uint8_t>((static_cast<unsigned>(plate) * 255U) /
                                      std::max<std::size_t>(1, stage.plates.size() - 1U)));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const HeightStageOutput& stage) {
    return grayscale_values(stage.elevation);
}

std::vector<std::uint8_t> grayscale(const ErosionStageOutput& stage) {
    return grayscale_values(stage.elevation);
}

std::vector<std::uint8_t> grayscale(const ClimateStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.moisture.size());
    for (const auto moisture : stage.moisture) {
        pixels.push_back(static_cast<std::uint8_t>(moisture / 257U));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const RiverStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.river_class.size());
    for (const auto river_class : stage.river_class) {
        pixels.push_back(static_cast<std::uint8_t>(river_class * 85U));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const BiomeStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.terrain.size());
    for (std::size_t index = 0; index < stage.terrain.size(); ++index) {
        pixels.push_back(static_cast<std::uint8_t>((rules::value_of(stage.terrain[index]) * 53U +
                                                    rules::value_of(stage.relief[index]) * 17U) &
                                                   UINT8_MAX));
    }
    return pixels;
}

std::vector<std::uint8_t> grayscale(const FeatureStageOutput& stage) {
    std::vector<std::uint8_t> pixels;
    pixels.reserve(stage.feature.size());
    for (const auto feature : stage.feature) {
        pixels.push_back(static_cast<std::uint8_t>((rules::value_of(feature) * 61U) & UINT8_MAX));
    }
    return pixels;
}

}  // namespace aetheria::worldgen
