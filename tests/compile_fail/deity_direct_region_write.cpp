#include "core/rules/power_sources.h"

#include <vector>

int main() {
    aetheria::rules::RootFaithState root;
    std::vector<aetheria::rules::FaithFollower> followers;
    aetheria::rules::RegionMagicState region;
    // 神祇若可把 Region 當第四個參數傳入，就能繞過信徒／事件中介。
    static_cast<void>(aetheria::rules::fall_deity(root, "deity.test", followers, region));
}
