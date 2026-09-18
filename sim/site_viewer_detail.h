#pragma once

#include "core/site/site_projection.h"
#include "sim/debug_canvas.h"

namespace aetheria::sim::detail::site_viewer {

inline constexpr std::int32_t kTilePixels = 8;

void paint_ground(DebugCanvas& canvas, const site::SiteSkeleton& skeleton,
                  const rules::Ruleset& ruleset);

void paint_roads(DebugCanvas& canvas, const site::SiteSkeleton& skeleton);

void paint_edges(DebugCanvas& canvas, const std::vector<rules::EdgeId>& edges,
                 const rules::Ruleset& ruleset);

void paint_blocks(DebugCanvas& canvas, const std::vector<site::SiteBlock>& blocks);

} // namespace aetheria::sim::detail::site_viewer
