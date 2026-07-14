#pragma once

// ============================================================================
// Pop3Wire.h - PopTB multiplayer wire format
//
// Single source of truth for every byte that crosses the wire in a network
// game. Shared by the game (via server.h / pop3.h) and by the standalone
// Pop3-Server lobby/packet router. Design/audit:
//   AI_Info/DESIGN_2026-07-12_pop3_server.md   (section 2/2b)
//   AI_Info/AUDIT_2026-07-12_headless_lobby_server.md
//
// RULES (see DESIGN section 2b):
//  - The layout is frozen to what 32-bit MSVC game builds have always sent.
//  - Fixed-width field types only. Windows is LLP64, Linux is LP64: a plain
//    `long` field silently changes size on Linux x64. The PW_* typedefs
//    below resolve to the EXACT game types on Windows (so game code keeps
//    pointer/type identity: PW_S32 IS `signed long` == SLONG) and to
//    <cstdint> fixed types elsewhere.
//  - No wchar_t on the wire: 2 bytes on Windows, 4 on Linux. PW_CH16 is
//    wchar_t on Windows (== UNICODE_CHAR, so wcscpy et al. compile
//    unchanged) and char16_t elsewhere.
//  - Every struct's size is pinned with static_asserts per build flavor.
//    A toolchain that disagrees FAILS TO COMPILE instead of desyncing.
//  - Growth is safe only because the join handshake requires exact
//    MAJOR/MINOR/BUILD equality (Pop3Network.cpp check_join_request).
// ============================================================================

#include <cstdint>

// ---- build-flavor knobs ----------------------------------------------------
// The game's Build.h sets CM_PERFORM_SYNC_CHECKING / _LARGE_ (dev builds use
// large checksums, final builds use small ones - the wire layout differs and
// the exact-build join gate keeps flavors apart). The game path picks those
// up automatically; a standalone consumer (Pop3-Server) must define the
// POP3WIRE_* knobs itself to select which game flavor it talks to.
#if !defined(POP3WIRE_SYNC_CHECKING)
#  if defined(CM_PERFORM_SYNC_CHECKING)
#    define POP3WIRE_SYNC_CHECKING        CM_PERFORM_SYNC_CHECKING
#    define POP3WIRE_LARGE_SYNC_CHECKING  CM_PERFORM_LARGE_SYNC_CHECKING
#  else
#    error Pop3Wire.h: define POP3WIRE_SYNC_CHECKING and POP3WIRE_LARGE_SYNC_CHECKING (standalone), or include after Build.h (game).
#  endif
#endif

// ---- wire-scoped types -----------------------------------------------------
#if defined(_WIN32)
typedef signed long     PW_S32;   // == game SLONG (type-identical on Windows)
typedef unsigned long   PW_U32;   // == game ULONG
typedef wchar_t         PW_CH16;  // == game UNICODE_CHAR
#else
typedef int32_t         PW_S32;
typedef uint32_t        PW_U32;
typedef char16_t        PW_CH16;
#endif
typedef unsigned char   PW_U8;    // == game UBYTE
typedef signed short    PW_S16;   // == game SWORD
typedef unsigned short  PW_U16;   // == game UWORD

static_assert(sizeof(PW_S32) == 4, "PW_S32 must be 4 bytes");
static_assert(sizeof(PW_U32) == 4, "PW_U32 must be 4 bytes");
static_assert(sizeof(PW_CH16) == 2, "PW_CH16 must be 2 bytes (never wchar_t on Linux)");
static_assert(sizeof(PW_U8) == 1 && sizeof(PW_S16) == 2 && sizeof(PW_U16) == 2, "wire base types");

// ---- wire dimensions -------------------------------------------------------
// Mirrors of game-side macros. server.h static_asserts that these equal the
// game's values, so a change on either side is a conscious, compile-checked
// wire-format decision (still guarded by the exact-build join gate).
#define POP3WIRE_GAME_NUMBER_PLAYERS      (10)  // == GAME_NUMBER_PLAYERS (8 + 1 spectator slot + 1 engine/hostbot)
#define POP3WIRE_NETWORK_NUMBER_PLAYERS   (10)  // == NETWORK_NUMBER_PLAYERS
#define POP3WIRE_MAX_LEVEL_NAME_LENGTH    (32)  // == MAX_LEVEL_NAME_LENGTH
#define POP3WIRE_FEATURE_COUNT            (25)  // == FEATURE_ENUM_MAX_SIZE - bump when adding a _FEATURE
#if POP3WIRE_LARGE_SYNC_CHECKING
#define POP3WIRE_MAX_NUM_CHECKSUMS        (8)   // == MAX_NUM_CHECKSUMS (dev flavor)
#else
#define POP3WIRE_MAX_NUM_CHECKSUMS        (1)   // == MAX_NUM_CHECKSUMS (final flavor)
#endif

#define	NUMBER_PREVIOUS_GAMETURNS         (6)   // rolling loss-recovery window in every server turn packet
#define	RESYNC_DATA_CHUNK_SIZE            (400) // resync data size

// ---- game-level packet types (first payload byte of POP_DATA frames) -------
#define	INVALID_PACKET						        (0)		// Bad packet decompression.
#define	SERVER_GAMETURN_PACKET						(1)		// This a packet from the Server.
#define CLIENT_GAMETURN_PACKET						(2)		// This a packet froma client.
#define	SERVER_GAMETURN_PACKET_NULL					(3)		// This is a packet from the server which contains
// all the players packets as PA_NONE.
#define	SERVER_GAMETURN_PACKET_NULL_EXCEPT_PLAYER	(4)		// Same as before but only one player contains
// a packet other than PA_NONE.
#define	SERVER_GAMETURN_PACKET_NULL_EXCEPT_PLAYERS	(5)		// Same as before but x players contains
// a packet other than PA_NONE but the number of packets is less
// than all maximum packets a server packet can send.
#define START_GAME_PACKET							(6)		// start game packet
#define CLIENT_CHECKIN_PACKET						(7)		// checkin packet
#define	LEVEL_INFO_PACKET							(8)		// level info packet
#define	RESYNC_DATA_PACKET							(9)		// resync data packet from server
#define	CLIENT_RESYNC_REQ_PACKET					(10)	// resync request packet from client
#define	CLIENT_LOST_GAMETURN_REQ_PACKET				(11)	// lost gameturn request from client
#define	SERVER_QUIT_GAME_PACKET						(12)	// packet from server telling client to quit
#define	SERVER_GOING_TO_QUIT_PACKET					(13)	// packet from server informing clients it is going to quit
#define	CLIENT_CAUGHT_UP_PACKET						(14)	// packet from client telling server it has reached gameturn n
#define	CLIENT_PING_REQ_PACKET						(15)	// ping req packet from client
#define	SERVER_PING_ACK_PACKET						(16)	// ping ack packet from server
#define	SERVER_PING_DATA_PACKET						(17)	// ping data packet - server tells clients about other clients' ping times

#define START_GAME_RECORD_PACKET					(18)	// start game packet - record
#define START_GAME_PLAYBACK_PACKET					(19)	// start game packet - playback
#define STOP_GAME_RECPLAY_PACKET					(20)	// stop game record/playback packet (continue game though)

#define CLIENT_HEARTBEAT_PACKET						(21)	// client heartbeat to prevent lag dropout
#define CLIENT_READY_PACKET							(22)	// ready data packet
#define CLIENT_ALLY_PACKET							(23)	// ally data packet
#define SERVER_READY_PACKET							(24)	// ready data packet
#define SERVER_ALLY_PACKET							(25)	// ally data packet
#define	CLIENT_LEVEL_DENY_PACKET					(26)	// client sends this when he hasn't got the required level
#define CLIENT_TOGGLE_ALLIANCE                      (27)    // Information packet saying the player toggled an alliance
#define CLIENT_REQUEST_SYNC_PACKET					(28)	// client asks host to send the level's referenced files
#define CLIENT_MANIFEST_PACKET						(29)	// turn-1 compat gate: client announces its PeerCompatManifest

#define	PACKET_COMPRESS_FLAG						(1<<7)	// set if packet is compressed

// ---- ready-status wire values ----------------------------------------------
#define NOT_READY (0)
#define READY (1)
#define NO_LEVEL (2)

// ---- per-turn lockstep input packet -----------------------------------------
struct NetworkPacket
{
    PW_S32 Data1;
    PW_S32 Data2;
    PW_S32 Data3;
    PW_U8  Action;
    PW_U8  Flags;

#if POP3WIRE_SYNC_CHECKING
    PW_U32 GameTurn;
#if POP3WIRE_LARGE_SYNC_CHECKING
    PW_U32 CheckSums[POP3WIRE_MAX_NUM_CHECKSUMS];
#else
    PW_U8  CheckSums[POP3WIRE_MAX_NUM_CHECKSUMS];
#endif
#endif
#ifdef POP3SAVENAME
    // Game-side cereal hook (PacketRecord2 .poprec, save/sync archives).
    // POP3SAVENAME comes from game.h; standalone consumers get plain data.
    template <class Archive>
    void serialize(Archive& ar)
    {
        ar(
            POP3SAVENAME(Data1),
            POP3SAVENAME(Data2),
            POP3SAVENAME(Data3),
            POP3SAVENAME(Action),
            POP3SAVENAME(Flags),
            POP3SAVENAME(GameTurn),
            POP3SAVENAME(CheckSums)
        );
    }
#endif
};

struct ServerNullPacket
{
    PW_U8  PacketType;
    PW_U32 GameTurn;
#if POP3WIRE_SYNC_CHECKING
    PW_U32 ValidatedGT;
#endif
};

struct ServerNullPacketExceptPlayer
{
    PW_U8  PacketType;
    PW_U32 GameTurn;
#if POP3WIRE_SYNC_CHECKING
    PW_U32 ValidatedGT;
#endif
    PW_U8  PlayerId;
    PW_U8  PacketsUsed;
    NetworkPacket Packets[NUMBER_PREVIOUS_GAMETURNS];
};

struct ServerNetworkPacket
{
    PW_U8  PacketType;
    PW_U32 GameTurn;
#if POP3WIRE_SYNC_CHECKING
    PW_U32 ValidatedGT;
#endif
    NetworkPacket Packets[NUMBER_PREVIOUS_GAMETURNS][POP3WIRE_GAME_NUMBER_PLAYERS];
};

struct ClientNetworkPacket
{
    PW_U8  PacketType;
    PW_U8  PlayerNumber;
    PW_U32 GameTurn;
    NetworkPacket Packet;
};

// D5-01: per-component CRC-32 identity of the host's sync-critical content.
// Rides inside ServerLevelPacket; also cached game-side (gnsi.HostLevelCrcs).
struct LevelSyncCrcs
{
    unsigned int Dat;        // levl2NNN.dat bytes
    unsigned int Hdr;        // levl2NNN.hdr bytes (legacy v2 header), 0 if absent
    unsigned int CpScripts;  // legacy CP scripts the header references (CPSCRnnn.DAT)
    unsigned int Script4;    // script4 files the v3 header references (scripts/<name>)
    unsigned int Constants;  // levels/constant.dat (P3CONST_ balance table)
    unsigned int ObjectBank; // objects/objs0-<c>.dat picked by header ObjectsBankNum
                             // (sim: building placement/footprint, creature altitude)
};

// Turn-1 compatibility manifest (2026-07-13 beta incident: the lobby let
// incompatible peers start and every player OOS'd on turn 1). Everything a
// peer must agree on BEFORE the host may start, beyond the exact-build join
// gate - which cannot see the game's CM_ compile flags (Beta and Release,
// with or without ONLINE_CHEATS, share one platform-lib config byte), the
// data-file CRCs or the feature bits. Every non-host client announces its
// manifest on the 500ms lobby cadence (ClientManifestPacket); the host's
// own manifest rides at the end of ServerLevelPacket. The host refuses
// START on any HARD-axis mismatch; OptionsSig is DIAGNOSTIC-ONLY (the
// START packet force-pushes lobby options, so a pre-start mismatch there
// is transient by design and must never hard-gate).

// PeerCompatManifest.BuildConfig - game-provided compile config. The
// transport's join byte (POP3_BUILD_CONFIG_ID) only sees Debug-vs-Release;
// this one distinguishes all three game configs.
#define PCM_CONFIG_DEVELOP           (1)
#define PCM_CONFIG_BETA              (2)
#define PCM_CONFIG_RELEASE           (3)

// PeerCompatManifest.ConfigFlags - sim-affecting compile toggles that two
// same-flavor builds can still disagree on. The GAME folds these from its
// Build.h (fill_peer_compat_manifest, Level.cpp); the platform lib cannot
// see CM_*.
#define PCM_FLAG_ONLINE_CHEATS       (1<<0)  // CM_ONLINE_CHEATS (cheats live online)
#define PCM_FLAG_SCRIPT_BOUNDS_CHECK (1<<1)  // CM_SCRIPT_BOUNDS_CHECK (script3 interpreter)
#define PCM_FLAG_DEBUG_RANDOM_SEED   (1<<2)  // CM_DEBUG_RANDOM_SEED (extra PRNG draws)
#define PCM_FLAG_DEBUG_DESYNC        (1<<3)  // DEBUG_DESYNC (Io/Snapshots instrumentation)
#define PCM_FLAG_SP_SYNC_CHECKING    (1<<4)  // CM_SINGLE_PLAYER_SYNC_CHECKING

struct PeerCompatManifest
{
    PW_U8  Major;        // MAJOR_VERSION
    PW_U8  Minor;        // MINOR_VERSION
    PW_U16 Build;        // BUILD_NUMBER
    PW_U8  ArchBits;     // 8 * sizeof(void*): 32 or 64
    PW_U8  BuildConfig;  // PCM_CONFIG_*
    PW_U8  ConfigFlags;  // PCM_FLAG_* bitfield
    PW_U8  Pad;
    PW_U32 FeatureBits;  // FeatureSystem bits 0..POP3WIRE_FEATURE_COUNT-1
    struct LevelSyncCrcs Crcs; // level identity (level_sync_crcs); all 0 = no level yet
    PW_S16 Level;        // lobby level number (-1 = none announced yet)
    PW_U16 Pad2;
    PW_U32 OptionsSig;   // CRC of lobby options - diagnostic only, NEVER hard-gated
};

struct ClientManifestPacket
{
    PW_U8 PacketType;
    PW_U8 Pad[3];
    struct PeerCompatManifest Manifest;
};

struct ServerLevelPacket
{
    PW_U8   PacketType;
    PW_S16  LevelNumber;
    PW_CH16 LevelName[POP3WIRE_MAX_LEVEL_NAME_LENGTH];
    PW_U32  AvailableSpells;
    PW_U32  AvailableBuildings;
    PW_U8   OtherLevelInfo;
    PW_U8   ShamanLives;
    PW_U8   FeatureSettings[POP3WIRE_FEATURE_COUNT];
    PW_U8   AllyStatus[POP3WIRE_NETWORK_NUMBER_PLAYERS];
    PW_U8   ReadyStatus[POP3WIRE_NETWORK_NUMBER_PLAYERS];
    PW_S32  Seed;
    PW_U16  BuildingLimit;
    PW_U16  PopulationLimit;
    // LevelCrcs.Dat == 0 = host has no local level file (e.g. headless
    // hostbot) -> clients skip the compare. Wire-format growth is safe: the
    // join handshake requires exact MAJOR/MINOR/BUILD equality.
    struct LevelSyncCrcs LevelCrcs;
    // Turn-1 compat gate: the HOST's own manifest, compared client-side on
    // the axes that cannot self-heal (belt-and-braces vs a stale host).
    struct PeerCompatManifest HostManifest;
};

struct ResyncDataPacket
{
    PW_U8  PacketType;
    PW_U32 Offset;
    PW_U32 ChunkPos;
    PW_U8  Data[RESYNC_DATA_CHUNK_SIZE];
};

struct ResyncReqPacket
{
    PW_U8  PacketType;
    PW_U32 Offset;
};

struct LostGTReqPacket
{
    PW_U8  PacketType;
    PW_U32 GameTurn;
};

struct ServerQuittingPacket
{
    PW_U8  PacketType;
    PW_U32 GameTurn;
};

struct ClientLevelDenyPacket
{
    PW_U8  PacketType;
    PW_U32 Level;
};

struct ClientRequestSyncPacket
{
    PW_U8  PacketType;
    PW_U32 Level;
};

struct PingPacket
{
    PW_U8 PacketType;
    PW_U8 PingId;
};

struct PingDataPacket
{
    PW_U8  PacketType;
    PW_U16 PingTime[POP3WIRE_NETWORK_NUMBER_PLAYERS];
};

struct ClientChangeReqPacket
{
    PW_U8 PacketType;
    PW_U8 Status;
};

struct ServerChangePacket
{
    PW_U8 PacketType;
    PW_U8 Status[POP3WIRE_NETWORK_NUMBER_PLAYERS];
};

// ---- layout pins ------------------------------------------------------------
// Sizes as produced by 32-bit MSVC natural alignment - the wire truth. Any
// compiler/platform/arch that disagrees fails HERE, not on the wire.
static_assert(sizeof(LevelSyncCrcs) == 24, "LevelSyncCrcs wire size");
static_assert(sizeof(PeerCompatManifest) == 44, "PeerCompatManifest wire size");
static_assert(sizeof(ClientManifestPacket) == 48, "ClientManifestPacket wire size");
static_assert(sizeof(PingPacket) == 2, "PingPacket wire size");
static_assert(sizeof(PingDataPacket) == 22, "PingDataPacket wire size");
static_assert(sizeof(ClientChangeReqPacket) == 2, "ClientChangeReqPacket wire size");
static_assert(sizeof(ServerChangePacket) == 11, "ServerChangePacket wire size");
static_assert(sizeof(ResyncReqPacket) == 8, "ResyncReqPacket wire size");
static_assert(sizeof(LostGTReqPacket) == 8, "LostGTReqPacket wire size");
static_assert(sizeof(ServerQuittingPacket) == 8, "ServerQuittingPacket wire size");
static_assert(sizeof(ClientLevelDenyPacket) == 8, "ClientLevelDenyPacket wire size");
static_assert(sizeof(ClientRequestSyncPacket) == 8, "ClientRequestSyncPacket wire size");
static_assert(sizeof(ResyncDataPacket) == 412, "ResyncDataPacket wire size");
static_assert(sizeof(ServerLevelPacket) == 200, "ServerLevelPacket wire size");

#if POP3WIRE_SYNC_CHECKING && POP3WIRE_LARGE_SYNC_CHECKING
static_assert(sizeof(NetworkPacket) == 52, "NetworkPacket wire size (dev flavor)");
static_assert(sizeof(ServerNullPacket) == 12, "ServerNullPacket wire size (dev flavor)");
static_assert(sizeof(ServerNullPacketExceptPlayer) == 328, "ServerNullPacketExceptPlayer wire size (dev flavor)");
static_assert(sizeof(ServerNetworkPacket) == 3132, "ServerNetworkPacket wire size (dev flavor)");
static_assert(sizeof(ClientNetworkPacket) == 60, "ClientNetworkPacket wire size (dev flavor)");
#elif POP3WIRE_SYNC_CHECKING
static_assert(sizeof(NetworkPacket) == 24, "NetworkPacket wire size (final flavor)");
static_assert(sizeof(ServerNullPacket) == 12, "ServerNullPacket wire size (final flavor)");
static_assert(sizeof(ServerNullPacketExceptPlayer) == 160, "ServerNullPacketExceptPlayer wire size (final flavor)");
static_assert(sizeof(ServerNetworkPacket) == 1452, "ServerNetworkPacket wire size (final flavor)");
static_assert(sizeof(ClientNetworkPacket) == 32, "ClientNetworkPacket wire size (final flavor)");
#endif
