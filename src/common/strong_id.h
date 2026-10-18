#pragma once

#include <compare>  // IWYU pragma: keep // operator<=>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>

namespace types {

/// Strongly-typed wrapper around an integral identifier.
///
/// Prevents accidental mixing of unrelated IDs (e.g. UserId vs OrderId)
/// that share the same underlying representation.
///
/// Typical usage:
/// @code
///   struct UserTag;
///   struct OrderTag;
///   using UserId  = types::StrongId<UserTag>;
///   using OrderId = types::StrongId<OrderTag>;
/// @endcode
///
/// @tparam Tag  Phantom type distinguishing this ID from others.
/// @tparam T    Underlying integral type (default: std::uint64_t).
template <typename Tag, typename T = std::uint64_t>
class StrongId {
public:
    static_assert(std::is_integral_v<T>, "StrongId requires an integral underlying type");
    using value_type = T;

    constexpr StrongId() noexcept = default;
    explicit constexpr StrongId(value_type v) noexcept : value(v) {}

    auto operator<=>(const StrongId&) const = default;

    /// Explicit unwrap. Deliberately explicit so that StrongId doesn't
    /// implicitly decay to its underlying type and re-enable accidental
    /// mixing of unrelated IDs.
    explicit constexpr operator value_type() const noexcept {
        return value;
    }

    constexpr value_type get() const noexcept {
        return value;
    }

    constexpr StrongId& operator++() noexcept {
        ++value;
        return *this;
    }

    constexpr StrongId operator++(int) noexcept {
        StrongId tmp = *this;
        ++value;
        return tmp;
    }

    constexpr StrongId& operator--() noexcept {
        --value;
        return *this;
    }
    constexpr StrongId operator--(int) noexcept {
        StrongId tmp = *this;
        --value;
        return tmp;
    }

private:
    value_type value{};
};

template <typename Tag, typename T>
std::ostream& operator<<(std::ostream& os, const StrongId<Tag, T>& id) {
    return os << id.get();
}

/// Hash that returns the underlying value verbatim.
///
/// Fastest, but produces poorly distributed buckets when IDs are
/// sequential or share high bits. Suitable for dense/contiguous IDs
/// where the hash table already sizes buckets appropriately.
template <typename T>
struct StrongIdIdentityHash {
    constexpr std::size_t operator()(const T& id) const noexcept {
        auto v = static_cast<T::value_type>(id);

        if constexpr (sizeof(std::size_t) == 8) {
            return static_cast<std::size_t>(v);
        } else if constexpr (sizeof(v) > sizeof(std::size_t)) {
            constexpr int kShift = 32;
            return static_cast<std::size_t>(v ^ (v >> kShift));
        } else {
            return static_cast<std::size_t>(v);
        }
    }
};

/// Hash that mixes bits (SplitMix64 / MurmurHash3 finalizer).
///
/// Better bucket distribution for sparse or sequential IDs.
/// Used by the std::hash<StrongId<...>> specialization.
template <typename T>
struct StrongIdMixedHash {
    constexpr std::size_t operator()(const T& id) const noexcept {
        auto v = static_cast<T::value_type>(id);

        if constexpr (sizeof(std::size_t) == 8) {
            // consts for SplitMix64 / MurmurHash3
            constexpr std::uint64_t kMixMul1 = 0xbf58476d1ce4e5b9ULL;
            constexpr std::uint64_t kMixMul2 = 0x94d049bb133111ebULL;

            constexpr int kShift30 = 30;
            constexpr int kShift27 = 27;
            constexpr int kShift31 = 31;

            std::uint64_t x = static_cast<std::uint64_t>(v);
            x ^= x >> kShift30;
            x *= kMixMul1;
            x ^= x >> kShift27;
            x *= kMixMul2;
            x ^= x >> kShift31;
            return static_cast<std::size_t>(x);
        } else {
            // consts for 32bit
            constexpr std::uint32_t kMixMul32 = 0x45d9f3bU;
            constexpr int kShift16 = 16;

            auto x = static_cast<std::uint64_t>(v);
            auto low = static_cast<std::uint32_t>(x ^ (x >> 32));
            low ^= low >> kShift16;
            low *= kMixMul32;
            low ^= low >> kShift16;

            return static_cast<std::size_t>(low);
        }
    }
};

}  // namespace types

/// std::hash specialization delegating to StrongIdMixedHash.
/// Note: this fixes the hash strategy for default unordered_* containers.
/// For identity hashing (dense/sequential IDs), specify
/// types::StrongIdIdentityHash explicitly as the Hasher.
template <typename Tag, typename T>
struct std::hash<types::StrongId<Tag, T>> {
    constexpr std::size_t operator()(const types::StrongId<Tag, T>& id) const noexcept {
        return types::StrongIdMixedHash<types::StrongId<Tag, T>>{}(id);
    }
};
