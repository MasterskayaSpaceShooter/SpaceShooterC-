#include <algorithm>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "protocol.h"
#include "types.h"

namespace {

std::string asString(const std::vector<std::uint8_t>& v) {
    return {v.begin(), v.end()};
}

std::vector<std::uint8_t> asBytes(std::string_view s) {
    return {s.begin(), s.end()};
}

}  // namespace

// ===========================================================================
// encode()
// ===========================================================================

TEST(Encode, Join_NormalName) {
    EXPECT_EQ(asString(protocol::encode(protocol::JoinMsg{"alice"})), "JOIN alice");
}

TEST(Encode, Join_EmptyName_NoValidationAtProtocolLayer) {
    EXPECT_EQ(asString(protocol::encode(protocol::JoinMsg{""})), "JOIN ");
}

TEST(Encode, Join_NameWithSpaces_PassedThroughVerbatim) {
    EXPECT_EQ(asString(protocol::encode(protocol::JoinMsg{"alice smith"})), "JOIN alice smith");
}

TEST(Encode, Move_AllDirections) {
    using types::Direction;
    EXPECT_EQ(asString(protocol::encode(protocol::MoveMsg{Direction::Up})), "MOVE up");
    EXPECT_EQ(asString(protocol::encode(protocol::MoveMsg{Direction::Down})), "MOVE down");
    EXPECT_EQ(asString(protocol::encode(protocol::MoveMsg{Direction::Left})), "MOVE left");
    EXPECT_EQ(asString(protocol::encode(protocol::MoveMsg{Direction::Right})), "MOVE right");
}

TEST(Encode, Attack_ExactWire) {
    EXPECT_EQ(asString(protocol::encodeAttack()), "ATTACK");
}

// ===========================================================================
// decode() — known keywords
// ===========================================================================

TEST(Decode, KnownKeywords_ExactWire) {
    EXPECT_EQ(protocol::decode(asBytes("ACCEPT")).type, protocol::ServerMsgType::Accept);
    EXPECT_EQ(protocol::decode(asBytes("REJECT")).type, protocol::ServerMsgType::Reject);
    EXPECT_EQ(protocol::decode(asBytes("GAME_END")).type, protocol::ServerMsgType::GameEnd);
}

// ===========================================================================
// decode() — exact keyword match (regression guard vs. the old starts_with bug)
// ===========================================================================

class DecodeRejectsSimilarKeywords : public ::testing::TestWithParam<std::string_view> {};

TEST_P(DecodeRejectsSimilarKeywords, YieldsUnknown) {
    EXPECT_EQ(protocol::decode(asBytes(GetParam())).type, protocol::ServerMsgType::Unknown);
}

INSTANTIATE_TEST_SUITE_P(SimilarKeywords,
                         DecodeRejectsSimilarKeywords,
                         ::testing::Values(
                             // extended form of a known keyword
                             "ACCEPTED",
                             "REJECTED",
                             "GAME_ENDED",
                             // truncated form — catches substring matching, not just prefix matching
                             "ACCEP",
                             "REJEC",
                             "GAME_EN",
                             // case variants — protocol is case-sensitive
                             "accept",
                             "reject",
                             "game_end"));

// ===========================================================================
// decode() — argument semantics
// ===========================================================================

TEST(Decode, Argument_EmptyForPayloadlessKeyword) {
    EXPECT_TRUE(protocol::decode(asBytes("ACCEPT")).argument.empty());
}

TEST(Decode, Argument_MultiWordPreservedVerbatim) {
    EXPECT_EQ(protocol::decode(asBytes("REJECT your name is too long")).argument, "your name is too long");
}

TEST(Decode, Argument_SplitAtFirstSpaceOnly) {
    // Extra spaces belong to the argument, not the keyword.
    EXPECT_EQ(protocol::decode(asBytes("REJECT  two spaces")).argument, "two spaces");
}

TEST(Decode, Argument_OnPayloadlessKeyword_IsLenient) {
    // Documented policy: controller ignores the argument for Accept.
    const auto m = protocol::decode(asBytes("ACCEPT extra"));
    EXPECT_EQ(m.type, protocol::ServerMsgType::Accept);
    EXPECT_EQ(m.argument, "extra");
}

// ===========================================================================
// decode() — edge cases
// ===========================================================================

TEST(Decode, EmptyInput_YieldsUnknown) {
    const auto m = protocol::decode({});
    EXPECT_EQ(m.type, protocol::ServerMsgType::Unknown);
    EXPECT_TRUE(m.argument.empty());
}

TEST(Decode, UnknownKeyword_PreservesArgumentForDiagnostics) {
    const auto m = protocol::decode(asBytes("PING hello world"));
    EXPECT_EQ(m.type, protocol::ServerMsgType::Unknown);
    EXPECT_EQ(m.argument, "hello world");
}

TEST(Decode, ByteOriented_EmbeddedNulPreserved) {
    // Guards against any future refactor to C-string parsing (strlen/strchr).
    const std::vector<std::uint8_t> raw = {'R', 'E', 'J', 'E', 'C', 'T', ' ', 0x00, 'x'};
    const auto m = protocol::decode(raw);
    ASSERT_EQ(m.type, protocol::ServerMsgType::Reject);
    ASSERT_EQ(m.argument.size(), 2u);
    EXPECT_EQ(m.argument[0], '\0');
    EXPECT_EQ(m.argument[1], 'x');
}

// ===========================================================================
// ServerMsg owns its argument (not a view into the input buffer)
// ===========================================================================

TEST(Decode, Argument_OwnsItsData) {
    auto bytes = asBytes("REJECT transient");
    const auto m = protocol::decode(bytes);

    // Poison the source buffer in place. If `argument` were a string_view
    // into `bytes`, the comparison below would read 0xAA bytes and fail
    // (or trip ASAN if the buffer happened to be freed).
    std::fill(bytes.begin(), bytes.end(), static_cast<std::uint8_t>(0xAA));

    EXPECT_EQ(m.type, protocol::ServerMsgType::Reject);
    EXPECT_EQ(m.argument, "transient");
}
