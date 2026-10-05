#ifndef __PROXY_H_
#define __PROXY_H_

#include "sock.hpp"
#include "component.hpp"
#include "epoll_event.hpp"
#include "message.hpp"
#include "tcp_proxy.hpp"
#include "udp_proxy.hpp"

#include <thread>
#include <tuple>

typedef std::chrono::high_resolution_clock time_clock; 

class Proxy
{
    private:
        BindManager *bm = nullptr;
        TcpProxy *tcp_proxy = nullptr;
        UdpProxy *udp_proxy = nullptr;
        Epoll::EventLoop el;
        int tick = 0;

    public:
        Proxy();
        ~Proxy();

        void Run(int port);
        void ProcessEvent(int retval);
        int BindTcpSocket(int port, int sock_type);
        int BindUdpSocket(int port, int sock_type);
        int DeleteSocket(Net::Socket *socket);
        Message *ParseMessage(Net::Socket *socket);

        std::tuple<int, json> ProcessControlChannel(Net::Socket *socket);
        void SendHealthCheck();
};

#endif 