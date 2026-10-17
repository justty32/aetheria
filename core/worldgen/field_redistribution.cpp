#include "core/worldgen/field_redistribution.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace aetheria::worldgen {
namespace {

// 等化排序鍵：值小者較乾。全為整數，不含格子編號。
[[nodiscard]] auto moisture_rank_key(const RiverStageOutput &field,
                                     std::size_t index) noexcept {
  return std::make_tuple(field.moisture[index], field.lake[index],
                         field.flow[index],
                         static_cast<std::uint16_t>(
                             UINT16_MAX - field.filled_elevation[index]));
}

void equalize_land_moisture(RiverStageOutput &field,
                            const QuantizedElevation &elevation) {
  const auto count = field.moisture.size();
  if (elevation.land.size() != count || field.lake.size() != count ||
      field.flow.size() != count || field.filled_elevation.size() != count) {
    throw std::invalid_argument{"濕度重整的欄位尺寸不一致"};
  }
  std::vector<std::uint32_t> order;
  order.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    if (elevation.land[index] != 0) {
      order.push_back(static_cast<std::uint32_t>(index));
    }
  }
  if (order.size() < 2U) {
    return;
  }
  // 格子編號只讓比較成為全序；同鍵群在下面一律給同一值，編號不影響結果。
  std::ranges::sort(order, [&](std::uint32_t left, std::uint32_t right) {
    const auto left_key = moisture_rank_key(field, left);
    const auto right_key = moisture_rank_key(field, right);
    return left_key != right_key ? left_key < right_key : left < right;
  });
  const auto lowest = static_cast<std::uint64_t>(field.moisture[order.front()]);
  const auto span =
      static_cast<std::uint64_t>(field.moisture[order.back()]) - lowest;
  const auto last_rank = static_cast<std::uint64_t>(order.size() - 1U);
  std::vector<std::uint16_t> remapped(order.size());
  std::size_t group_begin = 0;
  while (group_begin < order.size()) {
    auto group_end = group_begin + 1U;
    const auto key = moisture_rank_key(field, order[group_begin]);
    while (group_end < order.size() &&
           moisture_rank_key(field, order[group_end]) == key) {
      ++group_end;
    }
    // 同鍵群取首尾 rank 的中點（兩倍 rank 以免半格捨入），四捨五入到整數。
    const auto doubled_rank =
        static_cast<std::uint64_t>(group_begin) + (group_end - 1U);
    const auto value =
        lowest + (doubled_rank * span + last_rank) / (2U * last_rank);
    std::fill(remapped.begin() + static_cast<std::ptrdiff_t>(group_begin),
              remapped.begin() + static_cast<std::ptrdiff_t>(group_end),
              static_cast<std::uint16_t>(value));
    group_begin = group_end;
  }
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    field.moisture[order[rank]] = remapped[rank];
  }
}

} // namespace

ErosionStageOutput redistribute(ErosionStageOutput field,
                                const ElevationRedistributionParams &params) {
  return redistribute(std::move(field), params,
                      [](auto &, const auto &) noexcept {});
}

RiverStageOutput redistribute(RiverStageOutput field,
                              const QuantizedElevation &elevation,
                              const MoistureRedistributionParams &params) {
  return redistribute(std::move(field), params,
                      [&elevation](RiverStageOutput &stage,
                                   const MoistureRedistributionParams &) {
                        equalize_land_moisture(stage, elevation);
                      });
}

} // namespace aetheria::worldgen
