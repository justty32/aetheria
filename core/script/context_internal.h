#pragma once

// context_internal.h：Lua 執行器專用的 Region 真值接點；受限 script target 看不到本檔。

#include <aetheria/script/context.h>

#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <string_view>

namespace aetheria::script {
class ScriptWorldState;
}

namespace aetheria::script::detail {

class ContextAccess {
public:
    ContextAccess(ScriptWorldState& world, std::mt19937_64& rng) : world_{world}, rng_{rng} {}

    [[nodiscard]] Context make_context() { return Context{this}; }
    [[nodiscard]] std::int64_t owner(std::string_view object_id) const;
    void set_owner(std::string_view object_id, std::int64_t owner);
    [[nodiscard]] std::int64_t random_integer(std::int64_t minimum, std::int64_t maximum);
    void commit();

    // 只有 C++ core 內部可見；compile-fail 測試用它證明 include
    // 隔離不是壞路徑假通過。
    [[nodiscard]] ScriptWorldState* unsafe_world_for_core() noexcept { return &world_; }

private:
    ScriptWorldState& world_;
    std::mt19937_64& rng_;
    std::map<std::string, std::uint16_t, std::less<>> pending_;
};

}  // namespace aetheria::script::detail
