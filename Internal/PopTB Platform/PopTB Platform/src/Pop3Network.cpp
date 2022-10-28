#include    "Pop3Network.h"
#include    "Pop3Debug.h"
#include    "../../../../version.h"
#include    "Pop3Debug.h"
#include    "Pop3App.h"
#include    <algorithm>
#include    <Poco/Thread.h>
#include    <thread>
#include    <math.h>
#include    <fstream>
#include  <Poco/Path.h>

const std::string Pop3Network::tribes[] = { "Blue", "Red", "Yellow", "Green", "Cyan", "Pink", "Black", "Orange", "Neutral", "Spectator" };
extern ULONGLONG getGameClockMiliseconds();
#define		EXCEED(number,lex,hex)		if (number < lex) number=lex; if (number > hex) number=hex
#define		UEXCEED(number,hex)		    if (number > hex) number=hex

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

Pop3Network::Pop3Network() : player_num(0), connection_retries(0), am_host(false), network_status(NetworkStatus::Disconnected), server_status(Pop3ErrorStatusCodes::OK), allow_joiners(false), ProcessPopData(nullptr)
{
    GamePtrs = {};
}

Pop3Network::~Pop3Network()
{
    DebugBreak();
}

Poco::Mutex packet_info_mu;
void Pop3Network::add_packet_info(const PacketInfo & pi) const
{
    packet_info_mu.lock();

    if (packets.size() > 100)
        packets.pop_back();

    packets.push_front(pi);

    packet_info_mu.unlock();
}

Pop3ErrorStatusCodes Pop3Network::AreWeLobbied(NetworkDataCallbackProc theCallback, UBYTE start_mode, UNICODE_CHAR * player_name)
{
    my_name = std::wstring(player_name);
    if (!player_name[0]) return Pop3ErrorStatusCodes::BAD_NAME;
    server_status = Pop3ErrorStatusCodes::OK;

    // Player is going for single player
    if (start_mode != SM_HOSTING && start_mode != SM_JOINING)
        return Pop3ErrorStatusCodes::SINGLE_PLAYER;

    /// Network code. ///
    player_num = DATA_NOT_SET;
    if (start_mode == SM_HOSTING && *GamePtrs.RequestedPlayerNum == 255) *GamePtrs.RequestedPlayerNum = 0;
    ProcessPopData = theCallback;
    this->ServerInit(start_mode);

    if (start_mode == SM_HOSTING)
    {
        std::thread(&Pop3Network::host_watchdog_func, this).detach();
        Pop3Debug::trace("I am host and my player number is %d", player_num);
    }
    else
    {
        Pop3Debug::trace("I am joining and my requested player num is %d", *GamePtrs.RequestedPlayerNum);
        std::thread(&Pop3Network::join_watchdog_func, this).detach();
    }
    *GamePtrs.NetLobbied = TRUE;
    return Pop3ErrorStatusCodes::OK;
}

BOOL Pop3Network::GetPlayerInfo(POP3NETWORK_PLAYERINFO * out_player)
{
    for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        out_player[i].inUse = FALSE;
    }

    for (auto& player : players)
    {
        if (player.second.inUse && player.second.uniquePlayerId > -1 && player.second.uniquePlayerId < NETWORK_NUMBER_PLAYERS)
        {
            out_player[player.second.uniquePlayerId].inUse = player.second.inUse;
            out_player[player.second.uniquePlayerId].host = player.second.host;
            std::char_traits<UNICODE_CHAR>::copy(&out_player[player.second.uniquePlayerId].name[0], &player.second.name[0], MAX_PLAYER_NAME_LEN);
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
    remove_player_impl(player_id);
}

void Pop3Network::initGamePtrs(const POP3NETWORK_GAMEDATA & gd)
{
    GamePtrs = gd;
}

int Pop3Network::SendData(DWORD to_id, void * dataPtr, DWORD size)
{
    Send(to_id, Pop3NetworkTypes::POP_DATA, static_cast<char*>(dataPtr), size);
    return TRUE;
}

int Pop3Network::SendChat(BYTE chat_targets, const UNICODE_CHAR * message)
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
        Send(NET_SERVERPLAYERS, Pop3NetworkTypes::POP_CHAT, buf, len + 2);
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

    Send(NET_ALLPLAYERS, Pop3NetworkTypes::POP_SCRIPT, buf, len);
    return 0;
}

void Pop3Network::DestroySession()
{
    Pop3Debug::trace("Player %d is quitting as %d", player_num, am_host);

    if (am_host)
        Send(NET_ALLPLAYERS, Pop3NetworkTypes::CLIENT_QUIT);
    else
        Send(NET_SERVERPLAYERS, Pop3NetworkTypes::CLIENT_QUIT);

    network_status = NetworkStatus::Disconnected;
}

ULONGLONG Pop3Network::GetCurrentMs()
{
    return getGameClockMiliseconds();
}

POP3NETWORK_PLAYERINFO & Pop3Network::GetPlayerDetails(UBYTE playernum)
{
    UEXCEED(playernum, NETWORK_NUMBER_PLAYERS - 1); // Arrays start at 0. It should never reach 4.
    for (auto& player : players)
    {
        if (player.second.inUse && player.second.uniquePlayerId == playernum)
        {
            return player.second;
        }
    }

    // Player not found!
    static POP3NETWORK_PLAYERINFO ret{};
    ret.inUse = FALSE;
    return ret;
}

int Pop3Network::GetPlayerCount()
{
    int count = 0;

    for (auto& player : players)
    {
        if (player.second.inUse == TRUE)
        {
            count++;
        }
    }

    return count;
}

bool Pop3Network::am_i_host() const
{
    return am_host;
}

const NetworkStatus Pop3Network::getStatus() const
{

    return network_status;
}

const int Pop3Network::getRetries() const
{
    return connection_retries;
}

const std::list<class PacketInfo>& Pop3Network::getPacketInfo()
{
    return packets;
}

void Pop3Network::transfer_file(DWORD to, const std::string & file_name, const char * data, size_t length)
{
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(length) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    FT.FileParts.resize(number_of_parts);

    auto idx = 0;
    auto file_ptr = data;
    auto end_ptr = data + length;

    while ((file_ptr + PART_PACKET_DATA_SIZE) < end_ptr)
    {
        FT.FileParts[idx].packet_num = idx;
        memcpy(&FT.FileParts[idx].data, file_ptr, PART_PACKET_DATA_SIZE);
        file_ptr += PART_PACKET_DATA_SIZE;
        idx++;
    }

    // Copy the last packet
    auto remianing_size = end_ptr - file_ptr;

    if (remianing_size)
    {
        FT.FileParts[idx].packet_num = idx;
        memcpy(&FT.FileParts[idx].data, file_ptr, remianing_size);
    }

    memcpy(&FT.FileHeader.file_name[0], file_name.c_str(), file_name.length());
    FT.FileHeader.file_size = length;
    FT.SleepTimer = 0;

    // Host is ready to transmit, send start packet.
    Pop3Debug::trace("Sending file...");
    Send(to, Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER, reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
}

FileTransferStatus Pop3Network::getFileTransferStatus()
{
    return FT.Status;
}

unsigned int Pop3Network::getFileTransferPercent()
{
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    return static_cast<unsigned int>((FT.FilePartsRecv.size()/static_cast<double>(number_of_parts))*100);
}

size_t Pop3Network::getFileTransferTotalBytes()
{
    return FT.FileHeader.file_size;
}

size_t Pop3Network::getFileTransferRecievedBytes()
{
    return FT.FilePartsRecv.size()*PART_PACKET_DATA_SIZE;
}

std::string Pop3Network::getFileTransferName()
{
    return FT.FileHeader.file_name;
}

ULONGLONG Pop3Network::getFileTransferSleepTimer()
{
    return FT.SleepTimer;
}

void Pop3Network::send_remove_player(SWORD player_id)
{
    if (player_id < 0) return;
    static char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    buf[0] = static_cast<char>(player_id);
    Send(NET_ALLPLAYERS, Pop3NetworkTypes::HOST_DELETE_PLAYER, buf, 1);
}

void Pop3Network::remove_player_impl(int player_id)
{
	for (auto it = players.begin(); it != players.end();)
	{
		if (it->second.uniquePlayerId == player_id)
		{
			it->second.inUse = FALSE;
			it = players.erase(it);
		} else it++;
	}
}

int Pop3Network::ProcessChat(const char * buf, int buf_len)
{
    if (am_host)
    {
        SendChatToAll(buf, buf_len);
    }

    // If message is from or to me
    if (buf[0] == player_num || (buf[1] & (1 << player_num)) > 0)
    {
        //if (!is_blocked(buf[0]))
        {
            ProcessPopData(buf[0], const_cast<char*>(&buf[2]), buf_len - 2, (int)Pop3NetworkTypes::POP_CHAT, reinterpret_cast<const void*>(&buf[1]));
        }
    }
    return TRUE;
}

int Pop3Network::SendChatToAll(const char * buf, int buf_len)
{
    if (am_host)
    {
        for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
        {
            // Only send chat packets to players that need them
            if ((buf[1] & (1 << i)) > 0)
            {
                Send(i, Pop3NetworkTypes::POP_CHAT, buf, buf_len);
            }
        }
    }
    return TRUE;
}

SWORD Pop3Network::get_player_id(const char * peer_address, UWORD peer_port)
{
    for (auto& player : players)
    {
        if (player.second.inUse && player.second.port == peer_port && strcmp(player.second.address, peer_address) == 0)
        {
            return player.second.uniquePlayerId;
        }
    }
    return -1;
}

void Pop3Network::check_join_request(const char * peer_address, UWORD peer_port, const char * buffer) const
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
        if (p.second.inUse)
        {
            if (p.second.uniquePlayerId == pn)
            {
                allowed_pn = false;
                break;
            }
        }
    }

    if (allow_joiners && /*(pn == DATA_NOT_SET || allowed_pn) &&*/
        (buffer[0] == MAJOR_VERSION && buffer[1] == MINOR_VERSION && version.s == BUILD_NUMBER))
    {
        Pop3Debug::trace("NET_HOST_ACCEPT_JOIN");
        Send(peer_address, peer_port, Pop3NetworkTypes::HOST_ACCECPT_JOIN);
    }
    else
    {
        Pop3Debug::trace("NET_HOST_DENY_JOIN");
        Send(peer_address, peer_port, Pop3NetworkTypes::HOST_DENY_JOIN);
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
        if (player.second.inUse)
        {
            buf[p + 0] = static_cast<char>(player.second.uniquePlayerId);
            //buf[p + 1] = player.dwFlags;
            buf[p + 1] = player.second.host;
            std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[p + 2]), &player.second.name[0], MAX_PLAYER_NAME_LEN);
            buf[0]++;
            p += 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN);
        }
    }

    Send(to_id, Pop3NetworkTypes::HOST_PLAYERS, buf, p);
}

void Pop3Network::send_add_player(SWORD player_id)
{
    if (player_id < 0) return;
    static char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    for (auto& player : players)
    {
        if (player.second.uniquePlayerId == player_id)
        {
            buf[0] = static_cast<char>(player.second.uniquePlayerId);
            //buf[1] = player.dwFlags;
            buf[1] = player.second.host;
            std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[2]), &player.second.name[0], MAX_PLAYER_NAME_LEN);
            break;
        }
    }
    Send(NET_ALLPLAYERS, Pop3NetworkTypes::HOST_ADD_PLAYER, buf, 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN));
}

void Pop3Network::SendMyInfo(const char * peer_address, UWORD peer_port) const
{
    static char buf[MAX_PACKET_SIZE];
    memset(buf, 0, MAX_PACKET_SIZE);
    // Player num
    buf[0] = *GamePtrs.RequestedPlayerNum;
    // Host
    buf[1] = FALSE;
    std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[2]), my_name.c_str(), MAX_PLAYER_NAME_LEN);
    Send(player_num, peer_address, peer_port, Pop3NetworkTypes::CLIENT_JOIN, buf, 2 + sizeof(UNICODE_CHAR) * (std::min(my_name.size(), static_cast<unsigned>(MAX_PLAYER_NAME_LEN))));
}

void Pop3Network::add_players(const char * peer_address, UWORD peer_port, char * buffer)
{
    char* buf = &buffer[1];
    // i increments over the number of players in data packet, not player IDs
    for (int i = 0; i < buffer[0]; i++)
    {
        SWORD player_number = /*(buf[0] == 255) ? -1 :*/ buf[0];
        bool is_host = buf[1] > 0;
        if (is_host)
        {
            add_player(peer_address, peer_port, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buf[2]));
        }
        else
        {
            add_player("UNK", 0, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buf[2]));
        }
        // player data = 2 + MAX_PLAYER_NAME_LEN
        buf += 2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN);
    }
}

#define PLAYER_SLOT_INVALID -1
SWORD Pop3Network::add_player(const char * peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR * player_name)
{
    Pop3Debug::trace("Adding player %d on ip %s:%d (%d) %ls", player_number, peer_address, peer_port, is_host, player_name);

    // Don't add duplicate of player
    if (players.count(player_name))
    {
        if (player_name == my_name)
        {
            player_num = players[player_name].uniquePlayerId;
        }

        strncpy(players[player_name].address, peer_address, MAX_ADDRESS);
        players[player_name].port = peer_port;

        return players[player_name].uniquePlayerId;
    }


    if (am_host)
    {
		// Don't add duplicate player numbers.
		for (const auto& player : players)
		{
			if (player.second.inUse)
			{
				if (player.second.uniquePlayerId == player_number)
				{
					Pop3Debug::trace("Duplicate player id! %d %d", player.second.uniquePlayerId, player_number);
					player_number = PLAYER_SLOT_INVALID;
				}
			}
		}

		// Lets find a free spot.
		bool valid;
		SWORD free_pn;
		for (free_pn = 0; free_pn < NETWORK_NUMBER_PLAYERS; free_pn++)
		{
			valid = true;
			for (const auto& player : players)
			{
				if (player.second.inUse)
				{
					if (player.second.uniquePlayerId == free_pn)
						valid = false;
				}
				else
					break;
			}

			if (valid)
				break;
		}

		if (free_pn >= NETWORK_NUMBER_PLAYERS)
		{
			Pop3Debug::trace("No open slots, cannot join!");
			return -1; // Server's full!
		}
        Pop3Debug::trace("Player number is used or invalid");
        if (player_number == PLAYER_SLOT_INVALID)
        {
            Pop3Debug::trace("Chosen new player number from unused player numbers %d", free_pn);
            player_number = free_pn;
        }
    }

    POP3NETWORK_PLAYERINFO player{};
    player.inUse = TRUE;
    player.uniquePlayerId = player_number;
    std::char_traits<UNICODE_CHAR>::copy(&player.name[0], player_name, MAX_PLAYER_NAME_LEN);
    player.name[MAX_PLAYER_NAME_LEN - 1] = '\0';
    player.blocked = false;// this->isBlocked(player.name);
    player.host = is_host;
    strncpy(player.address, peer_address, MAX_ADDRESS);
    player.port = peer_port;
    players[player.name] = player;

    // If the player added is me, save the number
    if (player.name == my_name)
    {
        Pop3Debug::trace("This person is me!");
        player_num = player_number;
        cv2.broadcast();
        network_status = NetworkStatus::Connected;
    }

    return player_number;
}

#define STORE(d) ptr= &d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));
#define STORE_I(d) ptr= d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));
#define STORE_PTR(d) ptr= d;memcpy(&buf[i++*sizeof(void*)+5], &ptr, sizeof(void*));
void Pop3Network::SendPointers(const char * peer_address, UWORD peer_port)
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

    BYTE voidbyte(0);

    // ReSharper disable once CppJoinDeclarationAndAssignment
    STORE_PTR(GamePtrs.PlayerNum); // OFF_TRIBE
    STORE_PTR(GamePtrs.GnsiFlags); // OFF_STATUS
    STORE_PTR(GamePtrs.StartLevelNumber); // OFF_LEVEL_INDEX
    STORE_PTR(GamePtrs.Allies); // OFF_ALLIES
    STORE_PTR(GamePtrs.name); // OFF_NAMES
    STORE_PTR(GamePtrs.StatValues); // OFF_PLAYER_STATS
    STORE_PTR(GamePtrs.NumPeopleOfType); // OFF_UNITS
    STORE_PTR(GamePtrs.CurrNumPlayers); // OFF_PLAYERS
    STORE(voidbyte); // OFF_ACTIVE_PLAYERS
    STORE_PTR(GamePtrs.frame_rate_draw); // OFF_FRAMERATE
    STORE_PTR(GamePtrs.PingTime); // OFF_PING
    STORE_PTR(GamePtrs.GameTurn); // OFF_GAME_TIME
    STORE_PTR(GamePtrs.PlayerMsg); // OFF_GAME_MSG
    STORE_PTR(GamePtrs.BuildingsAvailable);// OFF_BUILDING_PANEL
    STORE_PTR(GamePtrs.PlayerFlags); // OFF_GOD
    STORE_PTR(GamePtrs.spells_type_info); // OFF_SPELLS
    STORE_I(reinterpret_cast<void*>(GamePtrs.PlayerStructSize)); // OFF_PLAYER_DIFF
    STORE_PTR(GamePtrs.NumBuildingsOfType); // OFF_PLAYER_BUILDINGS
    STORE_PTR(GamePtrs.SpellsCast); // OFF_PLAYER_SPELLS
	
                                                             // OFF_RESYNC_ON
                                                             // OFF_RESYNC_COUNT
                                                             // OFF_CHEAT_FLAGS
                                                             // OFF_CHEAT_BYRNE
                                                             // OFF_OBJECT_LIST
                                                             // OFF_SHAMAN_LIMIT
                                                             // OFF_FORGE_SETTING
                                                             // OFF_GUEST_SPELL

    buf[4] = static_cast<char>(i);
    Send(0, peer_address, peer_port, Pop3NetworkTypes::VERSION_DATA, buf, 5 + i * sizeof(void*));
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
    Send(0, GamePtrs.RemoteIPAddress, *GamePtrs.RemotePort, Pop3NetworkTypes::CLIENT_JOIN_REQUEST, buf, i);
}

void Pop3Network::ParsePacket(char * buffer, DWORD buf_size, const char * peer_address, UWORD peer_port)
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

    filetransfer_process_timeout(peer_address, peer_port);

    switch ((Pop3NetworkTypes)buffer[1])
    {
    case Pop3NetworkTypes::CLIENT_JOIN_REQUEST:
        check_join_request(peer_address, peer_port, &buffer[2]);
        break;

    case Pop3NetworkTypes::CLIENT_JOIN:
        player_number = /*(buffer[2] == 255) ? -1 :*/ buffer[2];
        is_host = buffer[3] > 0;
        from_id = add_player(peer_address, peer_port, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buffer[4]));
        send_players(from_id);
        send_add_player(from_id);
        break;

    case Pop3NetworkTypes::CLIENT_QUIT:
        if (from_id < 0) return;
        send_remove_player(from_id);
        remove_player_impl(from_id);
        break;

    case Pop3NetworkTypes::HOST_ACCECPT_JOIN:
        player_num = DATA_NOT_SET;
        SendMyInfo(peer_address, peer_port);
        break;

    case Pop3NetworkTypes::HOST_DENY_JOIN:
        server_status = Pop3ErrorStatusCodes::BAD_VERSION;
        break;

    case Pop3NetworkTypes::HOST_PLAYERS:
        add_players(peer_address, peer_port, &buffer[2]);
        break;

    case Pop3NetworkTypes::HOST_ADD_PLAYER:
        if (from_id < 0) return;
        player_number = /*(buffer[2] == 255) ? -1 :*/ buffer[2];
        is_host = buffer[3] > 0;
        add_player("", 0, player_number, is_host, reinterpret_cast<UNICODE_CHAR*>(&buffer[4]));
        break;

    case Pop3NetworkTypes::HOST_DELETE_PLAYER:
        if (from_id < 0) return;
        remove_player_impl(buffer[2]);
        break;

    case Pop3NetworkTypes::POP_DATA:
        if (from_id < 0) return;
        ProcessPopData(from_id, &buffer[2], buf_size - 2, buffer[1], nullptr);
        break;

    case Pop3NetworkTypes::POP_CHAT:
        ProcessChat(&buffer[2], buf_size - 2);
        break;
    case Pop3NetworkTypes::POP_SCRIPT:
		if (GamePtrs.Script_Callback)
		{
			std::string data(&buffer[2]);
			GamePtrs.Script_Callback(data);
		}
        break;
    case Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER:
        filetransfer_client_process_fileheader(peer_address, peer_port, &buffer[2]);
        break;
    case Pop3NetworkTypes::CLIENT_READY_FOR_FILE_TRANSFER:
        filetransfer_host_process_client_ready(peer_address, peer_port);
        break;
    case Pop3NetworkTypes::HOST_SEND_FILE_PART:
        filetransfer_client_process_file_part(&buffer[2]);
        break;
    case Pop3NetworkTypes::HOST_SEND_FILE_TRANSFER_COMPLETE:
        filetransfer_client_process_all_parts_sent();
        break;
    case Pop3NetworkTypes::CLIENT_RESEND_FILE_PART:
        filetransfer_host_process_resend_request(&buffer[2]);
        break;
    case Pop3NetworkTypes::CLIENT_RESNET_FILE_PARTS_COMPLETE:
        filetransfer_host_process_resend_requests_complete();
        break;
    case Pop3NetworkTypes::CLIENT_FILE_TRANSFER_COMPLETE:
        filetransfer_host_process_client_transfer_successful();
        break;
    case  Pop3NetworkTypes::HOST_REQUESTING_UPDATE_ON_TRANSFER:
        filetransfer_host_requesting_update();
        break;
    default:
        break;
    }
    add_packet_info(pi);
}

void Pop3Network::compile_fileparts()
{
    // Compile and save fileparts to disk
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    std::ofstream out(Poco::Path::temp()  + "\\pop.dat" , std::ios::trunc | std::ios::binary);
    if (out.is_open())
    {
        // -1 so we don't write the last packet as it might not be fully complete.
        for (int i = 0; i < number_of_parts - 1; i++)
        {
            out.write(&FT.FilePartsRecv[i].data[0], sizeof(FT.FilePartsRecv[i].data));
        }

        int final_part = number_of_parts - 1;
        ASSERT(FT.FilePartsRecv.count(final_part));

        // Calculate last part size.
        auto size_without_final_part = (number_of_parts - 1) * PART_PACKET_DATA_SIZE;
        auto size_of_final_part = FT.FileHeader.file_size - size_without_final_part;
        out.write(&FT.FilePartsRecv[final_part].data[0], size_of_final_part);
        out.close();
    }

    FT.FilePartsRecv.clear();
}

void Pop3Network::filetransfer_client_process_fileheader(const char * peer_address, UWORD peer_port, const char * buffer)
{
    FT.LastContactTime = GetCurrentMs();
    FT.FileHeader = *reinterpret_cast<const PopTBFileStartPacket*>(&buffer[0]);
    Send(peer_address, peer_port, Pop3NetworkTypes::CLIENT_READY_FOR_FILE_TRANSFER);
    FT.Status = FileTransferStatus::Client_Waiting_On_Host_To_Start_Sending_File_Parts;
    FT.peer_address = peer_address;
    FT.peer_port = peer_port;
    FT.SleepTimer = 0;
}

void Pop3Network::filetransfer_host_process_client_ready(const char * peer_address, UWORD peer_port)
{
    FT.LastContactTime = GetCurrentMs();
    FT.Status = FileTransferStatus::Host_Transfering_Data;
    FT.peer_address = peer_address;
    FT.peer_port = peer_port;
    for (const auto & fp : FT.FileParts)
    {
        Send(-1, FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::HOST_SEND_FILE_PART, reinterpret_cast<const char*>(&fp), sizeof(fp));
        Poco::Thread::sleep(1);
    }
    Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::HOST_SEND_FILE_TRANSFER_COMPLETE);
    FT.Status = FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer;
}

void Pop3Network::filetransfer_client_process_file_part(const char * buffer)
{
    if (FT.Status == FileTransferStatus::Transfer_Complete)
        return;

    FT.LastContactTime = GetCurrentMs();
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    auto fp_ptr = reinterpret_cast<const PopTBFilePartPacket*>(&buffer[0]);

    if (!FT.FilePartsRecv.count(fp_ptr->packet_num)) // Check if we already have this packet.
        FT.FilePartsRecv[fp_ptr->packet_num] = *fp_ptr;

    if (FT.FilePartsRecv.size() == number_of_parts)
    {
        FT.Status = FileTransferStatus::Transfer_Complete;
        Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_FILE_TRANSFER_COMPLETE);

        // Compile FileParts.
        compile_fileparts();

        memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
        FT.FileParts.clear();
        FT.FilePartsToBeResent.clear();
        FT.LastContactTime = 0;
        FT.peer_address.clear();
        FT.peer_port = 0;
    }
    else
        FT.Status = FileTransferStatus::Client_Waiting_On_Host_Parts_To_Finish;
}

#define PACKET_RETRY_PEN (05)
void Pop3Network::filetransfer_client_process_all_parts_sent()
{
    if (FT.Status == FileTransferStatus::Transfer_Complete)
        return;

    if ((GetCurrentMs() - FT.LastContactTime) < 100)
        return;

    FT.LastContactTime = GetCurrentMs();

    // Now we check if any fileparts need to be resent.
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));

    bool packet_resent = false;

    FT.Status = FileTransferStatus::Client_Sending_Resend_Requests;

    std::vector<int> missingPackets;

    // We need some packets to be resent.
    for (int i = 0; i < number_of_parts; i++)
        if (!FT.FilePartsRecv.count(i))
            missingPackets.push_back(i);

    FT.SleepTimer += static_cast<ULONGLONG>((missingPackets.size() / static_cast<double>(number_of_parts)) * PACKET_RETRY_PEN);

    for (const auto & pkt : missingPackets)
    {
        PopTBFilePartRequest part;
        part.packet_num = pkt;
        packet_resent = true;
        Send(-1, FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_RESEND_FILE_PART, reinterpret_cast<char*>(&part), sizeof(PopTBFilePartRequest));
        if (FT.SleepTimer)
            Poco::Thread::sleep(static_cast<long>(FT.SleepTimer));
    }

    if (packet_resent)
    {
        FT.Status = FileTransferStatus::Client_Waiting_On_Host_Parts_To_Finish;
        Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_RESNET_FILE_PARTS_COMPLETE);
    }
    else FT.Status = FileTransferStatus::Transfer_Complete;
}

void Pop3Network::filetransfer_host_process_resend_request(const char * buffer)
{
    FT.LastContactTime = GetCurrentMs();
    auto fp_ptr = reinterpret_cast<const PopTBFilePartRequest*>(&buffer[0]);
    FT.FilePartsToBeResent.push_back(*fp_ptr);
    FT.Status = FileTransferStatus::Host_Waiting_On_Client_To_Finish_Resend_Requests;
}

void Pop3Network::filetransfer_host_process_resend_requests_complete()
{
    FT.LastContactTime = GetCurrentMs();
    FT.Status = FileTransferStatus::Host_Transfering_Data;
    FT.SleepTimer += static_cast<ULONGLONG>((FT.FilePartsToBeResent.size() / static_cast<double>(FT.FileParts.size())) * PACKET_RETRY_PEN);
    for (const auto & f : FT.FilePartsToBeResent)
    {
        Send(-1, FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::HOST_SEND_FILE_PART, reinterpret_cast<char*>(&FT.FileParts[f.packet_num]), sizeof(FT.FileParts[f.packet_num]));
        if (FT.SleepTimer)
            Poco::Thread::sleep(static_cast<long>(FT.SleepTimer));
    }
    FT.FilePartsToBeResent.clear();
    Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::HOST_SEND_FILE_TRANSFER_COMPLETE);
    FT.Status = FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer;
}

void Pop3Network::filetransfer_host_process_client_transfer_successful()
{
    // Host.
    FT.LastContactTime = GetCurrentMs();
    memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
    FT.FileParts.clear();
    FT.FilePartsRecv.clear();
    FT.FilePartsToBeResent.clear();
    FT.LastContactTime = 0;
    FT.peer_address.clear();
    FT.peer_port = 0;
    FT.Status = FileTransferStatus::Transfer_Complete;
}

void Pop3Network::filetransfer_process_timeout(const char * peer_address, UWORD peer_port)
{
    if (FT.Status != FileTransferStatus::Ready && FT.Status != FileTransferStatus::Transfer_Complete)
    {
        if (FT.peer_address == peer_address && FT.peer_port == peer_port)
        {
            if ((GetCurrentMs() - FT.LastContactTime) > 1000)
            {
                FT.LastContactTime = GetCurrentMs();
                switch (FT.Status)
                {
                case FileTransferStatus::Host_Waiting_On_Client_Ack:
                    Send(-1, peer_address, peer_port, Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER, reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
                    break;
                case FileTransferStatus::Host_Waiting_On_Client_Resend_Complete:
                case FileTransferStatus::Host_Waiting_On_Client_To_Finish_Resend_Requests:
                case FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer:
                    Send(-1, peer_address, peer_port, Pop3NetworkTypes::HOST_REQUESTING_UPDATE_ON_TRANSFER, nullptr, 0);
                    break;
                }
            }
        }
    }
}

void Pop3Network::filetransfer_host_requesting_update()
{
    switch (FT.Status)
    {
    case FileTransferStatus::Transfer_Complete:
        Send(-1, FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_FILE_TRANSFER_COMPLETE, reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
        break;
    case FileTransferStatus::Client_Waiting_On_Host_To_Start_Sending_File_Parts:
        Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_READY_FOR_FILE_TRANSFER);
        break;
    case FileTransferStatus::Client_Waiting_On_Host_Parts_To_Finish:
        filetransfer_client_process_all_parts_sent();
        break;
    }
    FT.LastContactTime = GetCurrentMs();
}

void Pop3Network::Send(const char * peer_address, UWORD peer_port, Pop3NetworkTypes type) const
{
    Send(-1, peer_address, peer_port, type, nullptr, 0);
}

void Pop3Network::Send(int to_id, Pop3NetworkTypes type)
{
    Send(to_id, type, nullptr, 0);
}

void Pop3Network::Send(int to_id, Pop3NetworkTypes type, const char * buffer, DWORD buf_size)
{
    for (auto& player : players)
    {
        if ((player.second.inUse) && (// This should be first in the IF statement.			
            (to_id == static_cast<signed>(NET_SERVERPLAYERS) && player.second.host) ||
            (to_id == static_cast<signed>(NET_ALLPLAYERS) ||
                to_id == player.second.uniquePlayerId)))
        {
            Send(player.second.uniquePlayerId, player.second.address, player.second.port, type, buffer, buf_size);
        }
    }
}

void Pop3Network::host_watchdog_func()
{
    network_status = NetworkStatus::Connected;

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
    network_status = NetworkStatus::Joining;

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
            if ((cv2.tryWait(cv_m, CONNECT_INTERVAL * 1000) && server_status == Pop3ErrorStatusCodes::OK) || network_status == NetworkStatus::Connected)
                break;

            if (server_status != Pop3ErrorStatusCodes::OK)
                server_status = Pop3ErrorStatusCodes::OK;

            Pop3Debug::trace("Connecting (%d)...", ++connection_retries);
        }
        else first_time = false;

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