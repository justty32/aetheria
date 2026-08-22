#pragma once

// context.h：Lua 只看得到的受限查詢、事件化寫入與決定性 RNG 型別。

#include <cstdint>
#include <string_view>

namespace aetheria::script {

namespace detail {
class ContextAccess;
}

// ScriptRng 是單次掛勾專用的 mt19937_64 視圖。
// Context 擁有此視圖；掛勾返回後失效，腳本只能呼叫閉區間整數抽樣。
class ScriptRng {
public:
    [[nodiscard]] std::int64_t integer(std::int64_t minimum, std::int64_t maximum);

private:
    friend class detail::ContextAccess;
    friend class Context;
    explicit ScriptRng(detail::ContextAccess* access) : access_{access} {}

    detail::ContextAccess* access_{};
};

// Context 是 Lua 掛勾取得的受限世界視圖。
// Context 不暴露 RegionTiles 或其指標；寫入只能先驗證並記錄 owner 事件。
// 掛勾返回後此借用視圖及其 RNG 都失效。
class Context {
public:
    [[nodiscard]] std::int64_t owner(std::string_view object_id) const;
    void set_owner(std::string_view object_id, std::int64_t owner);
    [[nodiscard]] ScriptRng& rng() noexcept { return rng_; }

private:
    friend class detail::ContextAccess;
    explicit Context(detail::ContextAccess* access) : access_{access}, rng_{access} {}

    detail::ContextAccess* access_{};
    ScriptRng rng_;
};

}  // namespace aetheria::script
