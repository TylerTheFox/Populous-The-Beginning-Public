#include "Pop3NetworkUDP.h"
#include "Pop3Debug.h"
#include "Pop3App.h"
#include "Pop3MajorFault.h"

#include <Poco/Net/NetException.h>
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

		_set_se_translator(Pop3App::_Pop3_SEH);
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

        if (_mode == SM_HOSTING)
            dgs.bind(Poco::Net::SocketAddress(local_port));
        else dgs.connect(Poco::Net::SocketAddress(ipaddr, local_port));

        // Fuck Winsock, ignore all those fucking connection reset errors.
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(dgs.getSocket(), SIO_UDP_CONNRESET, &bNewBehavior, sizeof bNewBehavior, NULL, 0, &dwBytesReturned, NULL, NULL);

        while (!(*GamePtrs.GnsiFlags & GNS_QUITTING) && !Pop3App::isQuitting())
        {
            try
            {
                Poco::Net::SocketAddress sender;
                //clear the buffer by filling null, it might have previously received data
                memset(buf, 0, MAX_PACKET_SIZE);

                // Blocking
                int recv_len = dgs.receiveFrom(buf, sizeof(buf), sender);
                if ((*GamePtrs.GnsiFlags & GNS_QUITTING) && Pop3App::isQuitting())
                    return;

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
            catch (const Poco::Exception& e)
            {
                if (e.code() == WSANOTINITIALISED)
                    return;
                // Do nothing
            }
        }
    }
    catch (const Poco::Exception &)
    {
        // Do nothing
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

        static char buf[MAX_PACKET_SIZE];
        buf[0] = POP_PACKET_TYPE;
        buf[1] = static_cast<BYTE>(type);
        if (buffer && buf_size > 0) memcpy(&buf[2], buffer, buf_size);
        auto ret = dgs.sendTo(buf, buf_size + 2, Poco::Net::SocketAddress(peer_address, peer_port));

        if (ret == SOCKET_ERROR)
        {
            DebugBreak();
        }

        add_packet_info(pi);
    }
    catch (const Poco::Exception & e)
    {
        Pop3Debug::trace("Network Exception: %s", e.what());
    }
}
