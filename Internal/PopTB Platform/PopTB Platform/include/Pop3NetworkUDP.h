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
    std::thread*                        _thread;
    mutable Pop3UDPSocket               dgs;
    UBYTE                               _mode;

    void RunServer();
};