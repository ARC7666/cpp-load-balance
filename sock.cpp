#include "sock.hpp"
#include "epoll_event.hpp"

#include <iostream>

#define LISTEN_BACKLOG 511

Net::SockAddr::SockAddr(int port)
{
    memset(&adr, 0, sizeof(adr));
    adr.sin_family = AF_INET;
    adr.sin_addr.s_addr = htonl(INADDR_ANY);
    adr.sin_port = htons(port);
}

Net::SockAddr::SockAddr(struct sockaddr_in a)
{
    adr = a;
}

Net::SockAddr::~SockAddr()
{

}

Net::Socket::Socket()
{
    fd = -1;
    sock_addr = nullptr;
    querybuf = nullptr; 
}

int Net::Socket::SendMsgPackToSocket(const json &msg)
{
    std::vector<byte_t> v = json::to_msgpack(msg);
    byte_t *msg_byte = reinterpret_cast<byte_t*>(v.data());
    length_t length = v.size();

    byte_t *buffer = new byte_t[sizeof(length_t) + length]();
    int offset = 0;

    memcpy(buffer+offset, &length, sizeof(length_t));
    offset += sizeof(length_t);
    memcpy(buffer+offset, msg_byte, length);
    offset += length;

    int ret = write(fd, buffer, offset); 
    if(ret < 0)
    {
        return C_ERR;
    }

    delete []buffer;
    return C_OK;
}

int Net::Socket::ReadMsgPackFromSocket()
{
    length_t length; 
    int ret = read(fd, &length, sizeof(length_t));
    if(ret < 0)
    {
        if(errno == EAGAIN)
        {
            return C_YET;
        }
        return C_ERR;
    }

    querybuf = new byte_t[length]; 
    ret = read(fd, querybuf, length);
    querylen = length;

    if(ret < 0)
    {
        return C_ERR; 
    }
    return C_OK;
}

int Net::Socket::CreateSocket(int _sock_type, int _protocol)
{
    int flags;
    
    this->fd = socket(PF_INET, _protocol, 0);
    if(this->fd == -1)
    {
        return C_ERR;
    }

    int on = 1;
    if(setsockopt(this->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        return C_ERR;
    }

    if((flags = fcntl(this->fd, F_GETFL, 0)) == -1)
    {
        return C_ERR;
    }
    
    if(fcntl(this->fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return C_ERR;
    }

    sock_type = _sock_type;    

    return C_OK;
}

int Net::Socket::BindSocket()
{
    if(bind(this->fd, (struct sockaddr*)&sock_addr->adr, sizeof(sock_addr->adr)) == -1)
    {
        return C_ERR;
    }

    return C_OK;
}

Net::Socket::~Socket()
{
    if(sock_addr != nullptr)
    {
        delete sock_addr;
    }
}

Net::TcpSocket::TcpSocket(SockAddr *adr, int _mask)
{
    this->fd = -1;
    sock_addr = adr; 
    this->mask = _mask;
}

Net::TcpSocket::TcpSocket(SockAddr *adr, int _mask, socket_t fd)
{
    this->fd = fd;
    sock_addr = adr;
    this->mask = _mask;
}

/* socket_nread를 사용하여 얼마큼 읽을 수 있는지 확인 */
int Net::TcpSocket::ReadSocket()
{
    if(socket_nread(fd, &querylen) == -1)
    {
        return C_ERR;
    }
    
    querybuf = new byte_t[querylen+1];
    int ret = read(fd, querybuf, querylen);
    if(ret < 0)
    {
        if(errno == EAGAIN)
        {
            return C_YET;
        }

        return C_ERR;
    }

    return C_OK;
}

int Net::TcpSocket::SendSocket(byte_t *buffer, int len)
{
    while(true)
    {
        int ret = write(fd, buffer, len);
        if(ret < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }
            return C_ERR;
        }

        break;
    }
    return C_OK;
}


int Net::TcpSocket::ListenSocket()
{
    if(listen(this->fd, LISTEN_BACKLOG) == -1)
    {
        return C_ERR;
    }

    return C_OK;
}

int Net::TcpSocket::ConnectSocket()
{
    socklen_t addr_len = sizeof(sock_addr->adr);
    int ret = connect(fd, (struct sockaddr*)&sock_addr->adr, addr_len);
    if(ret < 0)
    {
        if(errno == 115)
        {
            /* non block socket이여서 뜸 반복문으로 정상적으로 연결되었는지 확인필요 */
            int error; 
            socklen_t len = sizeof(error);
            if( getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ) 
            {
                return C_ERR;
            }

            /* Connection Failed */
            if(error != 0)
            {
                return C_ERR;
            }

            return C_OK;
        }
        return C_ERR;
    }

    return C_OK;
}

Net::TcpSocket *Net::TcpSocket::AcceptSocket(int _sock_type)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    socket_t fd = accept4(this->fd, (sockaddr*)&addr, &addr_len, SOCK_NONBLOCK);

    if(fd <= 0)
    {
        return nullptr;
    }

    Net::SockAddr *sock = new Net::SockAddr(addr);
    Net::TcpSocket *socket = new Net::TcpSocket(sock, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR, fd);
    socket->sock_type = _sock_type;

    return socket;
}

Net::TcpSocket::~TcpSocket()
{
    if(this->fd != -1)
    {
        close(this->fd);
    }
}

Net::UdpSocket::UdpSocket(SockAddr *adr, int _mask)
{
    sock_addr = adr;
    mask = _mask;
}

Net::UdpSocket::UdpSocket(SockAddr *adr, int _mask, socket_t fd)
{
    sock_addr = adr;
    mask = _mask;
    this->fd = fd;
}

/**
 * 데이터를 SOCKET_BUFFER만큼 읽음 
*/
int Net::UdpSocket::ReadSocket()
{
    return C_OK;
}

int Net::UdpSocket::ReadUdpSocket(struct sockaddr_in *saddr)
{
    if(socket_nread(fd, &querylen) == -1)
    {
        return C_ERR;
    }

    querybuf = new byte_t[querylen];
    int flags = 0;
    socklen_t from_size = sizeof(*saddr);
    int ret = recvfrom(fd, querybuf, querylen, flags, (struct sockaddr*)saddr, &from_size);
    if(ret < 0)
    {
        if(errno == EAGAIN)
        {
            return C_YET;
        }

        return C_ERR;
    }
    return C_OK;
}

int Net::UdpSocket::SendUdpSocket(struct sockaddr_in daddr, byte_t *buffer, int len)
{
    int flags = 0;
    socklen_t adr_len = sizeof(daddr);
    while(true)
    {
        int ret = sendto(fd, buffer, len, flags, (struct sockaddr*)&daddr, adr_len);
        if(ret < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }
            return C_ERR;
        }

        break;
    }

    return C_OK;
}

int Net::UdpSocket::SendSocket(byte_t *buffer, int len)
{
    int flags = 0;
    socklen_t adr_len = sizeof(sock_addr->adr); 
    while(true)
    {
        int ret = sendto(fd, buffer, len, flags, (struct sockaddr*)&sock_addr->adr, adr_len);
        if(ret < 0)
        {
            if(errno == EAGAIN)
            {
                continue;
            }
            return C_ERR;
        }

        break;
    }
    return C_OK;
}

Net::UdpSocket::~UdpSocket()
{
    if(this->fd != -1)
    {
        close(this->fd);
    }
}