#pragma once
#include "../../../../Pop3.h"
#include "Pop3Types.h"
#include "Pop3LobbyProtocol.h"
#include <list>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <vector>
#include <atomic>
#include <thread>
#include <array>
#include <cstdint>

extern Poco::Mutex packet_info_mu;

// These are the callback types.
#define	NET_CALLBACK	POP3_CALLBACK
typedef void (NET_CALLBACK *NetworkServicesCallbackProc)(void* connection, UNICODE_CHAR* lpDriverDescription, GUID* guiddatatype, DWORD noguiddatatypes, void* param);
typedef void (NET_CALLBACK *NetworkDataCallbackProc)(DWORD from_id, void* dataPtr, DWORD dataSize, DWORD messageType, const void* param);
typedef void (NET_CALLBACK *NetworkLobbyApplicationsCallback)(void* dataPtr, void* param);
typedef void (NET_CALLBACK *NetworkMediumCallback)(UNICODE_CHAR* medium, void* param);
typedef void (NET_CALLBACK *NetworkPingCallback)(WORD id, WORD seq, DWORD rondtriptime, void* param);

// Lobby password. Persisted in gnsi.SafeNet.Password (net.cfg); the transport
// never sends the password in clear. The host issues a per-join random nonce
// in HOST_ACCEPT_JOIN and the client returns SHA1(nonce || password) appended
// to CLIENT_JOIN; the host recomputes and compares. See check_join_request /
// SendMyInfo / the CLIENT_JOIN handler (2026-07-12 P2).
#define POP3NETWORK_MAX_PASSWORD_LENGTH     (32)
#define POP3NETWORK_PW_NONCE_LEN            (8)    // random challenge bytes
#define POP3NETWORK_PW_HASH_LEN            (20)    // SHA-1 digest length
#define POP3NETWORK_PW_NONCE_TTL_MS      (30000)   // issued-nonce lifetime
#define POP3NETWORK_PW_MAX_NONCES          (64)    // cap the pending-nonce map (anti-DoS)
// HOST_DENY_JOIN reason byte (payload[0])
#define POP3NETWORK_DENY_VERSION            (0)
#define POP3NETWORK_DENY_PASSWORD           (1)
#define POP3NETWORK_DENY_FULL               (2)   // lobby size cap reached (3c; spectators exempt)
#define POP3NETWORK_DENY_NAME               (3)   // player name already in use by a different peer
#define	POP3NETWORK_MAX_SESSION_NAME_LENGTH (64)
#define	POP3NETWORK_MAX_PLAYER_NAME_LENGTH  (32)
#define	NET_ALLPLAYERS						(0xffffffff)
#define	NET_SERVERPLAYERS					(0xfffffffe)
#define CHAT_ALL                            (0xff)
#define CONNECT_INTERVAL                    (2)
#define CONNECT_ATTEMPTS                    (10)
#define DATA_NOT_SET                        (-1)
#define NET_DEFAULT_PORT                    (7575)
#define POP_PACKET_TYPE                     (10)
#define MAX_PACKET_SIZE                     (3206)
#define MAX_ADDRESS                         (50)
#define	POP3NETWORK_PLAYERCAPS_SPECTATOR	(1<<2)

enum class Pop3NetworkTypes
{
    VERSION_DATA,
    CLIENT_JOIN_REQUEST,
    CLIENT_JOIN,
    CLIENT_QUIT,
    HOST_ACCEPT_JOIN,
    HOST_DENY_JOIN,
    HOST_PLAYERS,
    HOST_ADD_PLAYER,
    HOST_DELETE_PLAYER,
    POP_DATA,
    POP_CHAT,
    HOST_READY_FOR_FILE_TRANSFER,
    CLIENT_READY_FOR_FILE_TRANSFER,
    HOST_SEND_FILE_PART,
    HOST_SEND_FILE_TRANSFER_COMPLETE,
    CLIENT_FILE_TRANSFER_COMPLETE,
    CLIENT_RESEND_FILE_PART,
    CLIENT_RESNET_FILE_PARTS_COMPLETE,
    HOST_REQUESTING_UPDATE_ON_TRANSFER,
    // Host -> client: there is no (longer an) active transfer for you - the
    // host aborted (retry cap) or was restarted. The client resets its FT
    // state to Ready so the lobby UI unsticks and the Sync button returns.
    // Appended at the enum tail: wire-compatible within a build, and MP
    // already requires identical builds (version-gated join).
    HOST_TRANSFER_ABORTED
};

enum class Pop3ErrorStatusCodes
{
    OK,
    SINGLE_PLAYER,
    HOSTING,
    JOINED,
    TIMEOUT,
    BAD_VERSION,
    BAD_PASSWORD,
    BAD_NAME,
    NET_INIT,
    NET_SOCKET,
    NET_BIND
};

enum class NetworkStatus
{
    Disconnected,
    Connected,
    Joining,
};

typedef struct
{
    BYTE inUse : 4;
    BYTE host : 4;

    DWORD dwFlags;
    BYTE slotNo;
    UNICODE_CHAR name[MAX_PLAYER_NAME_LEN];
    SWORD uniquePlayerId;
    UWORD port;
    char address[MAX_ADDRESS];
    bool blocked;
} POP3NETWORK_PLAYERINFO;

struct POP3NETWORK_GAMEDATA
{
    SBYTE*          PlayerNum;
    char*           RemoteIPAddress;
    UBYTE*          Allies;
    UBYTE*          CurrNumPlayers;
    UBYTE*          RequestedPlayerNum;
    UNICODE_CHAR*   name;
    UWORD*          RemotePort;
    UWORD*          LocalPort;
    UNICODE_CHAR*   Password;   // lobby password (may be null / empty = open)
    SWORD*          PingTime;
    SWORD*          StartLevelNumber;
    SWORD*          NumPeopleOfType;
    SWORD*          NumBuildingsOfType;
    UBYTE*          SpellsCast;
    ULONG*          GnsiFlags;
    ULONG*          GameTurn;
    ULONG*          BuildingsAvailable;
    ULONG*          PlayerFlags;
    SLONG*          StatValues;
    SLONG*          frame_rate_draw;
    void*           PlayerMsg;
    void*           spells_type_info;
    size_t          PlayerStructSize;
    UBYTE*          GameState;
    UBYTE*          GameMode;
    ULONG*          NetMyPlayerNumber;
    UBYTE*          NetLobbied;
    // Lobby size cap (2-8) chosen at create time; 0 = uncapped (all 8 game
    // slots). Enforced host-side in check_join_request; spectators exempt.
    UBYTE           MaxPlayers;
    std::function<void(void)>   FileTransfer_Callback;
};

typedef struct
{
    DWORD dwVersion; // This is the version number.
    GUID guidInstance; // In some SPs this can be
    GUID guidApplication; // decoded to get the network
    DWORD dwTotalMaxPlayers; // address and it's port or socket.
    DWORD dwTotalCurrentPlayers;
    DWORD dwMaxNoPlayers; // Max no of players allowed to play game.
    DWORD dwMaxNoSpectators; // Max no of spectators allowed to join.
    DWORD dwCurrentNoPlayers; // Number of players in game.
    DWORD dwCurrentNoSpectators; // Number of Spectators in the game.
    DWORD dwFlags;
    DWORD dwData[4]; // For user data.
    BYTE dwReserved[20]; // Reserved data do not change.
    UNICODE_CHAR sessionName[POP3NETWORK_MAX_SESSION_NAME_LENGTH];
} POP3NETWORK_SESSIONDESC;

class PacketInfo
{
public:
    DWORD packetSize;
    std::string peer_address;
    UWORD port;
    int from_id;
    int to_id;
    int packet_num;
    unsigned long long time;
    bool sending_packet;
    bool packet_sent;
    std::string extra_data;

    PacketInfo()
    {
        packetSize = 0;
        port = 0;
        from_id = -1;
        to_id = -1;
        packet_num = -1;
        time = 0;
        packet_sent = false;
        sending_packet = false;
    }
};

#define FILE_NAME_PACKET_SIZE (MAX_PACKET_SIZE - sizeof(size_t) - 2)
#define PART_PACKET_DATA_SIZE (MAX_PACKET_SIZE - sizeof(unsigned int) - 2)

struct PopTBFileStartPacket
{
    size_t              file_size;
    char                file_name[FILE_NAME_PACKET_SIZE];
};

struct PopTBFilePartPacket
{
    unsigned int        packet_num;
    char                data[PART_PACKET_DATA_SIZE];
};

struct PopTBFilePartRequest
{
    unsigned int        packet_num;
};

// Sent by host as payload of HOST_SEND_FILE_TRANSFER_COMPLETE to mark end of a window.
struct PopTBWindowComplete
{
    unsigned int        window_start;
    unsigned int        window_end;     // exclusive
};

// Sent by client as payload of CLIENT_RESEND_FILE_PART — bitmap ACK for a window.
#define FT_MAX_BITMAP_INTS 8  // supports windows up to 256 packets
struct PopTBWindowAck
{
    unsigned int        window_start;
    unsigned int        window_size;
    unsigned int        received_bitmap[FT_MAX_BITMAP_INTS];
};

// Sliding window constants
#define FT_INITIAL_WINDOW_SIZE      32
#define FT_MIN_WINDOW_SIZE          4
#define FT_MAX_WINDOW_SIZE          (FT_MAX_BITMAP_INTS * 32)  // 256
#define FT_INTER_PACKET_SLEEP_MS    1
#define FT_WINDOW_TIMEOUT_MS        3000
#define FT_MAX_RETRIES              20

enum class FileTransferStatus
{
    Ready,
    Host_Waiting_On_Client_Ack,
    Host_Sending_Window,
    Host_Waiting_On_Window_Ack,
    Host_Waiting_On_Client_To_Finish_Transfer,
    Client_Waiting_On_Host_To_Start,
    Client_Receiving_Window,
    Transfer_Complete
};

struct FileTransfer
{
    std::string                                     peer_address;
    UWORD                                           peer_port = 0;
    ULONGLONG                                       LastContactTime = 0;
    PopTBFileStartPacket                            FileHeader = {};
    std::vector<PopTBFilePartPacket>                FileParts;
    std::map<unsigned int, PopTBFilePartPacket>     FilePartsRecv;
    FileTransferStatus                              Status = FileTransferStatus::Ready;
    ULONGLONG                                       SleepTimer = 0;
    ULONGLONG                                       TotalByteSent = 0;

    // Sliding window state
    unsigned int        window_start = 0;
    unsigned int        window_size = FT_INITIAL_WINDOW_SIZE;
    unsigned int        total_parts = 0;
    unsigned int        retry_count = 0;
    ULONGLONG           window_send_time = 0;

    // Recipient network id (host side; -1 when idle / client side). Captured
    // at transfer_file() time together with the endpoint so every retry and
    // status-query path has somewhere to send - previously peer_address was
    // only learned from the client's READY packet, so losing the FIRST
    // header datagram left the host retrying into an empty address forever
    // (and the transfer slot busy for the rest of the session).
    SWORD               peer_id = -1;
};

class Pop3Network
{
public:
    Pop3Network();
    virtual ~Pop3Network();

    // Lobby Management.
    Pop3ErrorStatusCodes                            AreWeLobbied(NetworkDataCallbackProc theCallback, UBYTE start_mode, UNICODE_CHAR* player_name);

    // Player managment Functions.
    BOOL                                            GetPlayerInfo(POP3NETWORK_PLAYERINFO* player);
    void                                            EnableNewPlayers(BOOL allow_new_players);
    void                                            RemovePlayer(SWORD player_id);
    void                                            initGamePtrs(const struct POP3NETWORK_GAMEDATA & gd);

    // Data managment Functions.
    int                                             SendData(DWORD to_id, void* dataPtr, DWORD size);
    int                                             SendChat(BYTE chat_targets, const UNICODE_CHAR* message);
    void                                            DestroySession();

    // Misc Functions.
    static ULONGLONG                                GetCurrentMs();
    POP3NETWORK_PLAYERINFO &                        GetPlayerDetails(UBYTE playernum);
    // Copy a peer's name under players_mu into a caller buffer. Use this instead
    // of GetPlayerDetails().name when the id may be a spectator (map node can be
    // erased on the network thread by the reaper / HOST_DELETE_PLAYER).
    void                                            GetPlayerNameCopy(UBYTE playernum, UNICODE_CHAR* out, size_t out_len);
    int                                             GetPlayerCount();
    int                                             GetGamePlayerCount();   // real players only (ids < NETWORK_NUMBER_PLAYERS)
    bool                                            am_i_host() const;
    const NetworkStatus                             getStatus() const;
    // Last join-deny reason: -1 = none, else POP3NETWORK_DENY_*. Sticky across
    // the watchdog's server_status resets so the lobby UI can show "wrong
    // password" rather than a generic timeout.
    int                                             getJoinDenyReason() const { return m_join_deny_reason.load(); }
    const int                                       getRetries() const;
    const std::list<class PacketInfo> &             getPacketInfo();
    static const std::string                        tribes[];

    // --- Mode A lobby-server registration (3c) ------------------------------
    // Arms periodic LOBBY_HOST_REGISTER keepalives from the GAME socket to the
    // per-lobby proxy port (NAT-safe host association - the LobbySession
    // records the datagram's source as the host endpoint) and a key-gated
    // LOBBY_CLOSE at teardown. MUST be called after construction and BEFORE
    // AreWeLobbied() starts the receive thread - the fields are read from
    // that thread with no lock.
    void                                            SetLobbyRegistration(const char* lobbyHost, UWORD lobbyPort,
                                                                         const uint8_t* hostKey /*[POP3LOBBY_HOST_KEY_LEN]*/);
    // RTT to the lobby relay, measured on every keepalive/REGISTER_RESP pair
    // (~10s cadence). -1 until the first response. In a proxied game this IS
    // the host's real latency - its loopback "1ms" ping is a lie to everyone.
    int                                             getLobbyServerPingMs() const { return m_lobby_ping_ms.load(); }

    // Relay liveness (Phase 1, 2026-07-13): true when we are the connected HOST
    // of a federated lobby whose LOBBY_HOST_REGISTER keepalives have gone
    // unanswered long enough that the Pop3-Server relay is presumed dead
    // (restart / offline). The game polls this on the connected-lobby screen to
    // surface a "lost connection to server" dialog instead of sitting in a lobby
    // whose chat/relay is silently black-holed. Joiners are covered by the
    // existing peer-silence timeout (check_network_timeouts). Not stale while
    // claiming (host migration) or before the first keepalive was armed.
    bool                                            LobbyKeepaliveStale();

    // Host migration (Mode A): a surviving joiner claims the dead host's proxy
    // slot. BeginHostClaim points registration at the lobby endpoint and starts
    // periodic LOBBY_CLAIM_HOST from the game socket; on grant the net thread
    // adopts the fresh host key, arms registration + am_host, and raises
    // HostClaimSucceeded() for the game to poll (-> finish_host_migration).
    // AbortHostClaim ends a stalled attempt so the normal disconnect can run.
    void                                            BeginHostClaim(const char* lobbyHost, UWORD lobbyPort);
    // Also rolls back a host role a just-landed grant may have adopted (TOCTOU:
    // the net thread can apply the grant between the game's HostClaimSucceeded()
    // check and this abort), so the survivor can't strand as an armed host it
    // then force-quits. A claiming peer was never a legit host, so this is safe.
    void                                            AbortHostClaim() { m_claiming.store(false); m_claim_succeeded.store(false); m_lobbyreg_armed = false; am_host = false; }
    bool                                            HostClaimSucceeded() const { return m_claim_succeeded.load(); }

    // Host left a hosted federated game: ask the receive thread to emit one
    // LOBBY_CLOSE and disarm the keepalive so the lobby reaps immediately (no
    // zombie). No-op for joiners / non-federated hosts (armed only for a host).
    void                                            RequestLobbyClose() { m_lobby_leave_requested.store(true); }

    void                                            transfer_file(DWORD to, const std::string & file_name, const char * data, size_t length);
    FileTransferStatus                              getFileTransferStatus();
    // Percent is side-aware: receiving side = parts received / total; sending
    // side = window progress through the parts (previously always 0 for the
    // host because it counted FilePartsRecv, which only the receiver fills).
    unsigned int                                    getFileTransferPercent();
    size_t                                          getFileTransferTotalBytes();
    size_t                                          getFileTransferRecievedBytes();
    std::string                                     getFileTransferName();
    ULONGLONG                                       getFileTransferSleepTimer();
    SWORD                                           getFileTransferPeerId();   // host side: recipient network id (-1 idle)
protected:
    // Data
    int                                             player_num;
    int                                             connection_retries;
    bool                                            am_host;
    struct POP3NETWORK_GAMEDATA                     GamePtrs;
    NetworkDataCallbackProc                         ProcessPopData;
    Pop3ErrorStatusCodes                            server_status;
    std::wstring                                    my_name;
    enum NetworkStatus                              network_status;
    bool                                            allow_joiners;
    Poco::Condition                                 cv2;
    std::map<std::wstring, POP3NETWORK_PLAYERINFO>  players;
    mutable Poco::Mutex                             players_mu;
    mutable std::list<class PacketInfo>             packets;

    // Watchdog/receive-loop shutdown flag: set by shutdown_watchdogs(); also
    // meant to be polled by the derived Pop3NetworkUDP receive loop.
    std::atomic<bool>                               m_shutdown{ false };
    std::thread                                     m_host_watchdog;
    std::thread                                     m_join_watchdog;
    bool                                            m_join_first_time = true;

    // --- lobby password (P2) - all touched only on the network thread -------
    // HOST: nonces issued to joining peers, keyed "addr:port" -> (nonce, ms).
    // Const check_join_request inserts here, hence mutable.
    mutable std::map<std::string, std::pair<std::array<uint8_t, POP3NETWORK_PW_NONCE_LEN>, ULONGLONG>> m_join_nonces;
    // CLIENT: hash to append to CLIENT_JOIN once the host's nonce arrived.
    std::array<uint8_t, POP3NETWORK_PW_HASH_LEN>    m_join_pw_hash{};
    bool                                            m_join_pw_pending = false;
    // CLIENT: sticky last-deny reason, cleared to -1 per join attempt in
    // AreWeLobbied; the watchdog resets server_status so the UI reads this
    // instead. -1 = none, else POP3NETWORK_DENY_* (0 = version, so -1 keeps
    // "not denied" distinct from a version deny).
    std::atomic<int>                                m_join_deny_reason{ -1 };

    bool                                            password_required() const;
    static void                                     compute_pw_hash(const uint8_t* nonce, size_t nonce_len,
                                                                    const UNICODE_CHAR* password, uint8_t* out /*[POP3NETWORK_PW_HASH_LEN]*/);

    // --- Mode A lobby-server registration (3c) - written once on the main
    // thread before the receive thread exists, then read only by that thread
    // (keepalive tick) and the destructor (close frame, after thread join).
    bool                                            m_lobbyreg_armed = false;
    char                                            m_lobbyreg_host[MAX_ADDRESS] = {};
    UWORD                                           m_lobbyreg_port = 0;
    uint8_t                                         m_lobbyreg_key[POP3LOBBY_HOST_KEY_LEN] = {};
    ULONGLONG                                       m_lobbyreg_last_ms = 0;
    ULONGLONG                                       m_lobbyreg_sent_ms = 0;      // network thread only
    std::atomic<int>                                m_lobby_ping_ms{ -1 };       // read by game threads
    // Last LOBBY_HOST_REGISTER_RESP arrival (ms). Written by the net thread on
    // each RESP and seeded by SetLobbyRegistration; read by the game thread in
    // LobbyKeepaliveStale to detect a dead relay. Atomic for that cross-thread read.
    std::atomic<ULONGLONG>                          m_lobbyreg_last_resp_ms{ 0 };

    // Host migration (Mode A): state for a surviving joiner claiming the host slot.
    std::atomic<bool>                               m_claiming{ false };         // shared: game (Begin/Abort) + net (claim/grant)
    ULONGLONG                                       m_claim_last_ms = 0;         // network thread only
    std::atomic<bool>                               m_claim_succeeded{ false };  // net thread -> polled by game
    std::atomic<bool>                               m_lobby_leave_requested{ false }; // game -> net: LOBBY_CLOSE + disarm now
    int                                             m_lobby_close_left = 0;           // net thread: LOBBY_CLOSE resends remaining (UDP is lossy)

    void                                            ParsePacket(char* buffer, DWORD buf_size, const char* peer_address, UWORD peer_port);
    SWORD                                           add_player(const char* peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR* player_name);
    void                                            SendPointers(const char* peer_address, UWORD peer_port);
    void                                            send_join_request() const;
    void                                            add_packet_info(const PacketInfo & pi) const;
    // Signals the watchdog threads (and receive loop) to exit, then joins the
    // watchdogs. MUST be called from the most-derived destructor
    // (Pop3NetworkUDP::~Pop3NetworkUDP) while the virtual Send() is still
    // valid; ~Pop3Network() only calls it as a last-resort fallback.
    void                                            shutdown_watchdogs();
private:
    FileTransfer FT;
    // Guards ALL FT access (main-thread transfer_file/getters vs the network
    // thread's filetransfer_* handlers and filetransfer_tick). Lock order:
    // ft_mu is taken BEFORE players_mu (via Send); never the reverse.
    mutable Poco::Mutex ft_mu;

    // Host-only spectator liveness tracking. Spectators (uniquePlayerId >=
    // NETWORK_NUMBER_PLAYERS) live only in the peer `players` map, OUTSIDE the
    // fixed 0..N-1 dropout loop, so a crashed spectator would otherwise linger
    // forever and the host would relay to a dead address. Touched only on the
    // host network thread (ParsePacket) - kept out of POP3NETWORK_PLAYERINFO so
    // the shared gnsi.Net.PlayerInfo[] layout is unchanged.
    std::map<SWORD, ULONGLONG> m_spectator_last_contact;
    ULONGLONG                  m_last_spectator_sweep_ms = 0;

    // Utility Functions
    void                                            send_remove_player(SWORD player_id);
    void                                            remove_player_impl(int player_id);
    int                                             ProcessChat(const char* buf, int buf_len);
    int                                             SendChatToAll(const char* buf, int buf_len);
    SWORD                                           get_player_id(const char* peer_address, UWORD peer_port);
    void                                            check_join_request(const char* peer_address, UWORD peer_port, const char* buffer) const;
    void                                            send_players(int to_id);
    void                                            send_add_player(SWORD player_id);
    void                                            SendMyInfo(const char* peer_address, UWORD peer_port) const;
    void                                            add_players(const char* peer_address, UWORD peer_port, char* buffer, DWORD payload_size);
    void                                            Send(int to_id, Pop3NetworkTypes type);
    void                                            Send(int to_id, Pop3NetworkTypes type, const char* buffer, DWORD buf_size);
    void                                            Send(const char* peer_address, UWORD peer_port, Pop3NetworkTypes type) const;
    void                                            host_watchdog_func();
    void                                            join_watchdog_func();
    void                                            compile_fileparts();

    // File Transfers — sliding window protocol
    void                                            filetransfer_client_process_fileheader(const char * peer_address, UWORD peer_port, const char * buffer);
    void                                            filetransfer_host_process_client_ready(const char * peer_address, UWORD peer_port);
    void                                            filetransfer_client_process_file_part(const char * buffer);
    void                                            filetransfer_client_process_window_complete(const char * buffer);
    void                                            filetransfer_host_process_window_ack(const char * buffer);
    void                                            filetransfer_host_process_client_transfer_successful();
    void                                            filetransfer_host_send_window();
    void                                            filetransfer_host_send_missing(const unsigned int * bitmap, unsigned int ws, unsigned int wsize);
    void                                            filetransfer_host_requesting_update(const char * peer_address, UWORD peer_port);
    void                                            filetransfer_client_process_aborted();
    void                                            filetransfer_abort_locked(const char * reason);   // ft_mu held: notify peer + reset to Ready
protected:
    void                                            filetransfer_tick();

    // Network Layer Functions
    virtual void                                    ServerInit(UBYTE mode) = 0;
    virtual void                                    Send(int to_pn, const char* peer_address, UWORD peer_port, Pop3NetworkTypes type, const char* buffer, DWORD buf_size) const = 0;
};