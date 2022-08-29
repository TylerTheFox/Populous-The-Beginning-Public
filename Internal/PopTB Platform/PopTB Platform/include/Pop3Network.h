#pragma once
#include "../../../../Pop3.h"
#include "Pop3Types.h"
#include <list>
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <vector>

extern Poco::Mutex packet_info_mu;

// These are the callback types.
#define	NET_CALLBACK	__stdcall
typedef void (NET_CALLBACK *NetworkServicesCallbackProc)(void* connection, UNICODE_CHAR* lpDriverDescription, GUID* guiddatatype, DWORD noguiddatatypes, void* param);
typedef void (NET_CALLBACK *NetworkDataCallbackProc)(DWORD from_id, void* dataPtr, DWORD dataSize, DWORD messageType, const void* param);
typedef void (NET_CALLBACK *NetworkLobbyApplicationsCallback)(void* dataPtr, void* param);
typedef void (NET_CALLBACK *NetworkMediumCallback)(UNICODE_CHAR* medium, void* param);
typedef void (NET_CALLBACK *NetworkPingCallback)(WORD id, WORD seq, DWORD rondtriptime, void* param);

#define POP3NETWORK_MAX_PASSWORD_LENGTH     (1)   // lol
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
    HOST_ACCECPT_JOIN,
    HOST_DENY_JOIN,
    HOST_PLAYERS,
    HOST_ADD_PLAYER,
    HOST_DELETE_PLAYER,
    POP_DATA,
    POP_CHAT,
    POP_SCRIPT,
    HOST_READY_FOR_FILE_TRANSFER,
    CLIENT_READY_FOR_FILE_TRANSFER,
    HOST_SEND_FILE_PART,
    HOST_SEND_FILE_TRANSFER_COMPLETE,
    CLIENT_FILE_TRANSFER_COMPLETE,
    CLIENT_RESEND_FILE_PART,
    CLIENT_RESNET_FILE_PARTS_COMPLETE,
    HOST_REQUESTING_UPDATE_ON_TRANSFER
};

enum class Pop3ErrorStatusCodes
{
    OK,
    SINGLE_PLAYER,
    HOSTING,
    JOINED,
    TIMEOUT,
    BAD_VERSION,
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
    std::function<void(std::string&)> Script_Callback;
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

struct PopTBTestFile
{
    PopTBTestFile()
    {
        file_name = "ALargeFile.zip";
        length = 64000000;
        someints = new int[length];
        for (int i = 0; i < length; i++)
            someints[i] = i;
    }
    ~PopTBTestFile()
    {
        delete[] someints;
    }
    std::string file_name;
    int* someints;
    size_t length;
};

enum class FileTransferStatus
{
    Ready,
    Host_Waiting_On_Client_Ack,
    Host_Waiting_On_Client_To_Finish_Resend_Requests,
    Host_Transfering_Data,
    Host_Waiting_On_Client_Resend_Complete,
    Host_Waiting_On_Client_To_Finish_Transfer,
    Client_Sending_Resend_Requests,
    Client_Waiting_On_Host_To_Start_Sending_File_Parts,
    Client_Waiting_On_Host_Parts_To_Finish,
    Transfer_Complete
};

struct FileTransfer
{
    std::string                                     peer_address;
    UWORD                                           peer_port;
    ULONGLONG                                       LastContactTime;
    PopTBFileStartPacket                            FileHeader;
    std::vector<PopTBFilePartPacket>                FileParts;
    std::vector<PopTBFilePartRequest>               FilePartsToBeResent;
    std::map<unsigned int, PopTBFilePartPacket>     FilePartsRecv;
    FileTransferStatus                              Status;
    ULONGLONG                                       SleepTimer;
    ULONGLONG                                       TotalByteSent;
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
    int                                             SendScriptData(const std::string & data);
    void                                            DestroySession();

    // Misc Functions.
    static ULONGLONG                                GetCurrentMs();
    POP3NETWORK_PLAYERINFO &                        GetPlayerDetails(UBYTE playernum);
    int                                             GetPlayerCount();
    bool                                            am_i_host() const;
    const NetworkStatus                             getStatus() const;
    const int                                       getRetries() const;
    const std::list<class PacketInfo> &             getPacketInfo();
    static const std::string                        tribes[];

    void                                            transfer_file(DWORD to, const std::string & file_name, const char * data, size_t length);
    FileTransferStatus                              getFileTransferStatus();
    unsigned int                                    getFileTransferPercent();
    size_t                                          getFileTransferTotalBytes();
    size_t                                          getFileTransferRecievedBytes();
    std::string                                     getFileTransferName();
    ULONGLONG                                       getFileTransferSleepTimer();
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
    mutable std::list<class PacketInfo>             packets;

    void                                            ParsePacket(char* buffer, DWORD buf_size, const char* peer_address, UWORD peer_port);
    SWORD                                           add_player(const char* peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR* player_name);
    void                                            SendPointers(const char* peer_address, UWORD peer_port);
    void                                            send_join_request() const;
    void                                            add_packet_info(const PacketInfo & pi) const;
private:
    FileTransfer FT;

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
    void                                            add_players(const char* peer_address, UWORD peer_port, char* buffer);
    void                                            Send(int to_id, Pop3NetworkTypes type);
    void                                            Send(int to_id, Pop3NetworkTypes type, const char* buffer, DWORD buf_size);
    void                                            Send(const char* peer_address, UWORD peer_port, Pop3NetworkTypes type) const;
    void                                            host_watchdog_func();
    void                                            join_watchdog_func();
    void                                            compile_fileparts();

    // File Transfers
    void                                            filetransfer_client_process_fileheader(const char * peer_address, UWORD peer_port, const char * buffer);
    void                                            filetransfer_host_process_client_ready(const char * peer_address, UWORD peer_port);
    void                                            filetransfer_client_process_file_part(const char * buffer);
    void                                            filetransfer_client_process_all_parts_sent();
    void                                            filetransfer_host_process_resend_request(const char * buffer);
    void                                            filetransfer_host_process_resend_requests_complete();
    void                                            filetransfer_host_process_client_transfer_successful();
    void                                            filetransfer_process_timeout(const char * peer_address, UWORD peer_port);
    void                                            filetransfer_host_requesting_update();

    // Network Layer Functions
    virtual void                                    ServerInit(UBYTE mode) = 0;
    virtual void                                    Send(int to_pn, const char* peer_address, UWORD peer_port, Pop3NetworkTypes type, const char* buffer, DWORD buf_size) const = 0;
};