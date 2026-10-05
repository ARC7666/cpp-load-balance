#ifndef __BALANCER_PROXY_H_
#define __BALANCER_PROXY_H_

#include "main.hpp"
#include "component.hpp"
#include "epoll_event.hpp"
#include "sock.hpp"



/* 연결된 컴포넌트는 tcp_proxy나 udp_proxy에서 접근할 수 있어야 함 */

/**
 * 연결된 컴포넌트와의 register, unregister, health check 기능 수행
*/
class BalancerProxy
{
    private:
        BindManager *bm; 
        Epoll::EventLoop *el; 
        Net::Socket *socket; 

    public:
        BalancerProxy(Epoll::EventLoop *el, BindManager *bm, Net::Socket *socket);
        ~BalancerProxy();
        
        ErrorCode RegisterComponent(std::string protocol, int port, int relay_port);
        ErrorCode UnRegisterComponent(std::string protocol, int port, int relay_port);
        json Controller(const json &req);
        ErrorCode HealthCheckComponent(std::string state, std::string protocol, int bind_port);
        ErrorCode Verify(const json &req);
};



#endif