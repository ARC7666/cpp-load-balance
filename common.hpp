#ifndef __COMMON_H_
#define __COMMON_H_

#include <stdint.h>
#include "./include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace nlohmann::literals;

#define C_OK 0
#define C_ERR 1
#define C_YET 2

typedef uint64_t socket_t;
typedef uint32_t msg_t; 
typedef uint32_t length_t; 
typedef uint8_t byte_t; 

enum SockType
{
    BalancerProxyServer = 0,
    BalancerProxyClient = 1, 
    TcpProxyServer = 2,
    TcpProxyClient = 3,
    UdpProxyServer = 4, 
    UdpProxyClient = 5,
    TcpRelayClient = 6,
};

enum ServerMsg
{
    RegisterRes = 0,
    UnRegisterRes = 1,
    HealthCheckReq = 2,
};

enum ClientMsg
{
    RegisterReq = 0,
    UnRegisterReq = 1,
    HealthCheckRes = 2,
};

enum ErrorCode
{
    None = 0,
    InvalidRequest = 1,
    InvalidCmd = 2,
    AlreadyRegistered = 3,
    AlreadyUnRegistered = 4,
    AlreadyUsedBindPort = 5,
    InvalidRegisterProtocol = 6,
    InternalError = 7,
    BindError = 8,
    CannotFoundFD = 9,
};

#endif