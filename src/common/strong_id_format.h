#pragma once

#include <format>

#include "strong_id.h"

template <typename Tag, typename T>
struct std::formatter<types::StrongId<Tag, T>> {  // NOLINT

    // Parses format specifiers; must return the iterator pointing to '}'
    constexpr auto parse(std::format_parse_context& ctx) {
        auto it = ctx.begin();
        if (it != ctx.end() && *it != '}') {
            throw std::format_error("Invalid format specifier for StrongId.");
        }
        return it;
    }

    // Formats the StrongId object by delegating its inner value to the standard formatter
    auto format(const types::StrongId<Tag, T>& id, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "{}", id.get());
    }
};
