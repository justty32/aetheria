#include <aetheria/script/context.h>

// Lua 綁定 target 若能取得此內部標頭，就能繞過 Context 直接碰世界真值。
#include <core/script/context_internal.h>

int main() {
    using aetheria::script::detail::ContextAccess;
    auto member = &ContextAccess::unsafe_world_for_core;
    static_cast<void>(member);
}
