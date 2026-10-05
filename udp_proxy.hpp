#ifndef __UDP_PROXY_H_
#define __UDP_PROXY_H_

#include "main.hpp"
#include "component.hpp"
#include "sock.hpp"
#include "epoll_event.hpp"

class UdpProxy
{
    private:
        Epoll::EventLoop *el;
        BindManager *bm;

    public:
        UdpProxy(Epoll::EventLoop *el, BindManager *bm);
        ~UdpProxy();

        int UdpSendToRealServer(Net::UdpSocket *comp_socket);
        int UdpSendToClient(Net::UdpSocket *relay_socket);
        int GetBindPortFromSocket(Net::Socket *socket);
};

#endif