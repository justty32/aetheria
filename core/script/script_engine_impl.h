#pragma once

#include "core/script/script_engine_detail.h"
#include <map>

namespace aetheria::script {

class ScriptEngine::Impl {
public:
    Impl();
    explicit Impl(std::vector<ScriptSource> sources);
    [[nodiscard]] sol::environment make_environment();
    [[nodiscard]] const detail::Definition& require(std::string_view id, HookKind hook) const;
    [[nodiscard]] sol::protected_function function(const detail::Definition& definition, HookKind hook,
                                                   sol::environment& environment);
    [[nodiscard]] std::vector<std::string> definition_ids() const;
    [[nodiscard]] std::vector<std::string> sandbox_keys() const;

private:
    void initialize();

    sol::state lua_;
    sol::environment base_;
    std::map<std::string, detail::Definition, std::less<>> definitions_;
};

} // namespace aetheria::script
