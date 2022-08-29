#pragma once
#include "../../../../Pop3.h"
#include <Windows.h>
#include "Pop3Types.h"
#include <thread>
#include <condition_variable>
#include <vector>
#include <list>
#include <functional>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>

extern Poco::Mutex packet_info_mu;

#define	POP3NETWORK_MAX_LEVEL_NAME_LENGTH			(64)
#define	POP3NETWORK_MAX_LOBBY_NAME_LENGTH			(64)
#define	POP3NETWORK_MAX_SCOREDNS_NAME_LENGTH		(64)
#define	POP3NETWORK_MAX_SESSION_NAME_LENGTH		    (64)
#define	POP3NETWORK_MAX_PLAYER_NAME_LENGTH		    (32)
#define POP3NETWORK_MAX_PASSWORD_LENGTH			    (1)   // lol
#define	POP3NETWORK_PLAYERCAPS_SPECTATOR			(1<<2)
#define	POP3NETWORK_ALLPLAYERS					    (0xffffffff)
#define POP3NETWORK_SERVERPLAYER					(0xfffffffe)

typedef struct
{
    char szLobbyName[POP3NETWORK_MAX_LOBBY_NAME_LENGTH];
    char szLevelName[POP3NETWORK_MAX_LEVEL_NAME_LENGTH];
    char szPlayerName[POP3NETWORK_MAX_PLAYER_NAME_LENGTH];
    char szScoringDomainAddress[POP3NETWORK_MAX_SCOREDNS_NAME_LENGTH];
} POP3NETWORK_LOBBYINFO;

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

#define MAX_ADDRESS (50)

typedef struct
{
    BYTE inUse : 4;
    BYTE host : 4;

    DWORD dwFlags;
    BYTE slotNo;
    UNICODE_CHAR name[MAX_PLAYER_NAME_LEN];
    SWORD uniquePlayerId;
    SWORD port;
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
    SWORD*          PingTime;
    SWORD*          StartLevelNumber;
    SWORD*          NumPeopleOfType;
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
    std::function<void(std::string&)> Script_Callback;
};

#define	NET_CALLBACK	__stdcall
// These are the callback types.
typedef void (NET_CALLBACK *NetworkServicesCallbackProc)(void* connection, UNICODE_CHAR* lpDriverDescription, GUID* guiddatatype, DWORD noguiddatatypes, void* param);
typedef void (NET_CALLBACK *NetworkDataCallbackProc)(DWORD from_id, void* dataPtr, DWORD dataSize, DWORD messageType, const void* param);
typedef void (NET_CALLBACK *NetworkLobbyApplicationsCallback)(void* dataPtr, void* param);
typedef void (NET_CALLBACK *NetworkMediumCallback)(UNICODE_CHAR* medium, void* param);
typedef void (NET_CALLBACK *NetworkPingCallback)(WORD id, WORD seq, DWORD rondtriptime, void* param);

#define	NET_VERSION_DATA      		(0)
#define	NET_CLIENT_REQUEST_JOIN		(1)
#define	NET_CLIENT_JOIN				(2)
#define	NET_CLIENT_QUIT				(3)
#define	NET_HOST_ACCEPT_JOIN		(4)
#define	NET_HOST_DENY_JOIN		    (5)
#define	NET_HOST_PLAYERS			(6)
#define	NET_HOST_ADD_PLAYER			(7)
#define	NET_HOST_DELETE_PLAYER		(8)
#define	NET_POP_DATA				(10)
#define	NET_POP_CHAT				(11)
#define NET_POP_SCRIPT              (12)

#define	NET_ALLPLAYERS						(0xffffffff)
#define	NET_SERVERPLAYERS					(0xfffffffe)
#define CHAT_ALL (0xff)


#define NET_DEFAULT_PORT  (7575)

#define POP_PACKET_TYPE  (10)
#define MAX_PACKET_SIZE (3202)  //<RW> increase the max packet size to 768, with the extra sync checking, we need larger packets

#define NET_OK (0)
#define NET_SINGLE_PLAYER (1)
#define NET_HOSTING (2)
#define NET_JOINED (3)
#define NET_TIMEOUT (4)
#define NET_BAD_VERSION (5)
#define NET_BAD_NAME (6)
#define NET_NET_INIT (7)
#define NET_NET_SOCKET (8)
#define NET_NET_BIND (9)

#define CONNECT_INTERVAL (2)
#define CONNECT_ATTEMPTS (10)

#define DATA_NOT_SET (-1)

enum class NetworkStatus
{
    Disconnected,
    Connected,
    Joining,
};

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

class Pop3Network
{
public:
    Pop3Network();


    // Lobby Management.
    DWORD AreWeLobbied(NetworkDataCallbackProc theCallback, UBYTE start_mode, UNICODE_CHAR* player_name);

    // Player managment Functions.
    BOOL GetPlayerInfo(POP3NETWORK_PLAYERINFO* player);
    void EnableNewPlayers(BOOL allow_new_players);
    void RemovePlayer(SWORD player_id);
    void initGamePtrs(const struct POP3NETWORK_GAMEDATA & gd);

    // Data managment Functions.
    int SendData(DWORD to_id, void* dataPtr, DWORD size);
    int SendChat(BYTE chat_targets, const UNICODE_CHAR* message);
    int SendScriptData(const std::string & data);

    // Misc Functions.
    static ULONGLONG GetCurrentMs();

    // Dedicated Server Interface
    POP3NETWORK_PLAYERINFO & GetPlayerDetails(UBYTE playernum);
    int GetPlayerCount();
    void DestroySession();

    bool am_i_host() const;

    void host_watchdog_func();
    void join_watchdog_func();
    
    const NetworkStatus getStatus() const;
    const int getRetries() const;

    const std::list<class PacketInfo> & getPacketInfo();
    static const std::string tribes[];
private:
    mutable std::list<class PacketInfo> packets;
    UNICODE_CHAR my_name[MAX_PLAYER_NAME_LEN];
    POP3NETWORK_PLAYERINFO players[NETWORK_NUMBER_PLAYERS];
    NetworkDataCallbackProc ProcessPopData;
    BOOL allow_joiners;
    SWORD player_num;
    SOCKET udp_socket;
    BYTE active_players;
    Poco::Condition cv2;
    bool am_host;
    int error;

    enum NetworkStatus status;
    struct POP3NETWORK_GAMEDATA GamePtrs;

    std::vector<UNICODE_STRING> blockedPlayers;
    int connection_retries;

    void ClearPlayers();
    void ClearPlayer(int player);
    // Startup and Shutdown functions.

    BOOL StartupNetwork(NetworkDataCallbackProc theCallback, UBYTE mode);
    void RunServer(BOOL joining);
    void SendPointers(const char* peer_address, UWORD peer_port);
    void ParsePacket(char* buffer, DWORD buf_size, char* peer_address, UWORD peer_port);
    SWORD get_player_id(const char* peer_address, UWORD peer_port);
    void Send(const char* peer_address, UWORD peer_port, BYTE type) const;
    void Send(const char* peer_address, UWORD peer_port, BYTE type, const char* buffer, DWORD buf_size) const;
    void Send(int to_id, BYTE type);
    void Send(int to_id, BYTE type, const char* buffer, DWORD buf_size);

    int SendChatToAll(const char* buf, int buf_len);
    int ProcessChat(const char* buf, int buf_len);
    int ProcessScript(const char* buf, int buf_len);
    void SendMyInfo(const char* peer_address, UWORD peer_port) const;
    void send_join_request() const;
    void check_join_request(const char* peer_address, UWORD peer_port, const char* buffer) const;

    SWORD add_player(const char* peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR* player_name);
    void add_players(const char* peer_address, UWORD peer_port, char* buffer);
    void remove_player(int player_id);

    void send_players(int to_id);
    void send_add_player(SWORD player_id);
    void send_remove_player(SWORD player_id);

    void loadBlockedPlayers(const std::string& filename);
    bool isBlocked(UNICODE_STRING name);
    bool is_blocked(int player_id);

    void add_packet_info(const PacketInfo & pi) const;
};