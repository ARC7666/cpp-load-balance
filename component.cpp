#include "component.hpp"
#include <errno.h>

#include <iostream>

Component::Component(socket_t fd, int relay_port)
{
    socklen_t len = sizeof(this->addr);

    getpeername(fd, (struct sockaddr*)&this->addr, &len); 
    this->addr.sin_port = htons(relay_port);
    this->fd = fd;
    this->state = true;
}

Component::~Component()
{

}

BindComponent::BindComponent()
{

}

BindComponent::~BindComponent()
{

}

void BindComponent::AppendComponent(socket_t fd, int relay_port)
{
    Component *component = new Component(fd, relay_port);
    comps.push_back(component);
    return;
}

bool BindComponent::Compare(std::string protocol, int port)
{
    if(this->port == port && this->protocol == protocol)
    {
        return true;
    }

    return false;
}

bool BindComponent::HasComponent(socket_t fd)
{
    for(Component *c: comps)
    {
        if(c->fd == fd)
        {
            return true;
        }
    }
    return false;
}


Net::TcpSocket *BindComponent::BindTcpSocket(int port, socket_t fd, int relay_port)
{
    Net::SockAddr *addr = new Net::SockAddr(port);
    Net::TcpSocket *socket = new Net::TcpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    this->port = port; 
    this->protocol = "tcp";

    if(socket->CreateSocket(SockType::TcpProxyServer, SOCK_STREAM) == C_ERR)
    {
        delete socket;
        return nullptr;
    } 

    if(socket->BindSocket() == C_ERR)
    {
        delete socket;
        return nullptr;
    }

    if(socket->ListenSocket() == C_ERR)
    {
        delete socket;
        return nullptr;
    }

    AppendComponent(fd, relay_port);
    bind_socket = socket;

    return (Net::TcpSocket*)bind_socket;
}

Net::UdpSocket *BindComponent::BindUdpSocket(int port, socket_t fd, int relay_port)
{
    Net::SockAddr *addr = new Net::SockAddr(port);
    Net::UdpSocket *socket = new Net::UdpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    this->port = port; 
    this->protocol = "udp";

    if(socket->CreateSocket(SockType::UdpProxyServer, SOCK_DGRAM) == C_ERR)
    {
        delete socket;
        return nullptr;
    } 

    if(socket->BindSocket() == C_ERR)
    {
        delete socket;
        return nullptr;
    }

    AppendComponent(fd, relay_port);
    bind_socket = socket;

    return (Net::UdpSocket*)bind_socket;
}

void BindComponent::DeleteComponent(socket_t fd)
{
    auto it = std::find_if(comps.begin(), comps.end(), 
                        [fd](Component *c){
                            return c->fd == fd;
                        });

    if(it != comps.end())
    {
        Component *rc = *it;
        comps.erase(it);
        delete rc;
    }
}

int BindComponent::LenFDS()
{
    return comps.size();
}

Net::Socket *BindComponent::GetSocket()
{
    return bind_socket;
}

std::vector<Component*> *BindComponent::GetComps()
{
    return &comps;
}

Component* BindComponent::GetRoundRobinComponent()
{
    if(index >= comps.size())
    {
        index = 0;
    }

    Component *rc = comps[index];
    while(rc->state == false)
    {
        index += 1; 
        rc = comps[index];
    }
    index += 1;
    return rc;
}

BindManager::BindManager()
{

}

BindManager::~BindManager()
{
    /* 모든 bind 요소 delete */
    for(BindComponent *bc: binds)
    {
        // need to unbind 
        delete bc; 
    }

    binds.clear();
}

std::tuple<ErrorCode, Net::Socket*> BindManager::AddBind(std::string protocol, int port, socket_t fd, int relay_port)
{
    // 이미 동일한 fd로 바인드되고 있는지 확인
    for(BindComponent *bc: binds)
    {
        if(bc->HasComponent(fd) == true)
        {
            return std::make_tuple(ErrorCode::AlreadyRegistered, nullptr);
        }
    }

    BindComponent *bc = LoadBindComponent(protocol, port);
    // 이미 해당 포트와 프로토콜로 바인딩 되고 있다면 
    if(bc != nullptr)
    {
        bc->AppendComponent(fd, relay_port);
        return std::make_tuple(ErrorCode::None, nullptr);
    }

    BindComponent *new_bc = new BindComponent();
    Net::Socket *bind_socket = nullptr;

    if(protocol == "tcp")
    {
        bind_socket = new_bc->BindTcpSocket(port, fd, relay_port);
    }

    else if(protocol == "udp")
    {
        bind_socket = new_bc->BindUdpSocket(port, fd, relay_port);
    }

    /* socket bind실패시 바인드 목록 삭제 */
    if(bind_socket == nullptr)
    {
        delete new_bc;
        return std::make_tuple(ErrorCode::BindError, nullptr);
    }

    /* 바인드 목록에 추가 */
    binds.push_back(new_bc);

    return std::make_tuple(ErrorCode::None, bind_socket);
}

/* 인자로 주어진 fd를 지우고 bindcomponent의 fds의 길이가 0이라면 바인딩 제거 */
std::tuple<ErrorCode, Net::Socket *> BindManager::DeleteBind(socket_t fd)
{
    /* fds의 사이즈가 0이 된다면 해당 바인드 삭제 */
    for(BindComponent *bc: binds)
    {
        if(bc->HasComponent(fd) == true)
        {
            bc->DeleteComponent(fd);
            if(bc->LenFDS() == 0)
            {
                auto it = std::find(binds.begin(), binds.end(), bc); 
                if(it != binds.end())
                {
                    binds.erase(it);
                    Net::Socket *rs = bc->GetSocket();
                    delete bc;
                    return std::make_tuple(ErrorCode::None, rs);
                }
            }

            return std::make_tuple(ErrorCode::None, nullptr);
        }
    }

    return std::make_tuple(ErrorCode::AlreadyUnRegistered, nullptr);
}

BindComponent *BindManager::LoadBindComponent(std::string protocol, int port)
{
    for(BindComponent *bc: binds)
    {
        if(bc->Compare(protocol, port) == true)
        {
            return bc; 
        }
    }

    return nullptr;
}

std::vector<BindComponent*> *BindManager::GetBinds()
{
    return &binds;
}