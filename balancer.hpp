/**
 * Balancer Definitions
 * Defines the control interface for backend node dynamic registration.
 */
#ifndef __BALANCER_PROXY_H_
#define __BALANCER_PROXY_H_

#include "main.hpp"
#include "component.hpp"
#include "epoll_event.hpp"
#include "sock.hpp"




/**
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