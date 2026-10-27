#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "types.h"

namespace protocol {

// ---------------------------------------------------------------------------
// Wire format: line-oriented ASCII, `<KEYWORD>[ <argument>]`, no framing.
// The transport (TcpClient) delivers whole payloads; any length prefix or
// delimiter — including trailing CR/LF — is stripped before decode() sees
// the bytes. If the server appends "\r\n", decode() will treat it as part
// of the keyword and return Unknown.
//
// The only stable interface is the encode() / decode() pair. Callers must
// not construct or parse wire strings themselves.
// ---------------------------------------------------------------------------

// ---- Client -> Server ----

/// Enumerates the client-side command set. Not used for dispatch —
/// the per-command encode() overloads are the source of truth.
enum class ClientMsgType { Join, Move, Attack };

/// "JOIN <playerName>"
struct JoinMsg {
    std::string playerName;
};

/// "MOVE <direction>"
struct MoveMsg {
    types::Direction direction;
};

std::vector<std::uint8_t> encode(const JoinMsg&);
std::vector<std::uint8_t> encode(const MoveMsg&);

/// "SHOOT" — a pure signal, no payload.
std::vector<std::uint8_t> encodeShoot();

// ---- Server -> Client ----

enum class ServerMsgType { Accept, Reject, GameEnd, Unknown };

/// Decoded server message. `argument` is the text after the first space;
/// empty when the message carries no payload (e.g. "ACCEPT").
struct ServerMsg {
    ServerMsgType type;
    std::string argument;
};

/// Decodes one full wire payload.
///
/// Never throws for any input — malformed or unrecognised bytes yield
/// ServerMsgType::Unknown (with `argument` still populated, for
/// diagnostics).
///
/// Keyword matching is exact: "ACCEPTED" is Unknown, not Accept.
/// A trailing argument on a keyword that nominally has none
/// (e.g. "ACCEPT extra") is accepted leniently; the controller chooses
/// whether to use `argument`.
ServerMsg decode(std::span<const std::uint8_t> wire);

}  // namespace protocol
