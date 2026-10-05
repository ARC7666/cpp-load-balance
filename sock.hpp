#ifndef __SOCK_H_
#define __SOCK_H_

#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip.h> 

#include "common.hpp"

#define TCP_PROTOCOL 1
#define UDP_PROTOCOL 2

#define SOCKET_BUFFER 4096

#define socket_nread(s, n) ioctl(s, FIONREAD, n)

namespace Net
{
    class SockAddr
    {
        public:
            struct sockaddr_in adr;
            SockAddr(int port); 
            SockAddr(struct sockaddr_in a);
            ~SockAddr();
    };

    class Socket
    {
        public:
            int sock_type;
            int mask;
            socket_t fd;
            SockAddr *sock_addr;
            byte_t *querybuf;   /* 데이터가 담긴 버퍼 */
            length_t querylen = 0;
            int connection_port = 0; /* TcpProxyClient의 경우 사용 어떠한 바인드된 포트로 연결되었는지 확인 */

            Socket();
            virtual ~Socket();
            virtual int ReadSocket() = 0;
            virtual int SendSocket(byte_t *buffer, int len) = 0;
            int ReadMsgPackFromSocket();
            int SendMsgPackToSocket(const json &msg);
            int CreateSocket(int _sock_type, int _protocol);
            int BindSocket();
    };

    class TcpSocket : public Socket
    {
        public:
            socket_t connection_pair_fd; 

            TcpSocket(SockAddr *adr, int _mask, socket_t fd);
            TcpSocket(SockAddr *adr, int _mask);

            virtual int ReadSocket();
            virtual int SendSocket(byte_t *buffer, int len);
            virtual ~TcpSocket();

            int ListenSocket();
            int ConnectSocket();
            TcpSocket *AcceptSocket(int _sock_type);
    };

    class UdpSocket : public Socket
    {
        public:
            struct sockaddr_in client_saddr; 

            UdpSocket(SockAddr *adr, int _mask);
            UdpSocket(SockAddr *adr, int _mask, socket_t fd);

            int ReadUdpSocket(struct sockaddr_in *saddr);
            int SendUdpSocket(struct sockaddr_in daddr, byte_t *buffer, int len);
            virtual int ReadSocket();
            virtual int SendSocket(byte_t *buffer, int len);
            virtual ~UdpSocket();
    };
}

#endif 