#include	<sstream>
#include	<fstream>
#include	<algorithm>
#include    "Pop3Network.h"
#include    "Pop3Debug.h"
#include    "Pop3App.h"
#include    <Poco/Thread.h>
#include    <Poco/Condition.h>
#include    <Poco/Mutex.h>
#include    <Poco/Format.h>
#include    <stack>
#include    "../../../../version.h"

const std::string Pop3Network::tribes[] = { "Blue", "Red", "Yellow", "Green", "Cyan", "Pink", "Black", "Orange", "Neutral", "Spectator" };
#define		EXCEED(number,lex,hex)		if (number < lex) number=lex; if (number > hex) number=hex
#define		UEXCEED(number,hex)		    if (number > hex) number=hex

extern ULONGLONG getGameClockMiliseconds();

union U
{
    short s; // or use int16_t to be more specific
             //   vs.
    struct Byte
    {
        char c1, c2; // or use int8_t to be more specific
    }
    byte;
};

Poco::Mutex packet_info_mu;
void Pop3Network::add_packet_info(const PacketInfo & pi) const 
{
    packet_info_mu.lock();

    if (packets.size() > 100)
        packets.pop_back();

    packets.push_front(pi);

    packet_info_mu.unlock();
}

const std::list<class PacketInfo> & Pop3Network::getPacketInfo()
{
    return packets;
}

Pop3Network::Pop3Network() : ProcessPopData(nullptr), allow_joiners(false), player_num(-1), status(NetworkStatus::Disconnected), connection_retries(0)
{
    memset(players, 0, sizeof(POP3NETWORK_PLAYERINFO)*NETWORK_NUMBER_PLAYERS);
    memset(&GamePtrs, 0, sizeof(GamePtrs));
    this->loadBlockedPlayers("SAVE/blocked.txt");
    Pop3Debug::trace("Pop3Network init");
}

bool Pop3Network::am_i_host() const
{
    return am_host;
}

void Pop3Network::loadBlockedPlayers(const std::string& filename)
{
    blockedPlayers.clear();
    UNICODE_STRING name;
    std::wfstream infile;
    infile.open(filename);
    if (infile.is_open())
    {
        while (!infile.eof()) //-V1024
        {
            std::getline(infile, name);
            std::transform(name.begin(), name.end(), name.begin(), ::towlower); // To lower!
            this->blockedPlayers.push_back(name);
        }
        infile.close();
    }
}

bool Pop3Network::isBlocked(UNICODE_STRING name)
{
    // To Lower
    std::transform(name.begin(), name.end(), name.begin(), ::towlower);

    for (std::vector<UNICODE_STRING>::iterator it = blockedPlayers.begin(); it != blockedPlayers.end(); ++it)
    {
        if (name == (*it))
        {
            return true;
        }
    }
    return false;
}

bool Pop3Network::is_blocked(int player_id)
{
    for (auto& player : players)
    {
        if (player.inUse && player.uniquePlayerId == player_id)
        {
            return player.blocked;
        }
    }
    // Don't know who this is, but probably don't want to hear him
    return true;
}

// Startup and Shutdown functions.

void Pop3Network::host_watchdog_func()
{
    status = NetworkStatus::Connected;

    // Do not allow joiners on init unless the game is already loaded!
    while (!(*GamePtrs.GameState == GM_STATE_NORMAL && *GamePtrs.GameMode == GM_FRONTEND))
    {
        Poco::Thread::sleep(1); // Intentional 
    }
    player_num = *GamePtrs.RequestedPlayerNum;

    // We successfully joined!
    *GamePtrs.PlayerNum = static_cast<SBYTE>(player_num);
    *GamePtrs.NetMyPlayerNumber = player_num;

    // Let the flood gates open!
    allow_joiners = true;
}

void Pop3Network::join_watchdog_func()
{
    static bool first_time = true;
    status = NetworkStatus::Joining;

    // Wait until you're loaded!
    while (!(*GamePtrs.GameState == GM_STATE_NORMAL && *GamePtrs.GameMode == GM_FRONTEND))
    {
        Poco::Thread::sleep(1); // Intentional 
    }

    Poco::Mutex cv_m;
    while (true)
    {
        if (!first_time)
        {
            if ((cv2.tryWait(cv_m, CONNECT_INTERVAL * 1000) && error == NET_OK) || status == NetworkStatus::Connected)
                break;

            if (error != NET_OK)
                error = NET_OK;

            Pop3Debug::trace("Connecting (%d)...", ++connection_retries);
        } else first_time = false;

        Pop3Debug::trace("Requesting host to join!");

        // Send join packet
        send_join_request();
    }

    Pop3Debug::trace("Host has approved my connection!");

    ASSERT(player_num < NETWORK_NUMBER_PLAYERS && player_num > 0);

    // We successfully joined!
    *GamePtrs.PlayerNum = static_cast<SBYTE>(player_num);
    *GamePtrs.NetMyPlayerNumber = player_num;
}

const NetworkStatus Pop3Network::getStatus() const
{
    return status;
}

const int Pop3Network::getRetries() const
{
    return connection_retries;
}

BOOL Pop3Network::StartupNetwork(NetworkDataCallbackProc theCallback, UBYTE mode)
{
    ProcessPopData = theCallback;
    player_num = DATA_NOT_SET;
    if (mode == SM_HOSTING && *GamePtrs.RequestedPlayerNum == 255) *GamePtrs.RequestedPlayerNum = 0;

    Pop3Debug::trace("Setting up network...");

    try
    {
        std::thread(&Pop3Network::RunServer, this, mode == SM_JOINING).detach();

        if (mode == SM_HOSTING)
        {
            std::thread(&Pop3Network::host_watchdog_func, this).detach();
            Pop3Debug::trace("I am host and my player number is %d", player_num);
        }
        else
        {
            Pop3Debug::trace("I am joining and my requested player num is %d", *GamePtrs.RequestedPlayerNum);
            std::thread(&Pop3Network::join_watchdog_func, this).detach();
        }
    }
    catch (std::exception & err)
    {
        Pop3Debug::trace("Network error! %s", err.what());
        return NET_NET_INIT;
    }

    *GamePtrs.NetLobbied = TRUE;
    return (error == NET_OK);
}

// Lobby Management.
DWORD Pop3Network::AreWeLobbied(NetworkDataCallbackProc theCallback, UBYTE start_mode, UNICODE_CHAR* player_name)
{
    std::char_traits<UNICODE_CHAR>::copy(my_name, player_name, MAX_PLAYER_NAME_LEN);
    if (!player_name[0]) return NET_BAD_NAME;
    my_name[MAX_PLAYER_NAME_LEN - 1] = '\0';
    error = NET_OK;

    // Player is going for single player
    if (start_mode != SM_HOSTING && start_mode != SM_JOINING)
        return NET_SINGLE_PLAYER;

    // Clients should talk to the host now, don't return until we're in the lobby
    if (StartupNetwork(theCallback, start_mode))
    {
        if (start_mode == SM_HOSTING)
        {
            return NET_HOSTING;
        }
        return NET_JOINED;
    }
    return error;
}

// Player managment Functions.
BOOL Pop3Network::GetPlayerInfo(POP3NETWORK_PLAYERINFO* out_player)
{
    for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        out_player[i].inUse = FALSE;
    }

    for (auto& player : players)
    {
        if (player.inUse && player.uniquePlayerId > -1 && player.uniquePlayerId < NETWORK_NUMBER_PLAYERS)
        {
            out_player[player.uniquePlayerId].inUse = player.inUse;
            out_player[player.uniquePlayerId].host = player.host;
            std::char_traits<UNICODE_CHAR>::copy(&out_player[player.uniquePlayerId].name[0], &player.name[0], MAX_PLAYER_NAME_LEN);
        }
    }

    return TRUE;
}

void Pop3Network::EnableNewPlayers(BOOL allow_new_players)
{
    allow_joiners = allow_new_players;
}

void Pop3Network::RemovePlayer(SWORD player_id)
{
    Pop3Debug::trace("Player %d has been removed from the server.", player_id);

    send_remove_player(player_id);
    remove_player(player_id);
}

void Pop3Network::initGamePtrs(const POP3NETWORK_GAMEDATA & gd)
{
    GamePtrs = gd;
}

// Quitting
void Pop3Network::DestroySession()
{
    Pop3Debug::trace("Player %d is quitting as %d", player_num, am_host);

    status = NetworkStatus::Disconnected;

    if (am_host)
        Send(NET_ALLPLAYERS, NET_CLIENT_QUIT);
    else
        Send(NET_SERVERPLAYERS, NET_CLIENT_QUIT);
}

// Data managment Functions.
int Pop3Network::SendData(DWORD to_id, void* dataPtr, DWORD size)
{
    Send(to_id, NET_POP_DATA, static_cast<char*>(dataPtr), size);
    return TRUE;
}

// chat_targets = list of bit representing each player to receive message
// e.g. chat_targets = (1 << 2) means the message goes to player 2 (yellow)
//      chat_targets = (1 << 2) & (1 << 3) goes to yellow and green
int Pop3Network::SendChat(BYTE chat_targets, const UNICODE_CHAR* message)
{
    static char buf[MAX_PACKET_SIZE];
    int len = sizeof(UNICODE_CHAR) * (std::char_traits<UNICODE_CHAR>::length(message) + 1);
    buf[0] = static_cast<char>(player_num); // from
    buf[1] = chat_targets; // to
                           // Always add self to chat
    buf[1] |= (1 << player_num);
    memcpy(&buf[2], reinterpret_cast<const char*>(message), len);
    if (am_host)
    {
        ProcessChat(buf, len + 2);
    }
    else
    {
        Send(NET_SERVERPLAYERS, NET_POP_CHAT, buf, len + 2);
    }
    return TRUE;
}

int Pop3Network::SendScriptData(const std::string & data)
{
    static char buf[MAX_PACKET_SIZE];
    int len = static_cast<int>(data.size());

    if (len < MAX_PACKET_SIZE)
    {
        memcpy(&buf[0], reinterpret_cast<const char*>(data.c_str()), len);
    }
    else memcpy(&buf[0], reinterpret_cast<const char*>(data.c_str()), MAX_PACKET_SIZE);

    Send(NET_ALLPLAYERS, NET_POP_SCRIPT, buf, len);
    return 0;
}

int Pop3Network::SendChatToAll(const char* buf, int buf_len)
{
    if (am_host)
    {
        for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
        {
            // Only send chat packets to players that need them
            if ((buf[1] & (1 << i)) > 0)
            {
                Send(i, NET_POP_CHAT, buf, buf_len);
            }
        }
    }
    return TRUE;
}

int Pop3Network::ProcessChat(const char* buf, int buf_len)
{
    if (am_host)
    {
        SendChatToAll(buf, buf_len);
    }

    // If message is from or to me
    if (buf[0] == player_num || (buf[1] & (1 << player_num)) > 0)
    {
        if (!is_blocked(buf[0]))
        {
            ProcessPopData(buf[0], const_cast<char*>(&buf[2]), buf_len - 2, NET_POP_CHAT, reinterpret_cast<const void*>(&buf[1]));
        }
    }
    return TRUE;
}

int Pop3Network::ProcessScript(const char * buf, int buf_len)
{
    auto s = std::string(buf, buf_len);
    GamePtrs.Script_Callback(s);
    return TRUE;
}

#define STORE(d) ptr= &d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));
#define STORE_I(d) ptr= d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));
#define STORE_PTR(d) ptr= d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));

void Pop3Network::SendPointers(const char* peer_address, UWORD peer_port)
{
    // ReSharper disable once CppJoinDeclarationAndAssignment
    void* ptr;
    auto i = 0;
    static char buf[MAX_PACKET_SIZE];
    U version{ BUILD_NUMBER };

    memset(buf, 0, MAX_PACKET_SIZE);
    buf[0] = MAJOR_VERSION;
    buf[1] = MINOR_VERSION;
    buf[2] = version.byte.c1;
    buf[3] = version.byte.c2;
    buf[4] = 0;

    // ReSharper disable once CppJoinDeclarationAndAssignment
    STORE_PTR(GamePtrs.PlayerNum); // OFF_TRIBE
    STORE_PTR(GamePtrs.GnsiFlags); // OFF_STATUS
    STORE_PTR(GamePtrs.StartLevelNumber); // OFF_LEVEL_INDEX
    STORE_PTR(GamePtrs.Allies); // OFF_ALLIES
    STORE_PTR(GamePtrs.name); // OFF_NAMES
    STORE_PTR(GamePtrs.StatValues); // OFF_PLAYER_STATS
    STORE_PTR(GamePtrs.NumPeopleOfType); // OFF_UNITS
    STORE_PTR(GamePtrs.CurrNumPlayers); // OFF_PLAYERS
    STORE(active_players); // OFF_ACTIVE_PLAYERS
    STORE_PTR(GamePtrs.frame_rate_draw); // OFF_FRAMERATE
    STORE_PTR(GamePtrs.PingTime); // OFF_PING
    STORE_PTR(GamePtrs.GameTurn); // OFF_GAME_TIME
    STORE_PTR(GamePtrs.PlayerMsg); // OFF_GAME_MSG
    STORE_PTR(GamePtrs.BuildingsAvailable);// OFF_BUILDING_PANEL
    STORE_PTR(GamePtrs.PlayerFlags); // OFF_GOD
    STORE_PTR(GamePtrs.spells_type_info); // OFF_SPELLS
    STORE_I(reinterpret_cast<void*>(GamePtrs.PlayerStructSize)); // OFF_PLAYER_DIFF
                                                             // OFF_RESYNC_ON
                                                             // OFF_RESYNC_COUNT
                                                             // OFF_CHEAT_FLAGS
                                                             // OFF_CHEAT_BYRNE
                                                             // OFF_OBJECT_LIST
                                                             // OFF_SHAMAN_LIMIT
                                                             // OFF_FORGE_SETTING
                                                             // OFF_GUEST_SPELL

    buf[4] = static_cast<char>(i);
    Send(peer_address, peer_port, NET_VERSION_DATA, buf, 5 + i * sizeof(void*));
}

// Misc Functions.
ULONGLONG Pop3Network::GetCurrentMs()
{
    return getGameClockMiliseconds();
}

// Clears all players
void Pop3Network::ClearPlayers()
{
    for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        ClearPlayer(i);
    }
}

// Clears player at index i
void Pop3Network::ClearPlayer(int i)
{
    players[i].inUse = FALSE;
    players[i].host = FALSE;
    players[i].name[0] = 0;
    players[i].port = 0;
    players[i].dwFlags = 0;
    players[i].slotNo = 0;
    players[i].uniquePlayerId = 0;
    players[i].address[0] = 0;
    players[i].blocked = false;
}

void Pop3Network::RunServer(BOOL joining)
{
    Pop3Debug::trace("Starting UDP server %d on port %d", joining, *GamePtrs.LocalPort);

    struct sockaddr_in server, si_other;
    int recv_len;
    static char buf[MAX_PACKET_SIZE];
    bool first_connection = true;
    WSADATA wsa;
    std::stringstream st;

    int slen = sizeof(si_other);

    //Initialise winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        st << "Failed. Error Code : " << WSAGetLastError();
        Pop3Debug::trace(st.str().c_str());
        error = NET_NET_INIT;
        return;
    }
    Pop3Debug::trace("Initialised.");

    //Create a socket
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
    {
        int err = WSAGetLastError();
        st << "Could not create socket : " << err;
        Pop3Debug::trace(st.str().c_str());
        error = NET_NET_SOCKET;
        return;
    }
    Pop3Debug::trace("Socket created.");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    
    if (joining)
    {
        if (!*GamePtrs.LocalPort)
        {
            *GamePtrs.LocalPort = *GamePtrs.RemotePort;
        }
    }

    server.sin_port = htons(*GamePtrs.LocalPort);

    //Bind
    if (bind(udp_socket, reinterpret_cast<struct sockaddr *>(&server), sizeof(server)) == SOCKET_ERROR)
    {
        st << "Bind failed with error code : " << WSAGetLastError() << "\n";
        Pop3Debug::trace(st.str().c_str());
        error = NET_NET_BIND;
        return;
    }

    if (joining)
    {
        am_host = false;
    }
    else
    {
        add_player("", 0, *GamePtrs.RequestedPlayerNum, true, my_name);
        am_host = true;
    }

    //keep listening for data
    while (!(*GamePtrs.GnsiFlags & GNS_QUITTING) && !Pop3App::isQuitting())
    {
        //clear the buffer by filling null, it might have previously received data
        memset(buf, '\0', MAX_PACKET_SIZE);

        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(udp_socket, buf, MAX_PACKET_SIZE, 0, reinterpret_cast<struct sockaddr *>(&si_other), &slen)) == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            st << "Recvfrom() failed with error code : " << err << "\n";
            Pop3Debug::trace(st.str().c_str());
            continue;
        }

        if (first_connection)
        {
            // ReSharper disable once CppDeprecatedEntity
            SendPointers(inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
            first_connection = false;
        }

        if (recv_len > 1 && buf[0] == POP_PACKET_TYPE)
        {
            // ReSharper disable once CppDeprecatedEntity
            ParsePacket(buf, recv_len, inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        }
    }

    closesocket(udp_socket);
    WSACleanup();
}

void Pop3Network::ParsePacket(char* buffer, DWORD buf_size, char* peer_address, UWORD peer_port)
{
    bool is_host;
    SWORD player_number;
    SWORD from_id = get_player_id(peer_address, peer_port);

    PacketInfo pi;
    pi.from_id = from_id;
    pi.to_id = player_num;
    pi.packetSize = buf_size;
    pi.peer_address = peer_address;
    pi.port = peer_port;
    pi.packet_num = buffer[1];
    pi.time = getGameClockMiliseconds();

    switch (buffer[1])
    {
    case NET_CLIENT_REQUEST_JOIN:
        check_join_request(peer_address, peer_port, &buffer[2]);
        break;

    case NET_CLIENT_JOIN:
        player_number = /*(buffer[2] == 255) ? -1 :*/ buffer[2];
        is_host = buffer[3] > 0;
        from_id = add_player(peer_address, peer_port, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buffer[4]));
        send_players(from_id);
        send_add_player(from_id);
        pi.extra_data = Poco::format("%?i %?i %?i", player_number, is_host, from_id);
        break;

    case NET_CLIENT_QUIT:
        if (from_id < 0) return;
        send_remove_player(from_id);
        remove_player(from_id);
        break;

    case NET_HOST_ACCEPT_JOIN:
        player_num = DATA_NOT_SET;
        SendMyInfo(peer_address, peer_port);
        break;

    case NET_HOST_DENY_JOIN:
        error = NET_BAD_VERSION;
        break;

    case NET_HOST_PLAYERS:
        add_players(peer_address, peer_port, &buffer[2]);
        break;

    case NET_HOST_ADD_PLAYER:
        if (from_id < 0) return;
        player_number = /*(buffer[2] == 255) ? -1 :*/ buffer[2];
        is_host = buffer[3] > 0;
        add_player("", 0, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buffer[4]));
        pi.extra_data = Poco::format("%?i %?i %?i", player_number, is_host, from_id);
        break;

    case NET_HOST_DELETE_PLAYER:
        if (from_id < 0) return;
        remove_player(buffer[2]);
        break;

    case NET_POP_DATA:
        if (from_id < 0) return;
        ProcessPopData(from_id, &buffer[2], buf_size - 2, buffer[1], nullptr);
        pi.extra_data = Poco::format("%d", +buffer[1]);
        break;

    case NET_POP_CHAT:
        ProcessChat(&buffer[2], buf_size - 2);
        break;
    case NET_POP_SCRIPT:
        ProcessScript(&buffer[2], buf_size - 2);
        break;
    default:
        break;
    }
    add_packet_info(pi);
}

SWORD Pop3Network::get_player_id(const char* peer_address, UWORD peer_port)
{
    for (auto& player : players)
    {
        if (player.inUse && player.port == peer_port && strcmp(player.address, peer_address) == 0)
        {
            return player.uniquePlayerId;
        }
    }
    return -1;
}

void Pop3Network::Send(int to_id, BYTE type)
{
    Send(to_id, type, nullptr, 0);
}

void Pop3Network::Send(int to_id, BYTE type, const char* buffer, DWORD buf_size)
{
    for (auto& player : players)
    {
        if ((player.inUse) && (// This should be first in the IF statement.			
            (to_id == static_cast<signed>(NET_SERVERPLAYERS) && player.host) ||
            (to_id == static_cast<signed>(NET_ALLPLAYERS) ||
                to_id == player.uniquePlayerId)))
        {
            Send(player.address, player.port, type, buffer, buf_size);
        }
    }
}

void Pop3Network::Send(const char* peer_address, UWORD peer_port, BYTE type) const
{
    Send(peer_address, peer_port, type, nullptr, 0);
}

void Pop3Network::Send(const char* peer_address, UWORD peer_port, BYTE type, const char* buffer, DWORD buf_size) const
{
    if (peer_address[0] == 0 || peer_port == 0) return;
    ASSERT(buf_size < (MAX_PACKET_SIZE - 2));

    if (buffer != nullptr)
    {
        ASSERT(buf_size > 0);
    }
    else
    {
        ASSERT(buf_size == 0);
    }

    PacketInfo pi;
    pi.peer_address = peer_address;
    pi.port = peer_port;
    pi.packet_num = type;
    pi.packetSize = buf_size;
    pi.time = getGameClockMiliseconds();
    pi.sending_packet = true;
    pi.from_id = player_num;

    if (type == NET_POP_DATA)
        pi.extra_data = Poco::format("%d", +static_cast<char>(buffer[1]));

    // Resolve ip:port to player number
    for (auto& player : players)
    {
        if (&player.address[0] == peer_address && player.port == peer_port)
        {
            pi.to_id = player.uniquePlayerId; break;
        }
    }

    static char buf[MAX_PACKET_SIZE];
    struct sockaddr_in si_other;
    int slen = sizeof(si_other);
    memset(reinterpret_cast<char *>(&si_other), 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(peer_port);
    // ReSharper disable once CppDeprecatedEntity
    si_other.sin_addr.S_un.S_addr = inet_addr(peer_address);

    buf[0] = POP_PACKET_TYPE;
    buf[1] = type;
    if (buffer && buf_size > 0) memcpy(&buf[2], buffer, buf_size);

    if (sendto(udp_socket, buf, buf_size + 2, 0, reinterpret_cast<struct sockaddr*>(&si_other), slen) == SOCKET_ERROR)
    {
        std::stringstream st;
        st << "sendto() failed with error code : " << WSAGetLastError() << "\n";
        Pop3Debug::trace(st.str().c_str());
    } else pi.packet_sent = true;

    add_packet_info(pi);
}

void Pop3Network::SendMyInfo(const char* peer_address, UWORD peer_port) const
{
    static char buf[MAX_PACKET_SIZE];
    memset(buf, 0, MAX_PACKET_SIZE);
    // Player num
    buf[0] = *GamePtrs.RequestedPlayerNum;
    // Host
    buf[1] = FALSE;
    std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[2]), my_name, MAX_PLAYER_NAME_LEN);
    Send(peer_address, peer_port, NET_CLIENT_JOIN, buf, 2 + sizeof(UNICODE_CHAR) * (std::min(static_cast<unsigned>(std::char_traits<UNICODE_CHAR>::length(my_name) + 1), static_cast<unsigned>(MAX_PLAYER_NAME_LEN))));
}

void Pop3Network::send_join_request() const
{
    U version{ BUILD_NUMBER };
    size_t i = 0;
    static char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    buf[i++] = MAJOR_VERSION;
    buf[i++] = MINOR_VERSION;
    buf[i++] = version.byte.c1;
    buf[i++] = version.byte.c2;
    buf[i++] = *GamePtrs.RequestedPlayerNum;
    Send(GamePtrs.RemoteIPAddress, *GamePtrs.RemotePort, NET_CLIENT_REQUEST_JOIN, buf, i);
}

void Pop3Network::check_join_request(const char* peer_address, UWORD peer_port, const char* buffer) const
{
    U version;
    version.byte.c1 = buffer[2];
    version.byte.c2 = buffer[3];
    BYTE pn = buffer[4];
    bool allowed_pn = true;
    ASSERT(pn < MAX_NUM_REAL_PLAYERS + MAX_NUM_SPECTATORS || pn == static_cast<BYTE>(DATA_NOT_SET));

    Pop3Debug::trace("Join request recieved from %s on port %d requesting player number %d on build %d.%d.%d", peer_address, peer_port, pn, buffer[0], buffer[1], version.s);

    for (auto & p : players)
    {
        if (p.inUse)
        {
            if (p.uniquePlayerId == pn)
            {
                allowed_pn = false;
                break;
            }
        }
    }

    if (allow_joiners && /*(pn == DATA_NOT_SET || allowed_pn) &&*/
        (buffer[0] == MAJOR_VERSION &&
            buffer[1] == MINOR_VERSION &&
            version.s == BUILD_NUMBER))
    {
        Pop3Debug::trace("NET_HOST_ACCEPT_JOIN");
        Send(peer_address, peer_port, NET_HOST_ACCEPT_JOIN);
    }
    else
    {
        Pop3Debug::trace("NET_HOST_DENY_JOIN");
        Send(peer_address, peer_port, NET_HOST_DENY_JOIN);
    }
}

#define PLAYER_SLOT_INVALID -1
SWORD Pop3Network::add_player(const char* peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR* player_name)
{
    SWORD next_player_slot = PLAYER_SLOT_INVALID;
    SWORD next_free_slot = PLAYER_SLOT_INVALID;
    bool is_pn_used[NETWORK_NUMBER_PLAYERS];
    memset(&is_pn_used, 0, sizeof(bool)*NETWORK_NUMBER_PLAYERS);

    Pop3Debug::trace("Adding player %d on ip %s:%d (%d) %ls", player_number, peer_address, peer_port, is_host, player_name);

    for (SWORD i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        if (players[i].inUse)
        {
            // Don't add duplicate of player
            if (std::char_traits<UNICODE_CHAR>::compare(&players[i].name[0], &player_name[0], MAX_PLAYER_NAME_LEN) == 0)
            {
                Pop3Debug::trace("Duplicate player detected (on ID %d)! %ls %ls", players[i].uniquePlayerId, &players[i].name[0], player_name);
                return players[i].uniquePlayerId;
            }

            // Don't allow two players to have same ID
            if (players[i].uniquePlayerId == player_number)
            {
                Pop3Debug::trace("Duplicate player id! %d %d", players[i].uniquePlayerId, player_number);
                player_number = -1;
            }

            if (players[i].uniquePlayerId > -1)
                is_pn_used[players[i].uniquePlayerId] = true;
        }
        else
            if (next_free_slot == DATA_NOT_SET)
                next_free_slot = i;
    }

    if (next_free_slot != DATA_NOT_SET)
    {
        Pop3Debug::trace("Slots were open the new slot number is %d", next_free_slot);
        next_player_slot = next_free_slot;
    }
    else
    {
        Pop3Debug::trace("No open slots, cannot join!");
        return -1; // No open slots;
    }


    if (am_host)
    {
        if (player_number == PLAYER_SLOT_INVALID || is_pn_used[player_number])
        {
            Pop3Debug::trace("Player number is used or invalid");
            for (SWORD new_pn = 0; new_pn < NETWORK_NUMBER_PLAYERS; new_pn++)
                if (!is_pn_used[new_pn])
                {
                    Pop3Debug::trace("Chosen new player number from unused player numbers %d", new_pn);
                    player_number = new_pn;
                    break;
                }

            if (player_number == PLAYER_SLOT_INVALID)
            {
                Pop3Debug::trace("A player number could not be assigned, this is a fatal network error");
                return -1;
            }
        }
    }

    POP3NETWORK_PLAYERINFO player{};
    player.inUse = TRUE;
    player.uniquePlayerId = player_number;
    std::char_traits<UNICODE_CHAR>::copy(&player.name[0], player_name, MAX_PLAYER_NAME_LEN);
    player.name[MAX_PLAYER_NAME_LEN - 1] = '\0';
    player.blocked = this->isBlocked(player.name);
    player.host = is_host;
    strncpy(player.address, peer_address, MAX_ADDRESS);
    player.port = peer_port;

    // If the player added is me, save the number
    if (std::char_traits<UNICODE_CHAR>::compare(&player.name[0], &my_name[0], MAX_PLAYER_NAME_LEN) == 0)
    {
        Pop3Debug::trace("This person is me!");
        player_num = player_number;
        cv2.broadcast();
        status = NetworkStatus::Connected;
    }

    Pop3Debug::trace("Player slot is %d", next_player_slot);

    players[next_player_slot] = player;

#ifdef _DEDSEVR
    std::wcout << "Player (" << player_number << ") " << player.name << " joined ";
    if (player_number < 4) {
        std::wcout << "team " << tribes[player_number].c_str();
    }
    else {
        std::wcout << "as spectator.";
    }
#endif

    return player_number;
}

void Pop3Network::remove_player(int player_id)
{
    for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        if (players[i].uniquePlayerId == player_id)
        {
            ClearPlayer(i);
        }
    }
}

void Pop3Network::add_players(const char* peer_address, UWORD peer_port, char* buffer)
{
    char* buf = &buffer[1];
    // i increments over the number of players in data packet, not player IDs
    for (int i = 0; i < buffer[0]; i++)
    {
        SWORD player_number = /*(buf[0] == 255) ? -1 :*/ buf[0];
        bool is_host = buf[1] > 0;
        if (is_host)
        {
            add_player(peer_address, peer_port,
                player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buf[2]));
        }
        else
        {
            add_player("", 0,
                player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buf[2]));
        }
        // player data = 2 + MAX_PLAYER_NAME_LEN
        buf += 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN);
    }
}

void Pop3Network::send_players(int to_id)
{
    if (to_id < 0) return;
    int p = 1;
    static char buf[MAX_PACKET_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0; // number of players

    for (auto& player : players)
    {
        if (player.inUse)
        {
            buf[p + 0] = static_cast<char>(player.uniquePlayerId);
            //buf[p + 1] = player.dwFlags;
            buf[p + 1] = player.host;
            std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[p + 2]), &player.name[0], MAX_PLAYER_NAME_LEN);
            buf[0]++;
            p += 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN);
        }
    }

    Send(to_id, NET_HOST_PLAYERS, buf, p);
}

void Pop3Network::send_add_player(SWORD player_id)
{
    if (player_id < 0) return;
    static char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    for (auto& player : players)
    {
        if (player.uniquePlayerId == player_id)
        {
            buf[0] = static_cast<char>(player.uniquePlayerId);
            //buf[1] = player.dwFlags;
            buf[1] = player.host;
            std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[2]), &player.name[0], MAX_PLAYER_NAME_LEN);
            break;
        }
    }
    Send(NET_ALLPLAYERS, NET_HOST_ADD_PLAYER, buf, 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN));
}

void Pop3Network::send_remove_player(SWORD player_id)
{
    if (player_id < 0) return;
    static char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    buf[0] = static_cast<char>(player_id);
    Send(NET_ALLPLAYERS, NET_HOST_DELETE_PLAYER, buf, 1);
}


POP3NETWORK_PLAYERINFO & Pop3Network::GetPlayerDetails(UBYTE playernum)
{
    UEXCEED(playernum, NETWORK_NUMBER_PLAYERS - 1); // Arrays start at 0. It should never reach 4.
    for (auto& player : players)
    {
        if (player.inUse && player.uniquePlayerId == playernum)
        {
            return player;
        }
    }

    // Player not found!
    static POP3NETWORK_PLAYERINFO ret = {};
    ret.inUse = FALSE;
    return ret;
}

int Pop3Network::GetPlayerCount()
{
    int count = 0;

    for (auto& player : players)
    {
        if (player.inUse == TRUE)
        {
            count++;
        }
    }

    return count;
}
