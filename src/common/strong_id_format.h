#pragma once

#include <format>

#include "strong_id.h"

/// Formatter for StrongId that delegates to the underlying type's formatter.
///
/// Kept separate so that users who don't format StrongIds
/// don't pay for <format> (heavyweight).
template <typename Tag, typename T>
struct std::formatter<types::StrongId<Tag, T>> : std::formatter<T> {
    auto format(const types::StrongId<Tag, T>& id, std::format_context& ctx) const {
        return std::formatter<T>::format(id.get(), ctx);
    }
};
