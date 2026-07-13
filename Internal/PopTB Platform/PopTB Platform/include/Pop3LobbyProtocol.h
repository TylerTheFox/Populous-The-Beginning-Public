#pragma once

// ============================================================================
// Pop3LobbyProtocol.h - Pop3-Server directory protocol, v1
//
// Server-local for now; moves into PopTB Platform's include/ the moment the
// game client learns LOBBY_* (client 3c work). Design:
//   AI_Info/DESIGN_2026-07-12_pop3_server.md (sections 3, 3b, 4 Mode A, G1)
//
// Frame layout mirrors the game transport (Pop3NetworkUDP.cpp:173-177) but
// uses its OWN first byte so a directory datagram can never alias a game
// frame (game uses POP_PACKET_TYPE == 10):
//   [0] = POP3LOBBY_PACKET_TYPE (11)
//   [1] = Pop3LobbyMsg id
//   [2..] = little-endian payload (fixed-width types only, packed - same
//           portability rules as Pop3Wire.h section 2b)
//
// Password policy (v1): the directory NEVER sees passwords. The game's own
// join handshake carries a nonce/SHA1 challenge (P2, Pop3Network.cpp) and the
// Mode A proxy forwards it verbatim, so the HOST enforces the password.
// LOBBY_CREATE only carries a has-password flag for the browser's lock icon.
// Mode B (server-owned roster) is the point where the server needs its own
// challenge - revisit then.
//
// Version policy (owner requirement 2026-07-12): every lobby record carries
// the creator's exact game version triplet (major/minor/build - the same
// three values CLIENT_JOIN_REQUEST sends). The directory REJECTS a LOBBY_JOIN
// whose triplet differs from the lobby's (LOBBY_ERR_VERSION_MISMATCH) so
// mismatches fail fast with a clear message; the game's own join handshake
// (check_join_request, exact equality) remains the authoritative backstop.
// ============================================================================

#include <cstdint>
#include <cstddef>

#define POP3LOBBY_PACKET_TYPE       (11)
#define POP3LOBBY_PROTOCOL_VERSION  (1)

#define POP3LOBBY_MAX_NAME_CHARS    (64)   // == POP3NETWORK_MAX_SESSION_NAME_LENGTH
#define POP3LOBBY_HOST_KEY_LEN      (16)
#define POP3LOBBY_REGION_LEN        (8)
#define POP3LOBBY_ORIGIN_HOST_LEN   (64)

// Mirrors of frozen game-transport constants the Mode A proxy peeks at, kept
// here so the server never includes the Windows-typed Pop3Network.h. Source
// of truth: Pop3Network.h (frame bytes) / Pop3Wire.h (game PacketType ids).
#define POP3LOBBY_GAME_PACKET_TYPE  (10)    // POP_PACKET_TYPE
#define POP3LOBBY_GAME_MSG_POP_DATA (9)     // Pop3NetworkTypes::POP_DATA
#define POP3LOBBY_GAME_MAX_PACKET   (3206)  // MAX_PACKET_SIZE

// True if a POP_DATA payload id (compress flag stripped) is one of the
// SERVER_GAMETURN_PACKET* family (Pop3Wire.h ids 1/3/4/5) - the passive
// "this lobby is in progress" signal the proxy watches for.
constexpr bool pop3lobby_is_server_gameturn(uint8_t packet_id_with_flags)
{
    const uint8_t id = packet_id_with_flags & 0x7Fu; // strip PACKET_COMPRESS_FLAG
    return id == 1 || id == 3 || id == 4 || id == 5;
}

// Message ids start at 0x40, far above the game's Pop3NetworkTypes enum
// (currently 0..20) so logs/dispatchers can never confuse the two spaces.
enum Pop3LobbyMsg : uint8_t
{
    LOBBY_LIST_REQ           = 0x40,  // client -> directory (also the registry health probe); no payload
    LOBBY_LIST_RESP          = 0x41,  // directory -> client, paginated (Pop3LobbyListRespV1 + records)
    LOBBY_CREATE             = 0x42,  // client -> directory (Pop3LobbyCreateV1)
    LOBBY_CREATE_RESP        = 0x43,  // directory -> client (Pop3LobbyCreateRespV1)
    LOBBY_JOIN               = 0x44,  // client -> directory (Pop3LobbyJoinV1)
    LOBBY_JOIN_RESP          = 0x45,  // directory -> client (Pop3LobbyJoinRespV1)
    LOBBY_HOST_REGISTER      = 0x46,  // host game socket -> LOBBY port; NAT-safe host association + keepalive
    LOBBY_HOST_REGISTER_RESP = 0x47,  // lobby port -> host
    LOBBY_CLOSE              = 0x48,  // host game socket -> LOBBY port; key-gated teardown
    // NAT punch for port-restricted host NATs (incl. router hairpin): the
    // proxy's per-joiner sockets have different source ports than the lobby
    // port, so the host's NAT drops them until the host sends one datagram
    // toward each. PUNCH travels on the established lobby-port flow; the
    // host replies with PUNCH_ACK from its game socket to the named
    // per-joiner server port, opening the mapping. The proxy consumes ACKs
    // (lobby frames from the host are never forwarded to joiners).
    LOBBY_HOST_PUNCH         = 0x49,  // lobby port -> host (Pop3LobbyHostPunchV1)
    LOBBY_PUNCH_ACK          = 0x4A,  // host game socket -> per-joiner server port
    // Host migration (5b): a surviving joiner claims the vacated host role.
    // Accepted only when the old host leg has gone silent (or the host sent
    // LOBBY_CLOSE) - the client-side deterministic election guarantees a
    // single claimer, so the server just follows it. On accept the claimer
    // becomes the new host anchor and gets a fresh host key.
    LOBBY_CLAIM_HOST         = 0x4B,  // surviving joiner game socket -> LOBBY port (Pop3LobbyClaimHostV1)
    LOBBY_CLAIM_HOST_RESP    = 0x4C,  // lobby port -> claimer (Pop3LobbyClaimHostRespV1)

    // Seed rendezvous / reconnect (2026-07-13). The game seed (gsi.Seed) is
    // shared bit-identically by every peer, so it is a stable key to re-find a
    // relay session after a Pop3-Server restart. Any peer (host OR joiner) sends
    // LOBBY_RESUME_LOOKUP to the DIRECTORY port; the directory answers the live
    // lobby port for that seed, or LOBBY_ERR_NOT_FOUND if no session holds it.
    LOBBY_RESUME_LOOKUP      = 0x4D,  // peer game socket -> DIRECTORY port (Pop3LobbyResumeLookupV1)
    LOBBY_RESUME_LOOKUP_RESP = 0x4E,  // directory -> peer (Pop3LobbyResumeLookupRespV1)

    DIR_SYNC                 = 0x50,  // server <-> server federation lobby-table exchange (build step 4)
};

enum Pop3LobbyResult : uint8_t
{
    LOBBY_OK                   = 0,
    LOBBY_ERR_NAME_TAKEN       = 1,
    LOBBY_ERR_SERVER_FULL      = 2,   // lobby cap or port range exhausted
    LOBBY_ERR_RATE_LIMITED     = 3,   // per-IP lobby cap
    LOBBY_ERR_NOT_FOUND        = 4,
    LOBBY_ERR_VERSION_MISMATCH = 5,   // game build differs from the lobby's
    LOBBY_ERR_BAD_REQUEST      = 6,   // malformed / wrong protocol version
    LOBBY_ERR_BAD_KEY          = 7,   // LOBBY_HOST_REGISTER / LOBBY_CLOSE key mismatch
    LOBBY_ERR_IN_PROGRESS      = 8,   // lobby exists but the game already started
};

// Lobby record flags (LOBBY_CREATE.Flags and Pop3LobbyRecordV1.Flags)
#define POP3LOBBY_FLAG_HAS_PASSWORD (1 << 0)  // display only - host enforces (see header comment)
#define POP3LOBBY_FLAG_IN_PROGRESS  (1 << 1)  // set by the proxy when server gameturns flow

// LOBBY_JOIN flags
#define POP3LOBBY_JOINFLAG_SPECTATOR (1 << 0) // informational; the game handshake assigns the slot

#pragma pack(push, 1)

// The exact triplet CLIENT_JOIN_REQUEST sends (Pop3Network.cpp SendJoinRequest):
// major, minor as bytes, build as the 16-bit BUILD_NUMBER.
struct Pop3LobbyGameVersion
{
    uint8_t  Major;
    uint8_t  Minor;
    uint16_t Build;
};

struct Pop3LobbyCreateV1
{
    uint8_t              ProtocolVersion;   // POP3LOBBY_PROTOCOL_VERSION
    uint8_t              Flags;             // POP3LOBBY_FLAG_HAS_PASSWORD
    Pop3LobbyGameVersion GameVersion;
    uint16_t             HostGamePort;      // creator's game UDP port at the CREATE source IP
                                            // (loopback / non-NAT hint); 0 = host will LOBBY_HOST_REGISTER
    uint8_t              MaxPlayers;
    uint8_t              Reserved;
    char16_t             Name[POP3LOBBY_MAX_NAME_CHARS];  // UTF-16LE, NUL-terminated
};

struct Pop3LobbyCreateRespV1
{
    uint8_t  ProtocolVersion;
    uint8_t  Result;                            // Pop3LobbyResult
    uint16_t LobbyPort;                         // valid when Result == LOBBY_OK
    uint8_t  HostKey[POP3LOBBY_HOST_KEY_LEN];   // random; proves host identity to the LobbySession
};

struct Pop3LobbyJoinV1
{
    uint8_t              ProtocolVersion;
    uint8_t              Flags;             // POP3LOBBY_JOINFLAG_*
    Pop3LobbyGameVersion GameVersion;
    char16_t             Name[POP3LOBBY_MAX_NAME_CHARS];
};

struct Pop3LobbyJoinRespV1
{
    uint8_t              ProtocolVersion;
    uint8_t              Result;
    uint16_t             LobbyPort;         // valid when Result == LOBBY_OK
    Pop3LobbyGameVersion LobbyGameVersion;  // valid on LOBBY_OK and LOBBY_ERR_VERSION_MISMATCH
    uint8_t              LobbyFlags;        // POP3LOBBY_FLAG_* (client can prompt for a password)
    uint8_t              Reserved;
};

struct Pop3LobbyHostRegisterV1
{
    uint8_t  ProtocolVersion;
    uint8_t  PlayerCount;   // host's CONFIRMED roster size (was Reserved; 0 from pre-count hosts). The directory trusts this over the raw joiner-leg count so denied / wrong-password / still-connecting legs never inflate the browser count.
    uint8_t  HostKey[POP3LOBBY_HOST_KEY_LEN];
    uint32_t Seed;          // gsi.Seed once the game has started (0 in the pre-game lobby). Lets the LobbySession index itself for LOBBY_RESUME_LOOKUP after a server restart.
};

struct Pop3LobbyHostRegisterRespV1
{
    uint8_t ProtocolVersion;
    uint8_t Result;   // LOBBY_OK | LOBBY_ERR_BAD_KEY | LOBBY_ERR_BAD_REQUEST
};

struct Pop3LobbyCloseV1
{
    uint8_t ProtocolVersion;
    uint8_t Reserved;
    uint8_t HostKey[POP3LOBBY_HOST_KEY_LEN];
};

struct Pop3LobbyHostPunchV1
{
    uint8_t  ProtocolVersion;
    uint8_t  Reserved;
    uint16_t JoinerServerPort;   // per-joiner proxy port the host must punch
};

struct Pop3LobbyClaimHostV1
{
    uint8_t ProtocolVersion;
    uint8_t Reserved;
};

struct Pop3LobbyClaimHostRespV1
{
    uint8_t Result;   // LOBBY_OK | LOBBY_ERR_BAD_REQUEST (host still live / claimer not a joiner)
    uint8_t Reserved;
    uint8_t HostKey[POP3LOBBY_HOST_KEY_LEN];   // fresh key for the new host (valid on LOBBY_OK)
};

// Seed rendezvous / reconnect (2026-07-13). Sent to the DIRECTORY port by any
// peer that holds the game seed (host and joiners) to re-find its relay session
// after a server restart.
struct Pop3LobbyResumeLookupV1
{
    uint8_t  ProtocolVersion;
    uint8_t  Reserved;
    uint32_t Seed;   // gsi.Seed
};

struct Pop3LobbyResumeLookupRespV1
{
    uint8_t  ProtocolVersion;
    uint8_t  Result;      // LOBBY_OK (LobbyPort valid) | LOBBY_ERR_NOT_FOUND (no live session for the seed)
    uint16_t LobbyPort;   // the live lobby port serving that seed (valid on LOBBY_OK)
};

// LOBBY_LIST_RESP header; Count records follow immediately. Responses are
// paginated (POP3LOBBY_LIST_RECORDS_PER_DATAGRAM) to stay under a typical
// 1500-byte MTU - one request may produce several datagrams; a client
// reassembles on (TotalLobbies, FirstIndex). Zero lobbies -> one datagram
// with all three counts 0.
struct Pop3LobbyListRespV1
{
    uint8_t  ProtocolVersion;
    uint8_t  Reserved;
    uint16_t TotalLobbies;
    uint16_t FirstIndex;
    uint16_t Count;
};

struct Pop3LobbyRecordV1
{
    char16_t             Name[POP3LOBBY_MAX_NAME_CHARS];
    Pop3LobbyGameVersion GameVersion;
    uint16_t             LobbyPort;
    uint8_t              PlayerCount;   // host + live joiner mappings (spectators included)
    uint8_t              MaxPlayers;
    uint8_t              Flags;         // POP3LOBBY_FLAG_*
    char                 Region[POP3LOBBY_REGION_LEN];          // ASCII, NUL-padded ("US", "UK")
    char                 OriginHost[POP3LOBBY_ORIGIN_HOST_LEN]; // server to join through (federation, G11)
    uint16_t             OriginPort;
    uint8_t              Reserved[3];
};

#pragma pack(pop)

#define POP3LOBBY_LIST_RECORDS_PER_DATAGRAM (5)   // 2 + 8 + 5*214 = 1080 bytes

// Layout asserts ARE the cross-platform compatibility test (Pop3Wire.h 2b).
static_assert(sizeof(char16_t) == 2, "wire strings are UTF-16LE");
static_assert(sizeof(Pop3LobbyGameVersion) == 4, "game version triplet layout");
static_assert(sizeof(Pop3LobbyCreateV1) == 138 && offsetof(Pop3LobbyCreateV1, Name) == 10, "create v1 layout");
static_assert(sizeof(Pop3LobbyCreateRespV1) == 20, "create resp v1 layout");
static_assert(sizeof(Pop3LobbyJoinV1) == 134 && offsetof(Pop3LobbyJoinV1, Name) == 6, "join v1 layout");
static_assert(sizeof(Pop3LobbyJoinRespV1) == 10, "join resp v1 layout");
static_assert(sizeof(Pop3LobbyHostRegisterV1) == 22, "host register v1 layout");
static_assert(sizeof(Pop3LobbyHostRegisterRespV1) == 2, "host register resp v1 layout");
static_assert(sizeof(Pop3LobbyCloseV1) == 18, "close v1 layout");
static_assert(sizeof(Pop3LobbyHostPunchV1) == 4, "host punch v1 layout");
static_assert(sizeof(Pop3LobbyClaimHostV1) == 2, "claim host v1 layout");
static_assert(sizeof(Pop3LobbyClaimHostRespV1) == 18, "claim host resp v1 layout");
static_assert(sizeof(Pop3LobbyListRespV1) == 8, "list resp v1 layout");
static_assert(sizeof(Pop3LobbyRecordV1) == 214 && offsetof(Pop3LobbyRecordV1, OriginHost) == 145, "lobby record v1 layout");
