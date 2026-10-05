#ifndef __TCP_PROXY_H_
#define __TCP_PROXY_H_

#include "main.hpp"
#include "component.hpp"
#include "sock.hpp"
#include "epoll_event.hpp"

class TcpProxy
{
    private:
        Epoll::EventLoop *el;
        BindManager *bm; 

    public:
        TcpProxy(Epoll::EventLoop *el, BindManager *bm);
        ~TcpProxy();

        int TcpClientAccept(Net::TcpSocket *socket, int sock_type);
        int TcpSendToRealServer(Net::Socket *socket);
        int TcpSendToClient(Net::TcpSocket *socket);

        int GetBindPortFromSocket(Net::Socket *socket);
};

#endif 