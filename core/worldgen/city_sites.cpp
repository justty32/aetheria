#include "core/worldgen/region_civ_stages.h"

#include "core/worldgen/city_selection.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace aetheria::worldgen {

CityStageOutput generate_cities(const QuantizedElevation& elevation,
                                const ClimateStageOutput& climate,
                                const RiverStageOutput& rivers,
                                const BiomeStageOutput& biome,
                                const HistoryStageOutput& history,
                                const rules::Ruleset& ruleset, std::uint64_t stage_seed,
                                const CityGenerationConfig& config) {
    const auto& civilization = ruleset.civilization_rules();
    if (!civilization.loaded || history.features.width != elevation.width ||
        history.features.height != elevation.height ||
        history.survivor.size() != elevation.meters.size()) {
        throw std::invalid_argument{"城市階段缺少有效歷史層或文明規則"};
    }
    auto output = score_city_sites(elevation, climate, rivers, biome, history.features, ruleset);
    for (std::size_t index = 0; index < output.score.size(); ++index) {
        if (history.survivor[index] == 0) {
            continue;
        }
        const auto score = static_cast<std::int64_t>(output.score[index]) +
                           civilization.history.ancient_site_bonus;
        output.score[index] = static_cast<std::int32_t>(
            std::clamp<std::int64_t>(score, std::numeric_limits<std::int32_t>::min(),
                                     std::numeric_limits<std::int32_t>::max()));
    }
    output.cities = detail::select_city_sites(
        elevation, output, stage_seed,
        {civilization.target_city_count, civilization.major_city_count,
         civilization.town_count, civilization.minimum_spacing, config.minimum_score_bias});
    if (output.cities.size() < 2) {
        throw std::runtime_error{"城市選址不足兩座，無法建立道路"};
    }
    return output;
}

}  // namespace aetheria::worldgen
