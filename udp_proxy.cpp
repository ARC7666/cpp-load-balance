#include "udp_proxy.hpp"
#include <iostream>

UdpProxy::UdpProxy(Epoll::EventLoop *el, BindManager *bm)
{
    this->el = el;
    this->bm = bm;
}

/**
 * 컴포넌트가 바인딩한 포트로 클라이언트가 접근한다면 클라이언트의 데이터를 읽는다.
 * 그리고 새로 소켓을 만들어 실제 서버로 전송한다. 
 * 
*/
int UdpProxy::UdpSendToRealServer(Net::UdpSocket *comp_socket)
{
    struct sockaddr_in saddr; 

    int comp_bind_port = GetBindPortFromSocket(comp_socket);
    BindComponent *bc = bm->LoadBindComponent("udp", comp_bind_port);
    if(bc == nullptr)
    {
        return C_ERR;
    }

    Component *comp = bc->GetRoundRobinComponent();
    if(comp == nullptr)
    {
        return C_ERR;
    }

    Net::SockAddr *addr = new Net::SockAddr(comp->addr);
    Net::UdpSocket *relay_socket = new Net::UdpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    if(relay_socket->CreateSocket(SockType::UdpProxyClient, SOCK_DGRAM) == C_ERR)
    {
        delete relay_socket;
        return C_ERR; 
    }

    int ret = comp_socket->ReadUdpSocket(&saddr);
    if(ret == C_ERR)
    {
        std::cout<<"[Log] Failed Read UDP socket:"<<comp_socket->fd<<" from client"<<std::endl;
        delete relay_socket;
        return C_ERR;
    }

    else if(ret == C_YET)
    {
        std::cout<<"[Log] EAGAIN Read UDP socket:"<<comp_socket->fd<<" from client"<<std::endl;
        delete relay_socket;
        return C_YET;
    }

    ret = relay_socket->SendSocket(comp_socket->querybuf, comp_socket->querylen);
    if(ret == C_ERR)
    {
        std::cout<<"[Log] Failed Send UDP socket:"<<relay_socket->fd<<" from client"<<std::endl;
        delete []comp_socket->querybuf;
        delete relay_socket;
        return C_ERR;
    }

    if(el->AddEvent(relay_socket) == C_ERR)
    {
        delete []comp_socket->querybuf;
        delete relay_socket;
        return C_ERR;
    }

    relay_socket->client_saddr = saddr; 
    delete []comp_socket->querybuf;
    return C_OK;
}

/**
 * 실제 서버로부터 데이터가 도착한다면 처음 접속을 시도한 클라이언트에게 전달한다. 
*/
int UdpProxy::UdpSendToClient(Net::UdpSocket *relay_socket)
{
    struct sockaddr_in saddr; 
    int ret = relay_socket->ReadUdpSocket(&saddr);
    if(ret == C_ERR)
    {
        return C_ERR;
    }

    else if(ret == C_YET)
    {
        return C_YET;
    }

    ret = relay_socket->SendUdpSocket(relay_socket->client_saddr, relay_socket->querybuf, relay_socket->querylen);
    if(ret == C_ERR)
    {
        delete []relay_socket->querybuf;
        return C_ERR;
    }

    delete []relay_socket->querybuf;
    return C_OK;
}

int UdpProxy::GetBindPortFromSocket(Net::Socket *socket)
{
    return ntohs(socket->sock_addr->adr.sin_port);
}

UdpProxy::~UdpProxy()
{

}
