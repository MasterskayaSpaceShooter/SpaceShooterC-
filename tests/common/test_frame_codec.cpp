#include <cstring>
#include <gtest/gtest.h>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "frame_codec.h"

namespace {

using network::FrameCodec;
using network::PayloadView;

using Bytes = std::vector<std::uint8_t>;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

Bytes bigEndianU32(std::uint32_t v) {
    return {static_cast<std::uint8_t>(v >> 24),
            static_cast<std::uint8_t>(v >> 16),
            static_cast<std::uint8_t>(v >> 8),
            static_cast<std::uint8_t>(v)};
}

Bytes join(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const auto& p : parts)
        out.insert(out.end(), p.begin(), p.end());
    return out;
}

// Wire bytes for a frame carrying `length` payload bytes (payload omitted).
Bytes makeRawFrameWithLength(std::uint32_t length) {
    auto magic = bigEndianU32(0xDEADBEEF);
    auto len = bigEndianU32(length);
    magic.insert(magic.end(), len.begin(), len.end());
    return magic;
}

Bytes makeRawFrame(std::uint32_t length, const Bytes& payload) {
    auto out = makeRawFrameWithLength(length);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

Bytes frameToBytes(const FrameCodec::Frame& frame) {
    Bytes out;
    for (auto buf : frame.buffers()) {
        auto* p = static_cast<const std::uint8_t*>(buf.data());
        out.insert(out.end(), p, p + buf.size());
    }
    return out;
}

void append(boost::beast::flat_buffer& buf, const Bytes& data) {
    auto dst = buf.prepare(data.size());
    std::memcpy(dst.data(), data.data(), data.size());
    buf.commit(data.size());
}

struct DecodeResult {
    std::size_t count = 0;
    std::vector<Bytes> payloads;
};

DecodeResult decodeAll(boost::beast::flat_buffer& buf) {
    DecodeResult r{};
    r.count = FrameCodec::decode(buf, [&](PayloadView p) {
        r.payloads.emplace_back(p.begin(), p.end());
    });
    return r;
}

// ---------------------------------------------------------------------------
// Encode — header layout
// ---------------------------------------------------------------------------

class EncodeLayoutTest : public ::testing::TestWithParam<Bytes> {};

TEST_P(EncodeLayoutTest, HeaderIsBigEndianAndPayloadFollows) {
    const Bytes payload = GetParam();
    auto frame = FrameCodec::encode(Bytes(payload));
    const auto bytes = frameToBytes(frame);

    ASSERT_EQ(bytes.size(), 8u + payload.size());

    Bytes expected_header = bigEndianU32(0xDEADBEEF);
    auto length_bytes = bigEndianU32(static_cast<std::uint32_t>(payload.size()));
    expected_header.insert(expected_header.end(), length_bytes.begin(), length_bytes.end());

    EXPECT_EQ(Bytes(bytes.begin(), bytes.begin() + 8), expected_header);
    EXPECT_EQ(Bytes(bytes.begin() + 8, bytes.end()), payload);
}

INSTANTIATE_TEST_SUITE_P(Payloads,
                         EncodeLayoutTest,
                         ::testing::Values(Bytes{}, Bytes{1, 2, 3}, Bytes{0xFF, 0x00, 0xAA}));

TEST(FrameCodecTest, EncodeMaxSizeAllowed) {
    Bytes payload(FrameCodec::kMaxMessageSize, 0);
    EXPECT_NO_THROW({
        auto frame = FrameCodec::encode(std::move(payload));
        (void)frame;
    });
}

TEST(FrameCodecTest, EncodeOversizedThrows) {
    Bytes payload(FrameCodec::kMaxMessageSize + 1, 0);
    EXPECT_THROW((void)FrameCodec::encode(std::move(payload)), std::length_error);
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, FrameIsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<FrameCodec::Frame>);
    static_assert(!std::is_copy_assignable_v<FrameCodec::Frame>);
    static_assert(std::is_move_constructible_v<FrameCodec::Frame>);
    static_assert(std::is_move_assignable_v<FrameCodec::Frame>);
}

TEST(FrameCodecTest, FrameBuffers) {
    auto frame = FrameCodec::encode(Bytes{1, 2, 3});
    auto bufs = frame.buffers();

    ASSERT_EQ(bufs.size(), 2u);
    EXPECT_EQ(bufs[0].size(), 8u);
    EXPECT_EQ(bufs[1].size(), 3u);
}

// ---------------------------------------------------------------------------
// Decode — happy path
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, DecodeEmptyBuffer) {
    boost::beast::flat_buffer buf;
    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    EXPECT_TRUE(r.payloads.empty());
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeSingleFrame) {
    boost::beast::flat_buffer buf;
    append(buf, makeRawFrame(3, {1, 2, 3}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], (Bytes{1, 2, 3}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeEmptyPayloadFrame) {
    boost::beast::flat_buffer buf;
    append(buf, makeRawFrame(0, {}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_TRUE(r.payloads[0].empty());
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeMultipleFrames) {
    boost::beast::flat_buffer buf;
    append(buf, join({makeRawFrame(2, {1, 1}), makeRawFrame(3, {2, 2, 2})}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 2u);
    EXPECT_EQ(r.payloads, (std::vector<Bytes>{{1, 1}, {2, 2, 2}}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeBackToBackEmptyFrames) {
    boost::beast::flat_buffer buf;
    append(buf, join({makeRawFrame(0, {}), makeRawFrame(0, {}), makeRawFrame(0, {})}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 3u);
    EXPECT_EQ(r.payloads, (std::vector<Bytes>{Bytes{}, Bytes{}, Bytes{}}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodePayloadAtExactlyMaxSize) {
    boost::beast::flat_buffer buf;
    Bytes payload(FrameCodec::kMaxMessageSize, 0x5A);
    append(buf, makeRawFrame(static_cast<std::uint32_t>(FrameCodec::kMaxMessageSize), payload));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0].size(), FrameCodec::kMaxMessageSize);
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeNestedMagicInPayload) {
    boost::beast::flat_buffer buf;
    Bytes payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
    append(buf, makeRawFrame(static_cast<std::uint32_t>(payload.size()), payload));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], payload);
    EXPECT_EQ(buf.size(), 0u);
}

// ---------------------------------------------------------------------------
// Decode — incomplete input (must be a no-op and leave data intact)
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, DecodeIncompleteHeader) {
    boost::beast::flat_buffer buf;
    append(buf, Bytes{0xDE, 0xAD, 0xBE});  // 3 bytes < 8

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    EXPECT_EQ(buf.size(), 3u);
}

TEST(FrameCodecTest, DecodeHeaderOnlyIncompletePayload) {
    boost::beast::flat_buffer buf;
    append(buf, makeRawFrameWithLength(5));  // 8 bytes, claims 5 payload

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    EXPECT_EQ(buf.size(), 8u);
}

TEST(FrameCodecTest, DecodeIncompletePayload) {
    boost::beast::flat_buffer buf;
    auto wire = makeRawFrame(5, {1, 2});  // claims 5, has 2
    append(buf, wire);

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    EXPECT_EQ(buf.size(), wire.size());
}

TEST(FrameCodecTest, DecodeMultipleWithIncompleteTail) {
    boost::beast::flat_buffer buf;
    auto w1 = makeRawFrame(2, {1, 1});
    auto w2 = makeRawFrame(3, {2, 2, 2});
    auto w3 = makeRawFrame(5, {3, 3});  // incomplete
    append(buf, join({w1, w2, w3}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 2u);
    EXPECT_EQ(r.payloads, (std::vector<Bytes>{{1, 1}, {2, 2, 2}}));
    EXPECT_EQ(buf.size(), w3.size());
}

// ---------------------------------------------------------------------------
// Decode — resynchronization
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, DecodeAllJunkMakesProgress) {
    boost::beast::flat_buffer buf;
    append(buf, Bytes(8, 0x00));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    EXPECT_LT(buf.size(), 8u);  // codec made forward progress
}

TEST(FrameCodecTest, DecodeSkipsLeadingJunk) {
    boost::beast::flat_buffer buf;
    append(buf, join({Bytes{0x00, 0x11, 0x22}, makeRawFrame(2, {7, 8})}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], (Bytes{7, 8}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeSkipsPartialMagicPrefix) {
    boost::beast::flat_buffer buf;
    // 0xDE 0xAD 0xBE then a non-EF byte, then a valid frame.
    append(buf, join({Bytes{0xDE, 0xAD, 0xBE, 0x00}, makeRawFrame(2, {7, 8})}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], (Bytes{7, 8}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeSkipsJunkBetweenFrames) {
    boost::beast::flat_buffer buf;
    append(buf, join({makeRawFrame(2, {1, 1}), Bytes{0x00, 0x11}, makeRawFrame(2, {2, 2})}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 2u);
    EXPECT_EQ(r.payloads, (std::vector<Bytes>{{1, 1}, {2, 2}}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, DecodeRejectsOversizedLengthWithoutWaitingForPayload) {
    boost::beast::flat_buffer buf;
    append(buf, makeRawFrameWithLength(static_cast<std::uint32_t>(FrameCodec::kMaxMessageSize + 1)));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 0u);
    // Header must be rejected immediately; the codec must not stall waiting
    // for kMaxMessageSize+1 payload bytes.
    EXPECT_LT(buf.size(), 8u);
}

TEST(FrameCodecTest, DecodeOversizedLengthResyncsToFollowingFrame) {
    boost::beast::flat_buffer buf;
    auto bad = makeRawFrameWithLength(static_cast<std::uint32_t>(FrameCodec::kMaxMessageSize + 1));
    auto good = makeRawFrame(2, {9, 9});
    append(buf, join({bad, good}));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], (Bytes{9, 9}));
    EXPECT_EQ(buf.size(), 0u);
}

// ---------------------------------------------------------------------------
// Decode — callback interaction
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, DecodeConsumesParsedBytesWhenCallbackThrows) {
    boost::beast::flat_buffer buf;
    auto w1 = makeRawFrame(2, {1, 1});
    auto w2 = makeRawFrame(3, {2, 2, 2});
    append(buf, join({w1, w2}));

    int calls = 0;
    EXPECT_THROW((void)FrameCodec::decode(buf,
                                          [&](PayloadView) {
                                              if (++calls == 2)
                                                  throw std::runtime_error("boom");
                                          }),
                 std::runtime_error);

    EXPECT_EQ(calls, 2);
    EXPECT_EQ(buf.size(), w2.size());
}

TEST(FrameCodecTest, DecodePayloadViewAliasesBuffer) {
    boost::beast::flat_buffer buf;
    append(buf, makeRawFrame(3, {0xAA, 0xBB, 0xCC}));

    const auto* expected = static_cast<const std::uint8_t*>(buf.data().data()) + 8;

    const std::uint8_t* seen = nullptr;
    const auto n = FrameCodec::decode(buf, [&](PayloadView p) {
        seen = p.data();
    });

    EXPECT_EQ(n, 1u);
    EXPECT_EQ(seen, expected);  // zero-copy: view points into the buffer
}

// ---------------------------------------------------------------------------
// Decode — streaming
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, DecodeStreamingByteByByte) {
    boost::beast::flat_buffer buf;
    const auto wire = makeRawFrame(4, {1, 2, 3, 4});

    int calls = 0;
    Bytes got;
    for (std::size_t i = 0; i < wire.size(); ++i) {
        append(buf, Bytes{wire[i]});
        const auto n = FrameCodec::decode(buf, [&](PayloadView p) {
            ++calls;
            got.assign(p.begin(), p.end());
        });
        EXPECT_EQ(n, static_cast<std::size_t>(calls));
        if (i + 1 < wire.size()) {
            EXPECT_EQ(calls, 0) << "decoded prematurely at byte " << (i + 1);
        }
    }

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(got, (Bytes{1, 2, 3, 4}));
    EXPECT_EQ(buf.size(), 0u);
}

// ---------------------------------------------------------------------------
// Round-trip
// ---------------------------------------------------------------------------

TEST(FrameCodecTest, RoundTrip) {
    boost::beast::flat_buffer buf;
    append(buf, frameToBytes(FrameCodec::encode(Bytes{1, 2, 3, 4, 5})));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, 1u);
    ASSERT_EQ(r.payloads.size(), 1u);
    EXPECT_EQ(r.payloads[0], (Bytes{1, 2, 3, 4, 5}));
    EXPECT_EQ(buf.size(), 0u);
}

TEST(FrameCodecTest, RoundTripMultiple) {
    const std::vector<Bytes> payloads = {{}, {1}, {1, 2, 3}, {0xFF, 0x00, 0xAA}};

    boost::beast::flat_buffer buf;
    for (const auto& p : payloads)
        append(buf, frameToBytes(FrameCodec::encode(Bytes(p))));

    auto r = decodeAll(buf);

    EXPECT_EQ(r.count, payloads.size());
    EXPECT_EQ(r.payloads, payloads);
    EXPECT_EQ(buf.size(), 0u);
}

}  // namespace
