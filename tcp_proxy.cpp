#include "tcp_proxy.hpp"
#include <iostream>
#include <errno.h>

TcpProxy::TcpProxy(Epoll::EventLoop *el, BindManager *bm)
{
    this->el = el;
    this->bm = bm;
}

/**
 * 컴포넌트가 바인드한 포트로 접근을 요청한 클라이언트의 소켓을 만든다. 
*/
int TcpProxy::TcpClientAccept(Net::TcpSocket *socket, int sock_type)
{
    Net::TcpSocket *c_socket = socket->AcceptSocket(sock_type);
    if(c_socket == nullptr)
    {
        return C_ERR;
    }

    if(el->AddEvent(c_socket) == C_ERR)
    {
        delete c_socket;
        return C_ERR;
    }

    c_socket->connection_port = GetBindPortFromSocket(socket);

    return C_OK;
}

/**
 * 클라이언트의 데이터를 읽고 새로 소켓을 만들고 실제 TCP서버로 접속을 하고 데이터를 전송한다. 
*/
int TcpProxy::TcpSendToRealServer(Net::Socket *socket)
{
    BindComponent *bc = bm->LoadBindComponent("tcp", socket->connection_port);
    if(bc == nullptr)
    {
        return C_ERR;
    }
    Component *comp = bc->GetRoundRobinComponent();
    if(comp == nullptr)
    {
        return C_ERR;
    }

    Net::SockAddr *addr = new Net::SockAddr(comp->addr); /* 서버의 주소 정보 */
    Net::TcpSocket *relay_socket = new Net::TcpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    if(relay_socket->CreateSocket(SockType::TcpRelayClient, SOCK_STREAM) == C_ERR)
    {
        delete relay_socket;
        return C_ERR;
    }
    
    if(relay_socket->ConnectSocket() == C_ERR)
    {
        delete relay_socket;
        return C_ERR;
    }

    int ret = socket->ReadSocket();
    if(ret == C_ERR)
    {
        delete relay_socket;
        return C_ERR;
    }

    if(ret == C_YET)
    {
        delete relay_socket;
        return C_YET;
    }

    ret = relay_socket->SendSocket(socket->querybuf, socket->querylen);
    if(ret == C_ERR)
    {
        delete []socket->querybuf;
        delete relay_socket;
        return C_ERR;
    }

    if(el->AddEvent(relay_socket) == C_ERR)
    {   
        delete []socket->querybuf;
        delete relay_socket;
        return C_ERR;
    }

    relay_socket->connection_pair_fd = socket->fd;
    delete []socket->querybuf;
    return C_OK;
}

/**
 * 실제 서버로 전송된 데이터가 도착한다면 처음 접속을 시도한 클라이언트 소켓에게 데이털르 전달한다. 
*/
int TcpProxy::TcpSendToClient(Net::TcpSocket *socket)
{
    int client_fd = socket->connection_pair_fd;
    Net::Socket *client_socket = el->LoadSocket(client_fd);

    if(client_socket == nullptr)
    {
        return C_ERR;
    }

    int ret = socket->ReadSocket();
    if(ret == C_ERR)
    {
        return C_ERR;
    }

    if(ret == C_YET)
    {
        return C_YET;
    }

    ret = client_socket->SendSocket(socket->querybuf, socket->querylen);
    if(ret == C_ERR)
    {
        delete []socket->querybuf;
        return C_ERR;
    }

    delete []socket->querybuf;
    return C_OK;
}

int TcpProxy::GetBindPortFromSocket(Net::Socket *socket)
{
    return ntohs(socket->sock_addr->adr.sin_port);
}

TcpProxy::~TcpProxy()
{

}
