#pragma once
#include "Pop3Network.h"
#include <Poco/Net/DatagramSocket.h>
#include <thread>

class Pop3UDPSocket : public Poco::Net::DatagramSocket
{
public:
    poco_socket_t getSocket()
    {
        return sockfd();
    }
};

class Pop3NetworkUDP : public Pop3Network
{
public:
    virtual ~Pop3NetworkUDP();

    void ServerInit(UBYTE mode);
    void Send(int to_pn, const char* peer_address, UWORD peer_port, Pop3NetworkTypes type, const char* buffer, DWORD buf_size) const;
private:
    std::thread*                        _thread = nullptr;
    mutable Pop3UDPSocket               dgs;
    UBYTE                               _mode;

    void RunServer();
    // Mode A lobby-server keepalive (3c): LOBBY_HOST_REGISTER from the game
    // socket every ~10s while armed and hosting. Network thread only.
    void lobby_registration_tick();
    void claim_tick();   // host migration: resend LOBBY_CLAIM_HOST while claiming
    void lobby_close_tick();   // host left a hosted game: emit LOBBY_CLOSE + disarm keepalives
    // LOBBY_HOST_PUNCH handler: open our NAT toward a per-joiner proxy port.
    void handle_lobby_frame(const char* buf, DWORD len, const Poco::Net::SocketAddress& sender);
};