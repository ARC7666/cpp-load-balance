/**
 * Component Management Definitions
 * Defines structures for maintaining active backend servers, tracking their health,
 * and organizing them by bound relay ports for load balancing.
 */
#ifndef __COMPONENT_H_
#define __COMPONENT_H_

#include <vector>
#include <string>
#include <iostream>

#include "sock.hpp"
#include "common.hpp"

#include "epoll_event.hpp"

/**
 * NOTE! 
*/

class Component
{
    public:
        socket_t fd; 
        struct sockaddr_in addr;
        bool state;

        Component(socket_t fd, int relay_port);
        ~Component();
};

class BindComponent
{
    private:
        std::vector<Component*> comps;   
        int port;
        std::string protocol;
        Net::Socket *bind_socket = nullptr; 
        int index = 0;

    public:
        BindComponent();
        ~BindComponent();

        Net::TcpSocket *BindTcpSocket(int port, socket_t fd, int relay_port);
        Net::UdpSocket *BindUdpSocket(int port, socket_t fd, int relay_port);
        bool HasComponent(socket_t fd);
        bool Compare(std::string protocol, int port);
        void AppendComponent(socket_t fd, int relay_port);
        void DeleteComponent(socket_t fd);
        int LenFDS();
        Net::Socket *GetSocket();
        std::vector<Component*> *GetComps();
        Component *GetRoundRobinComponent();
};

class BindManager
{
    private:
        std::vector<BindComponent*> binds;

    public:
        BindManager();
        ~BindManager();

        std::tuple<ErrorCode, Net::Socket*> AddBind(std::string protocol, int port, socket_t fd, int relay_port);
        std::tuple<ErrorCode, Net::Socket*> DeleteBind(socket_t fd); 
        BindComponent *LoadBindComponent(std::string protocol, int port);
        std::vector<BindComponent*> *GetBinds();
};

#endif 