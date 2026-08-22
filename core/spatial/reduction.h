#pragma once

// reduction.h：L2→L1 與 L3→L2 共用的封閉 row storage、絕對快照套用與雜湊。

#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <vector>

namespace aetheria::spatial::reduction {

template <typename Row> struct Field {
    using RowType = Row;
    std::vector<typename Row::Value> values;

    template <typename Archive> void serialize(Archive& archive) { archive(values); }
};

template <typename Row> struct SnapshotValue {
    using RowType = Row;
    std::optional<typename Row::Value> value;
};

template <typename Row, typename Rows> struct RowsContain;

template <typename Row, typename... Rows>
struct RowsContain<Row, std::tuple<Rows...>>
    : std::disjunction<std::is_same<Row, Rows>...> {};

template <typename Rows> struct StorageFor;

template <typename... Rows> struct StorageFor<std::tuple<Rows...>> {
    using Fields = std::tuple<Field<Rows>...>;
    using Values = std::tuple<SnapshotValue<Rows>...>;
};

template <typename Rows> struct Storage {
    typename StorageFor<Rows>::Fields fields;
};

namespace detail {

inline constexpr std::uint64_t kFnvOffset = UINT64_C(14695981039346656037);
inline constexpr std::uint64_t kFnvPrime = UINT64_C(1099511628211);

inline void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= kFnvPrime;
}

template <typename Value, bool = std::is_enum_v<Value>> struct ScalarRaw {
    using Type = Value;
};

template <typename Value> struct ScalarRaw<Value, true> {
    using Type = std::underlying_type_t<Value>;
};

template <typename Value> void hash_scalar(std::uint64_t& hash, Value value) noexcept {
    static_assert(std::is_integral_v<Value> || std::is_enum_v<Value>);
    using Raw = typename ScalarRaw<Value>::Type;
    using Unsigned = std::make_unsigned_t<Raw>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t byte = 0; byte < sizeof(bits); ++byte) {
        hash_byte(hash, static_cast<std::uint8_t>(bits >> (byte * 8U)));
    }
}

template <typename... Rows>
[[nodiscard]] std::size_t apply_absolute(
    std::tuple<Field<Rows>...>& fields, std::size_t index,
    const std::tuple<SnapshotValue<Rows>...>& values) {
    std::size_t writes{};
    std::apply(
        [&](const auto&... value) {
            (([&] {
                 if (value.value.has_value()) {
                     using Row = typename std::remove_cvref_t<decltype(value)>::RowType;
                     std::get<Field<Row>>(fields).values.at(index) = *value.value;
                     ++writes;
                 }
             }()),
             ...);
        },
        values);
    return writes;
}

template <typename... Rows>
[[nodiscard]] std::uint64_t hash_values(
    const std::tuple<SnapshotValue<Rows>...>& values) noexcept {
    auto hash = kFnvOffset;
    std::apply(
        [&](const auto&... value) {
            ((hash_byte(hash, static_cast<std::uint8_t>(value.value.has_value())),
              value.value.has_value() ? (hash_scalar(hash, *value.value), void()) : void()),
             ...);
        },
        values);
    return hash;
}

}  // namespace detail

// Authority 是該層唯一可建立與套用快照的 ReductionTable。
template <typename Rows, typename Authority> class Snapshot {
public:
    template <typename Row>
    [[nodiscard]] std::optional<typename Row::Value> value() const noexcept {
        static_assert(RowsContain<Row, Rows>::value);
        return std::get<SnapshotValue<Row>>(values_).value;
    }

    [[nodiscard]] std::uint64_t hash() const noexcept {
        return detail::hash_values(values_);
    }

private:
    friend Authority;

    Snapshot() = default;

    [[nodiscard]] std::size_t apply_to(typename StorageFor<Rows>::Fields& fields,
                                       std::size_t index) const {
        return detail::apply_absolute(fields, index, values_);
    }

    typename StorageFor<Rows>::Values values_;
};

template <typename Rows>
void resize(Storage<Rows>& storage, std::size_t count) {
    std::apply([count](auto&... field) { (field.values.resize(count), ...); }, storage.fields);
}

template <typename Rows>
[[nodiscard]] bool valid_layout(const Storage<Rows>& storage, std::size_t count) noexcept {
    return std::apply(
        [count](const auto&... field) { return ((field.values.size() == count) && ...); },
        storage.fields);
}

template <typename Rows>
[[nodiscard]] std::size_t dynamic_bytes(const Storage<Rows>& storage) noexcept {
    return std::apply(
        [](const auto&... field) {
            return ((field.values.size() * sizeof(typename std::remove_cvref_t<
                                               decltype(field)>::RowType::Value)) +
                    ... + std::size_t{});
        },
        storage.fields);
}

}  // namespace aetheria::spatial::reduction
