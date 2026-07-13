#include "Pop3Platform_Win32.h"
#include "Pop3NetworkUDP.h"
#include "Pop3Debug.h"
#include "Pop3App.h"
#include "Pop3MajorFault.h"

#include <Poco/Net/NetException.h>
#include <Poco/Exception.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Format.h>
#include <Poco/Thread.h>
#include <thread>

#include <Winsock2.h>
#include <Mstcpip.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)

extern ULONGLONG    getGameClockMiliseconds();

Pop3NetworkUDP::~Pop3NetworkUDP()
{
    // Stop the watchdogs from the most-derived dtor (they call virtual Send);
    // then stop and join the receive thread.
    shutdown_watchdogs();
    if (_thread)
    {
        if (_thread->joinable())
            _thread->join();
        delete _thread;
        _thread = nullptr;
    }

    // Key-gated LOBBY_CLOSE so the lobby server reaps this lobby immediately
    // instead of waiting out the idle timeout. After the thread join, so the
    // socket is used from exactly one thread here.
    if (m_lobbyreg_armed)
    {
        m_lobbyreg_armed = false;
        unsigned char frame[2 + sizeof(Pop3LobbyCloseV1)];
        frame[0] = POP3LOBBY_PACKET_TYPE;
        frame[1] = LOBBY_CLOSE;
        Pop3LobbyCloseV1 close_msg{};
        close_msg.ProtocolVersion = POP3LOBBY_PROTOCOL_VERSION;
        memcpy(close_msg.HostKey, m_lobbyreg_key, POP3LOBBY_HOST_KEY_LEN);
        memcpy(frame + 2, &close_msg, sizeof(close_msg));
        try
        {
            dgs.sendTo(frame, sizeof(frame), Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port));
            Pop3Debug::trace("LOBBY_CLOSE sent to %s:%d", m_lobbyreg_host, (int)m_lobbyreg_port);
        }
        catch (const Poco::Exception&)
        {
        }
    }
}

// LOBBY_HOST_PUNCH from our lobby session: send one datagram from the game
// socket to the named per-joiner proxy port so a port-restricted NAT (incl.
// router hairpin) opens toward that leg. Only obeyed while registration is
// armed and only from the session's own endpoint - nobody else gets to make
// this socket emit datagrams at arbitrary targets.
void Pop3NetworkUDP::handle_lobby_frame(const char* buf, DWORD len, const Poco::Net::SocketAddress& sender)
{
    const unsigned char msg = static_cast<unsigned char>(buf[1]);

    // Host-migration claim RESP is accepted BEFORE the armed gate: a claiming
    // joiner is not a registered host yet. On grant, adopt the fresh host key
    // and assume the host role; the game layer polls HostClaimSucceeded().
    if (m_claiming.load() && msg == LOBBY_CLAIM_HOST_RESP && len >= 2 + sizeof(Pop3LobbyClaimHostRespV1))
    {
        try
        {
            if (sender != Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port))
                return;
        }
        catch (const Poco::Exception&) { return; }
        Pop3LobbyClaimHostRespV1 resp;
        memcpy(&resp, buf + 2, sizeof(resp));
        if (resp.Result == LOBBY_OK)
        {
            memcpy(m_lobbyreg_key, resp.HostKey, POP3LOBBY_HOST_KEY_LEN);
            m_lobbyreg_last_ms = 0;      // resume keepalives immediately
            m_lobbyreg_armed = true;
            am_host = true;
            m_claiming.store(false);
            m_claim_succeeded.store(true);
            Pop3Debug::trace("Host claim GRANTED - assuming host role");
        }
        // else: not granted (host still deemed live) - keep claiming.
        return;
    }

    // Seed-rendezvous lookup RESP from the DIRECTORY port. Accepted BEFORE the
    // armed gate: joiners are never armed but still probe by seed. NOT_FOUND =
    // our relay session is gone (server restart) -> raise the flag the game
    // polls (RelaySessionLost); OK = alive (Step C re-points if the port moved).
    if (msg == LOBBY_RESUME_LOOKUP_RESP && len >= 2 + sizeof(Pop3LobbyResumeLookupRespV1))
    {
        try
        {
            if (m_dir_port == 0 || m_lobbyreg_host[0] == 0
                || sender != Poco::Net::SocketAddress(m_lobbyreg_host, m_dir_port))
                return;
        }
        catch (const Poco::Exception&) { return; }
        Pop3LobbyResumeLookupRespV1 resp;
        memcpy(&resp, buf + 2, sizeof(resp));
        if (resp.Result == LOBBY_ERR_NOT_FOUND)
        {
            // Debounce: one NOT_FOUND can be the ~1 RTT window before our seed
            // keepalive registers at game start. Two consecutive (>= ~4s) means
            // the session is really gone.
            if (++m_resume_notfound >= 2 && !m_relay_session_lost.exchange(true))
                Pop3Debug::trace("Seed rendezvous: relay session GONE (server restarted), seed %u",
                                 (unsigned)m_resume_seed.load());
        }
        else if (resp.Result == LOBBY_OK)
        {
            m_resume_notfound = 0;
            m_relay_session_lost.store(false);   // session confirmed live
        }
        return;
    }

    if (!m_lobbyreg_armed)
        return;
    try
    {
        if (sender != Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port))
            return;

        // The keepalive's RESP doubles as a ping probe: RTT to the relay is
        // the host's real latency in a proxied game (see getLobbyServerPingMs).
        if (msg == LOBBY_HOST_REGISTER_RESP)
        {
            m_lobbyreg_last_resp_ms.store(GetCurrentMs());   // relay-liveness clock
            if (m_lobbyreg_sent_ms)
                m_lobby_ping_ms.store(static_cast<int>(GetCurrentMs() - m_lobbyreg_sent_ms));
            return;
        }

        if (msg != LOBBY_HOST_PUNCH || len < 2 + sizeof(Pop3LobbyHostPunchV1))
            return;
        Pop3LobbyHostPunchV1 punch;
        memcpy(&punch, buf + 2, sizeof(punch));
        if (punch.ProtocolVersion != POP3LOBBY_PROTOCOL_VERSION || punch.JoinerServerPort == 0)
            return;
        const unsigned char ack[2] = { POP3LOBBY_PACKET_TYPE, LOBBY_PUNCH_ACK };
        dgs.sendTo(ack, sizeof(ack), Poco::Net::SocketAddress(m_lobbyreg_host, punch.JoinerServerPort));
        Pop3Debug::trace("Lobby NAT punch -> %s:%d", m_lobbyreg_host, (int)punch.JoinerServerPort);
    }
    catch (const Poco::Exception&)
    {
    }
}

// Periodic LOBBY_HOST_REGISTER keepalive from the GAME socket (3c): the
// LobbySession records this datagram's source as the host endpoint (NAT-safe)
// and refreshes its idle timers. Fire-and-forget - the RESP arrives on this
// socket and is dropped by the frame filter (first byte 11 != POP_PACKET_TYPE),
// which is fine; the next keepalive re-registers regardless.
void Pop3NetworkUDP::lobby_registration_tick()
{
    if (!m_lobbyreg_armed || !am_host)
        return;
    const ULONGLONG now = GetCurrentMs();
    // A seed change (game just started) forces an immediate keepalive so the
    // LobbySession indexes the new seed at once - otherwise early resume probes
    // would false-NOT_FOUND until the next 10s keepalive.
    const bool seed_changed = m_seed_dirty.exchange(false);
    if (!seed_changed && m_lobbyreg_last_ms && now - m_lobbyreg_last_ms < 10000)
        return;
    m_lobbyreg_last_ms = now;
    m_lobbyreg_sent_ms = now;   // RESP arrival - this = relay RTT

    unsigned char frame[2 + sizeof(Pop3LobbyHostRegisterV1)];
    frame[0] = POP3LOBBY_PACKET_TYPE;
    frame[1] = LOBBY_HOST_REGISTER;
    Pop3LobbyHostRegisterV1 reg{};
    reg.ProtocolVersion = POP3LOBBY_PROTOCOL_VERSION;
    reg.PlayerCount = static_cast<uint8_t>(GetGamePlayerCount());   // confirmed roster -> browser count (denied/wrong-password legs excluded)
    memcpy(reg.HostKey, m_lobbyreg_key, POP3LOBBY_HOST_KEY_LEN);
    reg.Seed = m_resume_seed.load();   // index the session by game seed (0 pre-game) for LOBBY_RESUME_LOOKUP
    memcpy(frame + 2, &reg, sizeof(reg));
    try
    {
        dgs.sendTo(frame, sizeof(frame), Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port));
    }
    catch (const Poco::Exception&)
    {
    }
}

// Seed rendezvous probe (2026-07-13): once in-game (m_resume_seed != 0), ask the
// DIRECTORY port every ~2s whether a live session still serves our seed. Run by
// BOTH host and joiner (NOT gated on am_host) so either can detect a relay
// restart instantly (LOBBY_ERR_NOT_FOUND -> RelaySessionLost) and, in Step C,
// re-find the new lobby port. Needs the directory endpoint (m_lobbyreg_host +
// m_dir_port), which the host has from bring-up; joiners set it the same way.
void Pop3NetworkUDP::resume_lookup_tick()
{
    const uint32_t seed = m_resume_seed.load();
    if (seed == 0 || m_dir_port == 0 || m_lobbyreg_host[0] == 0)
        return;
    const ULONGLONG now = GetCurrentMs();
    if (m_resume_last_ms && now - m_resume_last_ms < 2000)
        return;
    m_resume_last_ms = now;

    unsigned char frame[2 + sizeof(Pop3LobbyResumeLookupV1)];
    frame[0] = POP3LOBBY_PACKET_TYPE;
    frame[1] = LOBBY_RESUME_LOOKUP;
    Pop3LobbyResumeLookupV1 req{};
    req.ProtocolVersion = POP3LOBBY_PROTOCOL_VERSION;
    req.Seed = seed;
    memcpy(frame + 2, &req, sizeof(req));
    try
    {
        dgs.sendTo(frame, sizeof(frame), Poco::Net::SocketAddress(m_lobbyreg_host, m_dir_port));
    }
    catch (const Poco::Exception&)
    {
    }
}

// Host migration: while claiming, resend LOBBY_CLAIM_HOST from the game socket
// to the lobby endpoint (~1s). The LobbySession grants once the old host has
// been silent past its threshold, replying LOBBY_CLAIM_HOST_RESP (handled in
// handle_lobby_frame above, before the armed gate).
void Pop3NetworkUDP::claim_tick()
{
    if (!m_claiming.load())
        return;
    const ULONGLONG now = GetCurrentMs();
    if (m_claim_last_ms && now - m_claim_last_ms < 1000)
        return;
    m_claim_last_ms = now;

    unsigned char frame[2 + sizeof(Pop3LobbyClaimHostV1)];
    frame[0] = POP3LOBBY_PACKET_TYPE;
    frame[1] = LOBBY_CLAIM_HOST;
    Pop3LobbyClaimHostV1 c{};
    c.ProtocolVersion = POP3LOBBY_PROTOCOL_VERSION;
    memcpy(frame + 2, &c, sizeof(c));
    try
    {
        dgs.sendTo(frame, sizeof(frame), Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port));
    }
    catch (const Poco::Exception&)
    {
    }
}

// Host left a hosted federated game (RequestLobbyClose): emit ONE key-gated
// LOBBY_CLOSE and disarm the keepalive so the LobbySession reaps immediately
// instead of lingering until the 45s host-gone timer (or forever, while the
// zombie keepalive keeps refreshing m_lastHostMs). Disarming is the load-bearing
// half: even if this datagram is lost, keepalives stop, so the reap finally fires.
void Pop3NetworkUDP::lobby_close_tick()
{
    if (m_lobby_leave_requested.exchange(false) && m_lobbyreg_armed)
        m_lobby_close_left = 3;          // send LOBBY_CLOSE a few times (one UDP loss shouldn't leave a 45s zombie)
    if (m_lobby_close_left <= 0)
        return;
    if (!m_lobbyreg_armed)               // joiner / non-federated host, or already disarmed
    {
        m_lobby_close_left = 0;
        return;
    }
    if (--m_lobby_close_left == 0)
        m_lobbyreg_armed = false;        // stop LOBBY_HOST_REGISTER after the last send -> host-gone reap can still fire
    unsigned char frame[2 + sizeof(Pop3LobbyCloseV1)];
    frame[0] = POP3LOBBY_PACKET_TYPE;
    frame[1] = LOBBY_CLOSE;
    Pop3LobbyCloseV1 close_msg{};
    close_msg.ProtocolVersion = POP3LOBBY_PROTOCOL_VERSION;
    memcpy(close_msg.HostKey, m_lobbyreg_key, POP3LOBBY_HOST_KEY_LEN);
    memcpy(frame + 2, &close_msg, sizeof(close_msg));
    try
    {
        dgs.sendTo(frame, sizeof(frame), Poco::Net::SocketAddress(m_lobbyreg_host, m_lobbyreg_port));
        Pop3Debug::trace("LOBBY_CLOSE sent (host left game) to %s:%d", m_lobbyreg_host, (int)m_lobbyreg_port);
    }
    catch (const Poco::Exception&)
    {
    }
}

void Pop3NetworkUDP::ServerInit(UBYTE mode)
{
    Pop3Debug::trace("Setting up network...");
    _mode = mode;
    _thread = new std::thread(&Pop3NetworkUDP::RunServer, this);
}

void Pop3NetworkUDP::RunServer()
{
    try
    {
        char buf[MAX_PACKET_SIZE];
        const std::string ipaddr(GamePtrs.RemoteIPAddress);
        auto & remote_port = *GamePtrs.RemotePort;
        auto & local_port = *GamePtrs.LocalPort;
        bool first_connection = true;

		_set_se_translator((_se_translator_function)Pop3App::_Pop3_SEH);
		set_terminate(Pop3App::_Pop3_Term);

        if (_mode == SM_JOINING)
        {
            if (local_port)
            {
                local_port = remote_port;
            }
        }

        if (_mode == SM_JOINING)
        {
            am_host = false;
        }
        else
        {
            add_player("", 0, *GamePtrs.RequestedPlayerNum, true, const_cast<UNICODE_CHAR*>(my_name.c_str()));
            am_host = true;
        }

        // Always bind instead of connect — connect() on UDP filters incoming
        // packets to only the connected address, which breaks if the host's
        // outbound port changes due to NAT rebinding.
        if (_mode == SM_HOSTING)
            dgs.bind(Poco::Net::SocketAddress(local_port));
        else
            dgs.bind(Poco::Net::SocketAddress(local_port));

        // Fuck Winsock, ignore all those fucking connection reset errors.
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(dgs.getSocket(), SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);

        // Set receive timeout so the loop can tick file transfers even without incoming packets
        dgs.setReceiveTimeout(Poco::Timespan(0, 100000)); // 100ms

        while (!(*GamePtrs.GnsiFlags & GNS_QUITTING) && !Pop3App::isQuitting() && !m_shutdown.load())
        {
            try
            {
                Poco::Net::SocketAddress sender;
                // Zero between receives: packet parsers (CLIENT_JOIN name,
                // HOST_PLAYERS records) read fixed widths past short payloads
                // and rely on the buffer being null beyond the datagram.
                memset(buf, 0, sizeof(buf));
                // Poll first so an idle lobby socket doesn't throw+catch
                // TimeoutException ~10x/sec (the 100ms receive timeout set
                // above stays as a safety net). Only call receiveFrom when a
                // datagram is actually ready; otherwise just tick transfers.
                if (!dgs.poll(Poco::Timespan(0, 100000), Poco::Net::Socket::SELECT_READ))
                {
                    filetransfer_tick();
                    lobby_registration_tick();
                    claim_tick();
                    lobby_close_tick();
                    resume_lookup_tick();
                    continue;
                }
                int recv_len = dgs.receiveFrom(buf, sizeof(buf), sender);
                if (((*GamePtrs.GnsiFlags & GNS_QUITTING) && Pop3App::isQuitting()) || m_shutdown.load())
                    return;

                if (recv_len >= 2 && static_cast<unsigned char>(buf[0]) == POP3LOBBY_PACKET_TYPE)
                {
                    // Lobby-server control frames (3c): only the NAT punch
                    // request is acted on; register RESPs etc. are
                    // fire-and-forget noise. Never a game frame - keep them
                    // away from SendPointers/ParsePacket.
                    handle_lobby_frame(buf, static_cast<DWORD>(recv_len), sender);
                }
                else
                {
                    if (first_connection)
                    {
                        SendPointers(sender.host().toString().c_str(), sender.port());
                        first_connection = false;
                    }

                    if (recv_len > 1 && buf[0] == POP_PACKET_TYPE)
                    {
                        ParsePacket(buf, recv_len, sender.host().toString().c_str(), sender.port());
                    }
                }
            }
            catch (const Poco::TimeoutException&)
            {
                // No packet received — that's fine, just tick below
            }
            catch (const Poco::Exception& e)
            {
                if (e.code() == WSANOTINITIALISED)
                    return;
            }

            // Tick file transfer timeouts/retransmissions independently of packet arrival
            filetransfer_tick();
            lobby_registration_tick();
            claim_tick();
            lobby_close_tick();
            resume_lookup_tick();
        }
    }
    catch (const Poco::Exception & e)
    {
        // Historically silent, which made a failed bind (port squatted by
        // another process) look like a host that just never answers. The
        // thread still dies - the game can't run without its socket - but
        // now the log says why.
        Pop3Debug::trace("Network receive thread died: %s", e.displayText().c_str());
    }
}

void Pop3NetworkUDP::Send(int to_pn, const char * peer_address, UWORD peer_port, Pop3NetworkTypes type, const char * buffer, DWORD buf_size) const
{
    try
    {
        if (peer_address[0] == 0 || peer_port == 0) return;
        ASSERT(buf_size <= (MAX_PACKET_SIZE - 2));

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
        pi.packet_num = (int)type;
        pi.packetSize = buf_size;
        pi.time = getGameClockMiliseconds();
        pi.sending_packet = true;
        pi.from_id = player_num;
        pi.to_id = to_pn;

        if (type == Pop3NetworkTypes::POP_DATA)
            pi.extra_data = Poco::format("%d", +static_cast<char>(buffer[1]));

        char sendbuf[MAX_PACKET_SIZE];
        sendbuf[0] = POP_PACKET_TYPE;
        sendbuf[1] = static_cast<BYTE>(type);
        if (buffer && buf_size > 0) memcpy(&sendbuf[2], buffer, buf_size);
        auto ret = dgs.sendTo(sendbuf, buf_size + 2, Poco::Net::SocketAddress(peer_address, peer_port));

        if (ret == SOCKET_ERROR)
        {
            Pop3Debug::trace("Send failed: SOCKET_ERROR to %s:%d", peer_address, peer_port);
        }

        add_packet_info(pi);
    }
    catch (const Poco::Exception & e)
    {
        Pop3Debug::trace("Network Exception: %s", e.what());
    }
}
