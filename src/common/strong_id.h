#pragma once

#include <compare>  // IWYU pragma: keep // operator<=>
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace types {

template <typename Tag, typename T = std::uint64_t>
class StrongId {
public:
    using value_type = T;

    constexpr StrongId() noexcept = default;
    explicit constexpr StrongId(value_type v) noexcept : value(v) {}

    auto operator<=>(const StrongId&) const = default;

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

template <typename T>
struct StrongIdIdentityHash {
    constexpr std::size_t operator()(const T& id) const noexcept {
        auto v = static_cast<T::value_type>(id);

        if constexpr (sizeof(std::size_t) == 8) {
            // 64bit
            return static_cast<std::size_t>(v);
        } else {
            constexpr int kShift32 = 32;
            return static_cast<std::size_t>(v ^ (v >> kShift32));
        }
    }
};

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

            v ^= v >> kShift30;
            v *= kMixMul1;
            v ^= v >> kShift27;
            v *= kMixMul2;
            v ^= v >> kShift31;

            return static_cast<std::size_t>(v);
        } else {
            // consts for 32bit
            constexpr std::uint32_t kMixMul32 = 0x45d9f3bU;

            constexpr int kShift64To32 = 32;
            constexpr int kShift16 = 16;

            auto low = static_cast<std::uint32_t>(v ^ (v >> kShift64To32));
            low ^= low >> kShift16;
            low *= kMixMul32;
            low ^= low >> kShift16;

            return static_cast<std::size_t>(low);
        }
    }
};

}  // namespace types
