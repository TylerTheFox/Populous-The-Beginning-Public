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
#include  <Poco/Net/IPAddress.h>

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
    // Fallback only -- Pop3NetworkUDP::~Pop3NetworkUDP must call this first,
    // while the derived object (and its virtual Send) is still alive. Joining
    // here also keeps a joinable std::thread member from calling terminate().
    shutdown_watchdogs();
}

void Pop3Network::shutdown_watchdogs()
{
    m_shutdown = true;
    cv2.broadcast(); // wake join_watchdog_func out of its 2s tryWait now
    if (m_host_watchdog.joinable())
        m_host_watchdog.join();
    if (m_join_watchdog.joinable())
        m_join_watchdog.join();
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
        m_host_watchdog = std::thread(&Pop3Network::host_watchdog_func, this);
        Pop3Debug::trace("I am host and my player number is %d", player_num);
    }
    else
    {
        Pop3Debug::trace("I am joining and my requested player num is %d", *GamePtrs.RequestedPlayerNum);
        m_join_watchdog = std::thread(&Pop3Network::join_watchdog_func, this);
    }
    *GamePtrs.NetLobbied = TRUE;
    return Pop3ErrorStatusCodes::OK;
}

BOOL Pop3Network::GetPlayerInfo(POP3NETWORK_PLAYERINFO * out_player)
{
    for (int i = 0; i < NETWORK_NUMBER_PLAYERS; i++)
    {
        out_player[i].inUse = FALSE;
        out_player[i].dwFlags = 0;
    }

    Poco::Mutex::ScopedLock lock(players_mu);
    for (auto& player : players)
    {
        if (player.second.inUse && player.second.uniquePlayerId > -1 && player.second.uniquePlayerId < NETWORK_NUMBER_PLAYERS)
        {
            out_player[player.second.uniquePlayerId].inUse = player.second.inUse;
            out_player[player.second.uniquePlayerId].host = player.second.host;
            out_player[player.second.uniquePlayerId].dwFlags = player.second.dwFlags;
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
    char buf[MAX_PACKET_SIZE];
    int len = sizeof(UNICODE_CHAR) * (std::char_traits<UNICODE_CHAR>::length(message) + 1);
    buf[0] = static_cast<char>(player_num); // from
    buf[1] = chat_targets; // to
    // Add self to the 0..N-1 target bitmask. A spectator (id >= NETWORK_NUMBER_
    // PLAYERS) has no bit here and only ever uses CHAT_ALL, and (1 << id) would
    // be out of range / UB for a large spectator id, so skip the shift for it.
    if (player_num < NETWORK_NUMBER_PLAYERS)
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
    // No clamp: spectator ids are >= NETWORK_NUMBER_PLAYERS and must resolve to
    // their real map entry. This is a map search, not an array index, and an
    // unmatched id returns the static empty (inUse=FALSE) record below.
    Poco::Mutex::ScopedLock lock(players_mu);
    for (auto& player : players)
    {
        if (player.second.inUse && player.second.uniquePlayerId == playernum)
        {
            return player.second;
        }
    }

    static POP3NETWORK_PLAYERINFO ret{};
    ret.inUse = FALSE;
    return ret;
}

void Pop3Network::GetPlayerNameCopy(UBYTE playernum, UNICODE_CHAR* out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = 0;
    Poco::Mutex::ScopedLock lock(players_mu);
    for (auto& player : players)
    {
        if (player.second.inUse && player.second.uniquePlayerId == playernum)
        {
            std::char_traits<UNICODE_CHAR>::copy(out, &player.second.name[0], out_len - 1);
            out[out_len - 1] = 0;
            return;
        }
    }
}

// Real players only (ids inside the fixed game slots). Spectators are
// routing-only peers and must not count towards "is anyone still playing"
// decisions (e.g. the dedicated-server empty check).
int Pop3Network::GetGamePlayerCount()
{
    Poco::Mutex::ScopedLock lock(players_mu);
    int count = 0;
    for (auto& player : players)
        if (player.second.inUse == TRUE && player.second.uniquePlayerId >= 0
            && player.second.uniquePlayerId < NETWORK_NUMBER_PLAYERS)
            count++;
    return count;
}

int Pop3Network::GetPlayerCount()
{
    Poco::Mutex::ScopedLock lock(players_mu);
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
    Poco::Mutex::ScopedLock lock(ft_mu);

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
    auto remaining_size = end_ptr - file_ptr;

    if (remaining_size)
    {
        FT.FileParts[idx].packet_num = idx;
        memcpy(&FT.FileParts[idx].data, file_ptr, remaining_size);
    }

    memcpy(&FT.FileHeader.file_name[0], file_name.c_str(), file_name.length());
    FT.FileHeader.file_size = length;
    FT.SleepTimer = 0;

    // Init sliding window
    FT.window_start = 0;
    FT.window_size = FT_INITIAL_WINDOW_SIZE;
    FT.total_parts = static_cast<unsigned int>(number_of_parts);
    FT.retry_count = 0;

    // Capture the recipient's id AND endpoint NOW (lock order ft_mu ->
    // players_mu, same as the Send(to,...) below). Previously the endpoint
    // was only learned from the client's READY reply, so a lost first
    // header datagram left every retry sending into an empty address and
    // the transfer slot busy for the rest of the session - all later sync
    // requests were then silently ignored.
    FT.peer_id = -1;
    FT.peer_address.clear();
    FT.peer_port = 0;
    {
        Poco::Mutex::ScopedLock plock(players_mu);
        for (auto& player : players)
        {
            if (player.second.inUse && player.second.uniquePlayerId == static_cast<SWORD>(to))
            {
                FT.peer_id = player.second.uniquePlayerId;
                FT.peer_address = player.second.address;
                FT.peer_port = player.second.port;
                break;
            }
        }
    }
    if (FT.peer_id < 0 || FT.peer_address.empty() || FT.peer_port == 0)
    {
        Pop3Debug::trace("transfer_file: no routable peer for id %u - not starting", to);
        FT.Status = FileTransferStatus::Ready;
        FT.FileParts.clear();
        FT.total_parts = 0;
        return;
    }

    FT.Status = FileTransferStatus::Host_Waiting_On_Client_Ack;
    FT.LastContactTime = GetCurrentMs();

    Pop3Debug::trace("Sending file '%s' (%u parts, window=%u) to player %d...",
                     FT.FileHeader.file_name, FT.total_parts, FT.window_size, (int)FT.peer_id);
    Send(-1, FT.peer_address.c_str(), FT.peer_port,
         Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER,
         reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
}

// ft_mu already held. Tell the peer the transfer is dead, then reset to
// Ready so the next sync request can start a fresh one (previously several
// states could never leave "busy", permanently disabling the sync system
// for the session).
void Pop3Network::filetransfer_abort_locked(const char * reason)
{
    Pop3Debug::trace("File transfer aborted: %s", reason);
    if (!FT.peer_address.empty() && FT.peer_port)
        Send(-1, FT.peer_address.c_str(), FT.peer_port,
             Pop3NetworkTypes::HOST_TRANSFER_ABORTED, nullptr, 0);
    memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
    FT.FileParts.clear();
    FT.total_parts = 0;
    FT.FilePartsRecv.clear();
    FT.peer_address.clear();
    FT.peer_port = 0;
    FT.peer_id = -1;
    FT.retry_count = 0;
    FT.Status = FileTransferStatus::Ready;
}

FileTransferStatus Pop3Network::getFileTransferStatus()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.Status;
}

unsigned int Pop3Network::getFileTransferPercent()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    switch (FT.Status)
    {
    case FileTransferStatus::Host_Waiting_On_Client_Ack:
    case FileTransferStatus::Host_Sending_Window:
    case FileTransferStatus::Host_Waiting_On_Window_Ack:
    case FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer:
        // Sending side: FilePartsRecv is only ever filled by the RECEIVER,
        // so the old maths pinned the host at 0% - report acked-window
        // progress instead.
        if (FT.total_parts == 0) return 0;
        return static_cast<unsigned int>((FT.window_start / static_cast<double>(FT.total_parts)) * 100);
    default:
        break;
    }
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    if (number_of_parts == 0) return 0;
    return static_cast<unsigned int>((FT.FilePartsRecv.size()/static_cast<double>(number_of_parts))*100);
}

SWORD Pop3Network::getFileTransferPeerId()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.peer_id;
}

size_t Pop3Network::getFileTransferTotalBytes()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.FileHeader.file_size;
}

size_t Pop3Network::getFileTransferRecievedBytes()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.FilePartsRecv.size()*PART_PACKET_DATA_SIZE;
}

std::string Pop3Network::getFileTransferName()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.FileHeader.file_name;
}

ULONGLONG Pop3Network::getFileTransferSleepTimer()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    return FT.SleepTimer;
}

void Pop3Network::send_remove_player(SWORD player_id)
{
    if (player_id < 0) return;
    char buf[MAX_PACKET_SIZE];

    memset(buf, 0, MAX_PACKET_SIZE);
    buf[0] = static_cast<char>(player_id);
    Send(NET_ALLPLAYERS, Pop3NetworkTypes::HOST_DELETE_PLAYER, buf, 1);
}

void Pop3Network::remove_player_impl(int player_id)
{
	Poco::Mutex::ScopedLock lock(players_mu);
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

    // If message is from or to me. A local spectator (player_num >= NETWORK_
    // NUMBER_PLAYERS) has no bit in the 0..N-1 mask and (1 << player_num) would
    // be UB, so it receives all-chat via the CHAT_ALL check instead.
    bool to_me = (buf[0] == player_num);
    if (!to_me)
    {
        if (player_num < NETWORK_NUMBER_PLAYERS)
            to_me = ((buf[1] & (1 << player_num)) > 0);
        else
            to_me = (static_cast<BYTE>(buf[1]) == static_cast<BYTE>(CHAT_ALL));
    }
    if (to_me)
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
        // Spectators live outside the 0..N-1 chat bitmask, so the loop above can
        // never address them. Relay all-chat to every connected spectator
        // (id >= NETWORK_NUMBER_PLAYERS) explicitly. Collect ids under the lock,
        // then Send (which re-locks internally) outside it.
        if (static_cast<BYTE>(buf[1]) == static_cast<BYTE>(CHAT_ALL))
        {
            std::vector<SWORD> spec_ids;
            {
                Poco::Mutex::ScopedLock lock(players_mu);
                for (const auto& p : players)
                    if (p.second.inUse && p.second.uniquePlayerId >= NETWORK_NUMBER_PLAYERS)
                        spec_ids.push_back(p.second.uniquePlayerId);
            }
            for (SWORD sid : spec_ids)
                Send(sid, Pop3NetworkTypes::POP_CHAT, buf, buf_len);
        }
    }
    return TRUE;
}

SWORD Pop3Network::get_player_id(const char * peer_address, UWORD peer_port)
{
    Poco::Mutex::ScopedLock lock(players_mu);
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
    ASSERT(pn < NETWORK_NUMBER_PLAYERS || pn == static_cast<BYTE>(DATA_NOT_SET)); // slot SPECTATOR_SLOT(9) is a valid spectator request

    Pop3Debug::trace("Join request recieved from %s on port %d requesting player number %d on build %d.%d.%d", peer_address, peer_port, pn, buffer[0], buffer[1], version.s);

    Poco::Mutex::ScopedLock lock(players_mu);
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
        Send(peer_address, peer_port, Pop3NetworkTypes::HOST_ACCEPT_JOIN);
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
    Poco::Mutex::ScopedLock lock(players_mu);
    int p = 1;
    char buf[MAX_PACKET_SIZE];
    memset(buf, 0, sizeof(buf));
    buf[0] = 0; // number of players

    const int REC = 2 + (int)(sizeof(UNICODE_CHAR) * MAX_PLAYER_NAME_LEN);
    for (auto& player : players)
    {
        if (player.second.inUse)
        {
            // Spectators (ids >= NETWORK_NUMBER_PLAYERS) are routing-only
            // peers with an open-ended census - the roster carries the fixed
            // game slots plus, for a joining spectator, its OWN record (how
            // it learns its assigned id). Anything more overflows the
            // receiver's fixed-size parse (join hang when the joiner's own
            // record fell past the clamp).
            if (player.second.uniquePlayerId >= NETWORK_NUMBER_PLAYERS
                && player.second.uniquePlayerId != to_id)
                continue;
            if (p + REC > (int)sizeof(buf))
            {
                Pop3Debug::trace("send_players: roster truncated at %d records (buffer full)", (int)buf[0]);
                break;
            }
            buf[p + 0] = static_cast<char>(player.second.uniquePlayerId);
            //buf[p + 1] = player.dwFlags;
            buf[p + 1] = player.second.host;
            std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[p + 2]), &player.second.name[0], MAX_PLAYER_NAME_LEN);
            buf[0]++;
            p += REC;
        }
    }

    Send(to_id, Pop3NetworkTypes::HOST_PLAYERS, buf, p);
}

void Pop3Network::send_add_player(SWORD player_id)
{
    if (player_id < 0) return;
    Poco::Mutex::ScopedLock lock(players_mu);
    char buf[MAX_PACKET_SIZE];

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
    char buf[MAX_PACKET_SIZE];
    memset(buf, 0, MAX_PACKET_SIZE);
    // Player num
    buf[0] = *GamePtrs.RequestedPlayerNum;
    // Host
    buf[1] = FALSE;
    // Copy only the real name (copying MAX_PLAYER_NAME_LEN reads past the end
    // of my_name's buffer) and send the null terminator with it: the host
    // stores the name verbatim and echoes it back in HOST_PLAYERS, and we
    // recognize ourselves by exact name match.
    const size_t name_len = std::min<size_t>(my_name.size(), MAX_PLAYER_NAME_LEN - 1);
    std::char_traits<UNICODE_CHAR>::copy(reinterpret_cast<UNICODE_CHAR *>(&buf[2]), my_name.c_str(), name_len);
    Send(player_num, peer_address, peer_port, Pop3NetworkTypes::CLIENT_JOIN, buf, static_cast<DWORD>(2 + sizeof(UNICODE_CHAR) * (name_len + 1)));
}

void Pop3Network::add_players(const char * peer_address, UWORD peer_port, char * buffer, DWORD payload_size)
{
    if (payload_size < 1)
        return;

    // Bound the record count by both the declared count and the actual payload
    // size so a malicious/truncated HOST_PLAYERS datagram cannot walk past the
    // receive buffer.
    const DWORD REC = static_cast<DWORD>(2 + sizeof(UNICODE_CHAR) * (MAX_PLAYER_NAME_LEN));
    int count = static_cast<unsigned char>(buffer[0]);
    // Fixed game slots + (for a joining spectator) its own out-of-slot record.
    if (count > NETWORK_NUMBER_PLAYERS + 1)
        count = NETWORK_NUMBER_PLAYERS + 1;
    DWORD max_by_size = (payload_size - 1) / REC;
    if (static_cast<DWORD>(count) > max_by_size)
        count = static_cast<int>(max_by_size);

    char* buf = &buffer[1];
    // i increments over the number of players in data packet, not player IDs
    for (int i = 0; i < count; i++)
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
        buf += REC;
    }
}

#define PLAYER_SLOT_INVALID -1
SWORD Pop3Network::add_player(const char * peer_address, UWORD peer_port, SWORD player_number, bool is_host, UNICODE_CHAR * player_name)
{
    Poco::Mutex::ScopedLock lock(players_mu);
    Pop3Debug::trace("Adding player %d on ip %s:%d (%d) %ls", player_number, peer_address, peer_port, is_host, player_name);

    // Don't add duplicate of player
    if (players.count(player_name))
    {
        // The map is keyed by NAME. Refreshing the stored endpoint is only
        // legitimate when the record is the same peer (same endpoint) or has
        // no live route yet ("UNK"/empty placeholder from roster records).
        // A host must NOT let a DIFFERENT live endpoint claim an existing
        // name: that redirected the original peer's route (incl. its
        // SERVER_GAMETURN stream) to the newcomer - the original silently
        // stalled out. Duplicate default profile names made this easy to hit
        // with spectators. Reject the newcomer instead (it retries and the
        // user must pick a distinct name).
        POP3NETWORK_PLAYERINFO& existing = players[player_name];
        const bool same_endpoint = (strncmp(existing.address, peer_address, MAX_ADDRESS) == 0)
                                && (existing.port == peer_port);
        const bool unroutable = (existing.address[0] == '\0')
                             || (strcmp(existing.address, "UNK") == 0);
        // A join may NEVER claim the host's row: the host self-adds with an
        // empty address (satisfying `unroutable`), so without the host check
        // a joiner using the host's name would hijack it and the resulting
        // HOST_ADD_PLAYER echo would zero every client's stored host route.
        if (am_host && existing.inUse
            && (existing.host || (!same_endpoint && !unroutable)))
        {
            Pop3Debug::trace("Join REJECTED: name '%ls' already connected from %s:%d (newcomer %s:%d) - duplicate names cannot share a session",
                             player_name, existing.address, (int)existing.port, peer_address, (int)peer_port);
            return -1;
        }

        if (player_name == my_name)
        {
            player_num = existing.uniquePlayerId;
        }

        strncpy(existing.address, peer_address, MAX_ADDRESS);
        existing.port = peer_port;

        return existing.uniquePlayerId;
    }


    const bool wants_spectator = (player_number == SPECTATOR_SLOT);
    if (am_host && wants_spectator)
    {
        // Unlimited spectators: assign the lowest free network id at or above
        // NETWORK_NUMBER_PLAYERS. Spectator ids live OUTSIDE the fixed 0..N-1
        // game slots (GetPlayerInfo skips ids >= NETWORK_NUMBER_PLAYERS), so they
        // never enter any peer's player array and never touch the sim/checksum.
        // This is routing state, not sim state: it only needs to be collision-
        // free, not deterministic.
        SWORD id = NETWORK_NUMBER_PLAYERS;
        for (;;)
        {
            bool taken = false;
            for (const auto& p : players)
                if (p.second.inUse && p.second.uniquePlayerId == id) { taken = true; break; }
            if (!taken) break;
            id++;
        }
        player_number = id;
    }
    else if (am_host)
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
		for (free_pn = 0; free_pn < SPECTATOR_SLOT; free_pn++) // never auto-assign a real player into the reserved spectator slot
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

		if (free_pn >= SPECTATOR_SLOT)
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
    // Slot SPECTATOR_SLOT is reserved exclusively for spectators, so the
    // spectator cap is a pure function of the slot number: every peer derives
    // it identically, with no change to the on-wire player record.
    player.dwFlags = (player_number >= NETWORK_NUMBER_PLAYERS) ? POP3NETWORK_PLAYERCAPS_SPECTATOR : 0;
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
    char buf[MAX_PACKET_SIZE];
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
    char buf[MAX_PACKET_SIZE];

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
    // Minimum valid packet: 2 bytes header (packet type + message type) + at least 1 byte payload for most types
    if (buf_size < 2) return;

    bool is_host;
    SWORD player_number;
    SWORD from_id = get_player_id(peer_address, peer_port);

    // Host-side liveness / reaping for spectators (id >= NETWORK_NUMBER_PLAYERS).
    // They are not in the 0..N-1 dropout loop, so refresh a per-id timer on any
    // packet and periodically drop spectators that have gone silent (crash /
    // lost connection). Host network-thread only: m_spectator_last_contact is
    // touched nowhere else, so it needs no lock; removal uses the map-locked
    // helpers. Pure network housekeeping - no sim state, cannot desync.
    if (am_host)
    {
        const ULONGLONG now = GetCurrentMs();
        if (from_id >= NETWORK_NUMBER_PLAYERS)
            m_spectator_last_contact[from_id] = now;

        if (now - m_last_spectator_sweep_ms >= 5000)
        {
            m_last_spectator_sweep_ms = now;
            std::vector<SWORD> stale;
            for (const auto& kv : m_spectator_last_contact)
                if (now - kv.second >= 120000)  // 120s idle -> reap (long enough to ride out a level load, where the engine sends no heartbeats)
                    stale.push_back(kv.first);
            for (SWORD sid : stale)
            {
                Pop3Debug::trace("Reaping idle spectator id %d", sid);
                m_spectator_last_contact.erase(sid);
                send_remove_player(sid);
                remove_player_impl(sid);
            }
        }
    }

    PacketInfo pi;
    pi.from_id = from_id;
    pi.to_id = player_num;
    pi.packetSize = buf_size;
    pi.peer_address = peer_address;
    pi.port = peer_port;
    pi.packet_num = buffer[1];
    pi.time = getGameClockMiliseconds();

    switch ((Pop3NetworkTypes)buffer[1])
    {
    case Pop3NetworkTypes::CLIENT_JOIN_REQUEST:
        check_join_request(peer_address, peer_port, &buffer[2]);
        break;

    case Pop3NetworkTypes::CLIENT_JOIN:
        // Only the HOST admits peers, and only while joining is allowed. The
        // second handshake step must honour allow_joiners too - a late/
        // duplicated/crafted CLIENT_JOIN after game start would add a peer
        // mid-game (no lockstep state transfer: guaranteed divergence). And a
        // crafted CLIENT_JOIN aimed at a CLIENT would otherwise make it add a
        // phantom entry and broadcast HOST_ADD_PLAYER to everyone.
        if (!am_host || !allow_joiners)
        {
            Pop3Debug::trace("CLIENT_JOIN from %s:%d ignored (%s)", peer_address, (int)peer_port,
                             am_host ? "joining disabled - game started" : "not the host");
            break;
        }
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

    case Pop3NetworkTypes::HOST_ACCEPT_JOIN:
        if (am_host)
            break;
        player_num = DATA_NOT_SET;
        SendMyInfo(peer_address, peer_port);
        break;

    case Pop3NetworkTypes::HOST_DENY_JOIN:
        if (am_host)
            break;
        server_status = Pop3ErrorStatusCodes::BAD_VERSION;
        break;

    case Pop3NetworkTypes::HOST_PLAYERS:
        // Only the host defines the player list; a client processing a spoofed
        // HOST_PLAYERS could otherwise be renumbered/hijacked. Address-only
        // check: the host's UDP port may legitimately rebind under NAT (see
        // the bind-not-connect comment in Pop3NetworkUDP::RunServer).
        if (am_host)
            break;
        // Compare PARSED addresses: peer_address is Poco's canonical numeric
        // form while RemoteIPAddress is the raw typed --join text (127.1,
        // zero-padded octets, ... would never strcmp-match and the join would
        // hang). Unparseable/empty target -> skip the check.
        if (GamePtrs.RemoteIPAddress && GamePtrs.RemoteIPAddress[0])
        {
            try
            {
                if (Poco::Net::IPAddress(std::string(peer_address)) !=
                    Poco::Net::IPAddress(std::string(GamePtrs.RemoteIPAddress)))
                    break;
            }
            catch (...) {}
        }
        add_players(peer_address, peer_port, &buffer[2], buf_size - 2);
        break;

    case Pop3NetworkTypes::HOST_ADD_PLAYER:
        // Only the host defines the roster (mirror of the HOST_PLAYERS spoof
        // guard): the host processing a reflected/crafted HOST_ADD_PLAYER
        // would run its slot assignment and park a phantom player in a free
        // slot, stalling its check-in loop.
        if (am_host)
            break;
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
        filetransfer_client_process_window_complete(&buffer[2]);
        break;
    case Pop3NetworkTypes::CLIENT_RESEND_FILE_PART:
        filetransfer_host_process_window_ack(&buffer[2]);
        break;
    case Pop3NetworkTypes::CLIENT_RESNET_FILE_PARTS_COMPLETE:
        break; // no longer used in sliding window protocol
    case Pop3NetworkTypes::CLIENT_FILE_TRANSFER_COMPLETE:
        filetransfer_host_process_client_transfer_successful();
        break;
    case  Pop3NetworkTypes::HOST_REQUESTING_UPDATE_ON_TRANSFER:
        filetransfer_host_requesting_update(peer_address, peer_port);
        break;
    case Pop3NetworkTypes::HOST_TRANSFER_ABORTED:
        // Host->client only: a host processing this (crafted/reflected)
        // would tear down its own in-flight transfer to a DIFFERENT client.
        if (am_host)
            break;
        filetransfer_client_process_aborted();
        break;
    default:
        break;
    }
    add_packet_info(pi);
}

void Pop3Network::compile_fileparts()
{
    size_t number_of_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));
    std::ofstream out(Poco::Path::temp() + "\\pop.dat", std::ios::trunc | std::ios::binary);
    if (out.is_open())
    {
        for (size_t i = 0; i + 1 < number_of_parts; i++)
            out.write(&FT.FilePartsRecv[static_cast<unsigned int>(i)].data[0], sizeof(FT.FilePartsRecv[0].data));

        unsigned int final_part = static_cast<unsigned int>(number_of_parts - 1);
        ASSERT(FT.FilePartsRecv.count(final_part));
        auto size_of_final_part = FT.FileHeader.file_size - (number_of_parts - 1) * PART_PACKET_DATA_SIZE;
        out.write(&FT.FilePartsRecv[final_part].data[0], size_of_final_part);
        out.close();
    }
    FT.FilePartsRecv.clear();
}

// ---------------------------------------------------------------------------
//  Sliding-window file transfer implementation
// ---------------------------------------------------------------------------

void Pop3Network::filetransfer_client_process_fileheader(const char * peer_address, UWORD peer_port, const char * buffer)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    FT.LastContactTime = GetCurrentMs();
    FT.FileHeader = *reinterpret_cast<const PopTBFileStartPacket*>(buffer);
    FT.peer_address = peer_address;
    FT.peer_port = peer_port;
    FT.FilePartsRecv.clear();
    FT.SleepTimer = 0;
    FT.Status = FileTransferStatus::Client_Waiting_On_Host_To_Start;

    Pop3Debug::trace("File transfer starting: %s (%zu bytes)", FT.FileHeader.file_name, FT.FileHeader.file_size);
    Send(peer_address, peer_port, Pop3NetworkTypes::CLIENT_READY_FOR_FILE_TRANSFER);
}

void Pop3Network::filetransfer_host_process_client_ready(const char * peer_address, UWORD peer_port)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    // Wire-driven: a stale/duplicated/crafted READY after the transfer ended
    // would otherwise restart the send loop over a CLEARED FileParts vector
    // (out-of-bounds read transmitted onto the wire).
    if (FT.Status != FileTransferStatus::Host_Waiting_On_Client_Ack &&
        FT.Status != FileTransferStatus::Host_Sending_Window &&
        FT.Status != FileTransferStatus::Host_Waiting_On_Window_Ack)
        return;
    FT.LastContactTime = GetCurrentMs();
    FT.peer_address = peer_address;
    FT.peer_port = peer_port;
    FT.window_start = 0;
    filetransfer_host_send_window();
}

void Pop3Network::filetransfer_host_send_window()
{
    FT.Status = FileTransferStatus::Host_Sending_Window;
    unsigned int window_end = (std::min)(FT.window_start + FT.window_size, FT.total_parts);

    for (unsigned int i = FT.window_start; i < window_end; i++)
    {
        if (i >= FT.FileParts.size())   // belt: never index past the part store
            break;
        Send(-1, FT.peer_address.c_str(), FT.peer_port,
            Pop3NetworkTypes::HOST_SEND_FILE_PART,
            reinterpret_cast<const char*>(&FT.FileParts[i]), sizeof(PopTBFilePartPacket));
        Poco::Thread::sleep(FT_INTER_PACKET_SLEEP_MS);
    }

    PopTBWindowComplete wc;
    wc.window_start = FT.window_start;
    wc.window_end = window_end;
    Send(-1, FT.peer_address.c_str(), FT.peer_port,
        Pop3NetworkTypes::HOST_SEND_FILE_TRANSFER_COMPLETE,
        reinterpret_cast<char*>(&wc), sizeof(wc));

    FT.window_send_time = GetCurrentMs();
    FT.Status = FileTransferStatus::Host_Waiting_On_Window_Ack;
    Pop3Debug::trace("Window sent [%u..%u) size=%u", FT.window_start, window_end, FT.window_size);
}

void Pop3Network::filetransfer_host_send_missing(const unsigned int * bitmap, unsigned int ws, unsigned int wsize)
{
    FT.Status = FileTransferStatus::Host_Sending_Window;
    unsigned int window_end = (std::min)(ws + wsize, FT.total_parts);

    for (unsigned int i = ws; i < window_end; i++)
    {
        unsigned int offset = i - ws;
        bool received = (bitmap[offset / 32] >> (offset % 32)) & 1;
        if (!received)
        {
            if (i >= FT.FileParts.size())   // belt: never index past the part store
                break;
            Send(-1, FT.peer_address.c_str(), FT.peer_port,
                Pop3NetworkTypes::HOST_SEND_FILE_PART,
                reinterpret_cast<const char*>(&FT.FileParts[i]), sizeof(PopTBFilePartPacket));
            Poco::Thread::sleep(FT_INTER_PACKET_SLEEP_MS);
        }
    }

    PopTBWindowComplete wc;
    wc.window_start = ws;
    wc.window_end = window_end;
    Send(-1, FT.peer_address.c_str(), FT.peer_port,
        Pop3NetworkTypes::HOST_SEND_FILE_TRANSFER_COMPLETE,
        reinterpret_cast<char*>(&wc), sizeof(wc));

    FT.window_send_time = GetCurrentMs();
    FT.Status = FileTransferStatus::Host_Waiting_On_Window_Ack;
}

void Pop3Network::filetransfer_client_process_file_part(const char * buffer)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    if (FT.Status == FileTransferStatus::Transfer_Complete)
        return;

    FT.LastContactTime = GetCurrentMs();
    FT.retry_count = 0;   // progress - rearm the give-up counter
    auto fp_ptr = reinterpret_cast<const PopTBFilePartPacket*>(buffer);

    if (!FT.FilePartsRecv.count(fp_ptr->packet_num))
        FT.FilePartsRecv[fp_ptr->packet_num] = *fp_ptr;

    FT.Status = FileTransferStatus::Client_Receiving_Window;
}

void Pop3Network::filetransfer_client_process_window_complete(const char * buffer)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    if (FT.Status == FileTransferStatus::Transfer_Complete)
        return;

    FT.LastContactTime = GetCurrentMs();

    auto wc = reinterpret_cast<const PopTBWindowComplete*>(buffer);
    unsigned int ws = wc->window_start;
    unsigned int we = wc->window_end;
    unsigned int wsize = we - ws;

    size_t total_parts = static_cast<size_t>(ceil(static_cast<double>(FT.FileHeader.file_size) / static_cast<double>(PART_PACKET_DATA_SIZE)));

    // Check if transfer is fully complete
    if (FT.FilePartsRecv.size() == total_parts)
    {
        FT.Status = FileTransferStatus::Transfer_Complete;
        Send(FT.peer_address.c_str(), FT.peer_port, Pop3NetworkTypes::CLIENT_FILE_TRANSFER_COMPLETE);
        compile_fileparts();
        if (GamePtrs.FileTransfer_Callback)
            GamePtrs.FileTransfer_Callback();
        memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
        FT.FileParts.clear();
        FT.peer_address.clear();
        FT.peer_port = 0;
        Pop3Debug::trace("File transfer complete!");
        return;
    }

    // Build bitmap ACK for this window
    PopTBWindowAck ack = {};
    ack.window_start = ws;
    ack.window_size = wsize;

    for (unsigned int i = ws; i < we && i < total_parts; i++)
    {
        unsigned int offset = i - ws;
        if (FT.FilePartsRecv.count(i))
            ack.received_bitmap[offset / 32] |= (1u << (offset % 32));
    }

    unsigned int bitmap_ints = (wsize + 31) / 32;
    DWORD payload_size = static_cast<DWORD>(sizeof(unsigned int) * 2 + sizeof(unsigned int) * bitmap_ints);
    Send(-1, FT.peer_address.c_str(), FT.peer_port,
        Pop3NetworkTypes::CLIENT_RESEND_FILE_PART,
        reinterpret_cast<char*>(&ack), payload_size);

    FT.Status = FileTransferStatus::Client_Receiving_Window;
}

void Pop3Network::filetransfer_host_process_window_ack(const char * buffer)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    // Wire-driven: an ACK that arrives after the transfer ended (UDP
    // duplicate/reorder, or crafted) must not walk the cleared FileParts.
    if (FT.Status != FileTransferStatus::Host_Sending_Window &&
        FT.Status != FileTransferStatus::Host_Waiting_On_Window_Ack &&
        FT.Status != FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer)
        return;
    FT.LastContactTime = GetCurrentMs();

    auto ack = reinterpret_cast<const PopTBWindowAck*>(buffer);
    unsigned int ws = ack->window_start;
    unsigned int wsize = ack->window_size;
    unsigned int window_end = (std::min)(ws + wsize, FT.total_parts);

    // Count missing
    unsigned int missing = 0;
    for (unsigned int i = ws; i < window_end; i++)
    {
        unsigned int offset = i - ws;
        bool received = (ack->received_bitmap[offset / 32] >> (offset % 32)) & 1;
        if (!received) missing++;
    }

    Pop3Debug::trace("Window ACK [%u..%u): %u/%u received", ws, window_end, wsize - missing, wsize);

    if (missing == 0)
    {
        // Window fully received — advance
        FT.retry_count = 0;

        // Grow window on success
        if (FT.window_size < FT_MAX_WINDOW_SIZE)
            FT.window_size = (std::min)((unsigned int)FT_MAX_WINDOW_SIZE, FT.window_size + 8);

        FT.window_start = window_end;

        if (FT.window_start >= FT.total_parts)
        {
            FT.Status = FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer;
            Pop3Debug::trace("All windows sent, waiting for client confirmation");
            return;
        }

        filetransfer_host_send_window();
    }
    else
    {
        FT.retry_count++;

        // Shrink window on loss
        float loss_ratio = static_cast<float>(missing) / wsize;
        if (loss_ratio > 0.5f)
            FT.window_size = (std::max)((unsigned int)FT_MIN_WINDOW_SIZE, FT.window_size / 2);
        else if (loss_ratio > 0.1f)
            FT.window_size = (std::max)((unsigned int)FT_MIN_WINDOW_SIZE, FT.window_size - 4);

        Pop3Debug::trace("Retransmitting %u missing packets (window_size now %u)", missing, FT.window_size);
        filetransfer_host_send_missing(ack->received_bitmap, ws, wsize);
    }
}

void Pop3Network::filetransfer_host_process_client_transfer_successful()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    FT.LastContactTime = 0;
    memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
    FT.FileParts.clear();
    FT.FilePartsRecv.clear();
    FT.peer_address.clear();
    FT.peer_port = 0;
    FT.peer_id = -1;
    FT.retry_count = 0;
    FT.total_parts = 0;
    FT.Status = FileTransferStatus::Transfer_Complete;
    Pop3Debug::trace("Host: client confirmed transfer complete");
}

// Client: the host says there is no (longer an) active transfer for us -
// unstick the UI (progress bar disappears, the lobby Sync button returns).
void Pop3Network::filetransfer_client_process_aborted()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    if (FT.Status == FileTransferStatus::Ready || FT.Status == FileTransferStatus::Transfer_Complete)
        return;
    Pop3Debug::trace("File transfer aborted by host");
    memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
    FT.FileParts.clear();
    FT.FilePartsRecv.clear();
    FT.peer_address.clear();
    FT.peer_port = 0;
    FT.peer_id = -1;
    FT.retry_count = 0;
    FT.total_parts = 0;
    FT.Status = FileTransferStatus::Ready;
}

// A stalled RECEIVER asks "what's the state of my transfer?". Reply to the
// ASKER's endpoint (the old code replied to FT.peer_address, which is empty
// once a transfer completes or aborts - the asker never heard anything and
// stayed stuck "Downloading..." forever). For every active host state,
// resend what the receiver is missing; when there is nothing in flight for
// it, say so explicitly so it can reset.
void Pop3Network::filetransfer_host_requesting_update(const char * peer_address, UWORD peer_port)
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    FT.LastContactTime = GetCurrentMs();
    switch (FT.Status)
    {
    case FileTransferStatus::Host_Waiting_On_Client_Ack:
        // Header may have been lost - resend it.
        Send(-1, peer_address, peer_port,
             Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER,
             reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
        break;
    case FileTransferStatus::Host_Sending_Window:
    case FileTransferStatus::Host_Waiting_On_Window_Ack:
        // The window (or its end marker) was lost - resend the current
        // window. Previously this hit `default: break` and the host ignored
        // a stalled client forever (deadlock until the 20-retry abort).
        filetransfer_host_send_window();
        break;
    case FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer:
        // All windows are out but the client is still asking: it lost part
        // of the final window - resend it.
        filetransfer_host_send_window();
        break;
    case FileTransferStatus::Client_Waiting_On_Host_To_Start:
        Send(peer_address, peer_port, Pop3NetworkTypes::CLIENT_READY_FOR_FILE_TRANSFER);
        break;
    case FileTransferStatus::Transfer_Complete:
    case FileTransferStatus::Ready:
    default:
        // Nothing in flight for the asker - tell it so it can reset its UI
        // instead of re-asking every 3 seconds forever.
        Send(-1, peer_address, peer_port,
             Pop3NetworkTypes::HOST_TRANSFER_ABORTED, nullptr, 0);
        break;
    }
}

void Pop3Network::filetransfer_tick()
{
    Poco::Mutex::ScopedLock lock(ft_mu);
    if (FT.Status == FileTransferStatus::Ready || FT.Status == FileTransferStatus::Transfer_Complete)
        return;

    ULONGLONG now = GetCurrentMs();

    switch (FT.Status)
    {
    case FileTransferStatus::Host_Waiting_On_Window_Ack:
        if ((now - FT.window_send_time) > FT_WINDOW_TIMEOUT_MS)
        {
            FT.retry_count++;
            if (FT.retry_count > FT_MAX_RETRIES)
            {
                filetransfer_abort_locked("window ack: too many retries");
                return;
            }
            FT.window_size = (std::max)((unsigned int)FT_MIN_WINDOW_SIZE, FT.window_size / 2);
            Pop3Debug::trace("Window ACK timeout, resending window (size=%u, retry=%u)", FT.window_size, FT.retry_count);
            filetransfer_host_send_window();
        }
        break;

    case FileTransferStatus::Host_Waiting_On_Client_Ack:
        if ((now - FT.LastContactTime) > 2000)
        {
            // Previously uncapped AND FT.peer_address was empty until the
            // client's READY arrived - a lost first header left this state
            // retrying into nowhere forever, with the transfer slot busy
            // for the rest of the session (every later sync request was
            // silently ignored).
            FT.retry_count++;
            if (FT.retry_count > FT_MAX_RETRIES)
            {
                filetransfer_abort_locked("client never acknowledged the transfer header");
                return;
            }
            FT.LastContactTime = now;
            Send(-1, FT.peer_address.c_str(), FT.peer_port,
                Pop3NetworkTypes::HOST_READY_FOR_FILE_TRANSFER,
                reinterpret_cast<char*>(&FT.FileHeader), sizeof(FT.FileHeader));
        }
        break;

    case FileTransferStatus::Host_Waiting_On_Client_To_Finish_Transfer:
        if ((now - FT.LastContactTime) > FT_WINDOW_TIMEOUT_MS)
        {
            FT.retry_count++;
            if (FT.retry_count > FT_MAX_RETRIES)
            {
                filetransfer_abort_locked("client never confirmed completion");
                return;
            }
            FT.LastContactTime = now;
            Send(-1, FT.peer_address.c_str(), FT.peer_port,
                Pop3NetworkTypes::HOST_REQUESTING_UPDATE_ON_TRANSFER, nullptr, 0);
        }
        break;

    case FileTransferStatus::Client_Receiving_Window:
    case FileTransferStatus::Client_Waiting_On_Host_To_Start:
        if ((now - FT.LastContactTime) > FT_WINDOW_TIMEOUT_MS)
        {
            // Give up eventually (host quit / crashed mid-transfer) so the
            // lobby UI unsticks and the Sync button comes back.
            FT.retry_count++;
            if (FT.retry_count > FT_MAX_RETRIES)
            {
                Pop3Debug::trace("File transfer abandoned: host stopped responding");
                memset(&FT.FileHeader, 0, sizeof(FT.FileHeader));
                FT.FilePartsRecv.clear();
                FT.peer_address.clear();
                FT.peer_port = 0;
                FT.retry_count = 0;
                FT.total_parts = 0;
                FT.Status = FileTransferStatus::Ready;
                return;
            }
            FT.LastContactTime = now;
            Send(FT.peer_address.c_str(), FT.peer_port,
                Pop3NetworkTypes::HOST_REQUESTING_UPDATE_ON_TRANSFER);
        }
        break;

    default:
        break;
    }
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
    Poco::Mutex::ScopedLock lock(players_mu);
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
    while (!(*GamePtrs.GameState == GM_STATE_NORMAL && *GamePtrs.GameMode == GM_FRONTEND) && !m_shutdown.load())
    {
        Poco::Thread::sleep(1); // Intentional 
    }
    if (m_shutdown.load())
        return; // torn down before start; do not stomp identity globals

    player_num = *GamePtrs.RequestedPlayerNum;

    // We successfully joined!
    *GamePtrs.PlayerNum = static_cast<SBYTE>(player_num);
    *GamePtrs.NetMyPlayerNumber = player_num;

    // Let the flood gates open!
    allow_joiners = true;
}

void Pop3Network::join_watchdog_func()
{
    network_status = NetworkStatus::Joining;

    // Wait until you're loaded!
    while (!(*GamePtrs.GameState == GM_STATE_NORMAL && *GamePtrs.GameMode == GM_FRONTEND) && !m_shutdown.load())
    {
        Poco::Thread::sleep(1); // Intentional 
    }

    Poco::Mutex cv_m;
    while (!m_shutdown.load())
    {
        if (!m_join_first_time)
        {
            // Wait in 100ms slices re-checking m_shutdown: a broadcast that
            // lands between the loop condition and tryWait would otherwise be
            // lost and stall process exit for the full CONNECT_INTERVAL.
            bool signalled = false;
            for (int waited = 0; waited < CONNECT_INTERVAL * 1000 && !m_shutdown.load(); waited += 100)
            {
                if (cv2.tryWait(cv_m, 100))
                {
                    signalled = true;
                    break;
                }
            }
            if (m_shutdown.load())
                return; // shutting down - skip the trailing send_join_request
            if ((signalled && server_status == Pop3ErrorStatusCodes::OK) || network_status == NetworkStatus::Connected)
                break;

            if (server_status != Pop3ErrorStatusCodes::OK)
                server_status = Pop3ErrorStatusCodes::OK;

            Pop3Debug::trace("Connecting (%d)...", ++connection_retries);
        }
        else m_join_first_time = false;

        Pop3Debug::trace("Requesting host to join!");

        // Send join packet
        send_join_request();
    }

    if (m_shutdown.load())
        return; // aborted join must never stomp identity globals

    Pop3Debug::trace("Host has approved my connection!");

    // A spectator's real routing id is >= NETWORK_NUMBER_PLAYERS (unlimited
    // spectators). The game's identity globals index fixed size-
    // NETWORK_NUMBER_PLAYERS arrays and gsi.Players[MAX_NUM_PLAYERS], so clamp a
    // spectator to SPECTATOR_SLOT for local bookkeeping; the real routing id
    // stays in Pop3Network::player_num (used for all wire routing).
    const bool i_am_spectator = (*GamePtrs.RequestedPlayerNum == SPECTATOR_SLOT);
    ASSERT((player_num > 0 && player_num < NETWORK_NUMBER_PLAYERS) || i_am_spectator);

    // We successfully joined!
    const SBYTE exported = i_am_spectator ? static_cast<SBYTE>(SPECTATOR_SLOT) : static_cast<SBYTE>(player_num);
    *GamePtrs.PlayerNum = exported;
    *GamePtrs.NetMyPlayerNumber = exported;
}