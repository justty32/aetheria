// 負向控制：直接執行必須因 R=2^28 超出 combat.toml 的 2^27 定義域而中止。

#include "core/rules/combat.h"
#include "core/rules/ruleset.h"

int main() {
    const auto ruleset =
        aetheria::rules::RulesetLoader::load(AETHERIA_SOURCE_DIR "/data");
    const auto& rules = ruleset.combat_rules();
    static_cast<void>(aetheria::rules::fixed_ratio_power(1LL << 28, 1,
                                                         rules.default_exponent, rules));
}
