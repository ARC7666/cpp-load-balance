#include "balancer.hpp"
#include "common.hpp"
#include <iostream>

BalancerProxy::BalancerProxy(Epoll::EventLoop *el, BindManager *bm, Net::Socket *socket)
{
    this->el = el; 
    this->bm = bm; 
    this->socket = socket;
}

BalancerProxy::~BalancerProxy()
{

}

ErrorCode BalancerProxy::Verify(const json &req)
{
    if(req.contains("cmd") == false)
    {
        return ErrorCode::InvalidRequest;
    }

    if(req["cmd"].type() != json::value_t::string)
    {
        return ErrorCode::InvalidRequest;
    }

    if(req["cmd"] == "healthcheck")
    {
        return ErrorCode::None;
    }


    if(req.contains("protocol") == false
    || req.contains("port") == false
    || req.contains("relay_port") == false)
    {
        return ErrorCode::InvalidRequest;
    }

    if(req["protocol"].type() != json::value_t::string
    || req["port"].type() != json::value_t::number_unsigned
    || req["relay_port"].type() != json::value_t::number_unsigned)
    {
        return ErrorCode::InvalidRequest;
    }

    return ErrorCode::None;
}

json BalancerProxy::Controller(const json &req)
{
    json response = {
        {"error", ErrorCode::None},
    }; 

    /* 유효성 검사 */
    ErrorCode result = Verify(req);
    if(result != ErrorCode::None)
    {
        response["error"] = result;
        return response;
    }
    
    std::string cmd = req["cmd"];
    if(cmd == "healthcheck")
    {
        result = HealthCheckComponent(req["state"], req["protocol"], req["port"]);
        response["error"] = result;
        return response;
    }


    std::string protocol = req["protocol"];
    int port = req["port"];
    int relay_port = req["relay_port"];

    if(cmd == "register")
    {
        if(protocol == "tcp" || protocol == "udp")
        {
            response["error"] = RegisterComponent(protocol, port, relay_port);
        }
        else
        {
            response["error"] = ErrorCode::InvalidRegisterProtocol;
        }
    }

    else if(cmd == "unregister")
    {
        response["error"] = UnRegisterComponent(protocol, port, relay_port);
    }
    
    else
    {
        response["error"] = ErrorCode::InvalidCmd;
        return response;
    }

    return response;
}

/**
 * {"cmd": "register", "protocol", "tcp", "port": 80, "relay_port" : 8000}
*/
/**
 * 넘겨받은 port, 넘겨받은 주소 쌍 만들기 == (port, IPEndpoint)
*/
ErrorCode BalancerProxy::RegisterComponent(std::string protocol, int port, int relay_port)
{
    /* protocol과 port 겹치는거 있는지 확인 */
    ErrorCode err;
    Net::Socket *bind_socket; 
    std::tie(err, bind_socket) = bm->AddBind(protocol, port, socket->fd, relay_port);
    if(err != ErrorCode::None)
    {
        return err;
    }

    /* 이미 protocol과 port가 겹친다면 */
    if(bind_socket != nullptr)
    {
        /* bind까지 했지만 epoll 등록이 실패한다면 bind목록 롤백 */
        if(el->AddEvent(bind_socket) == C_ERR)
        {
            bm->DeleteBind(bind_socket->fd);
            delete bind_socket;
            return ErrorCode::BindError;
        }
    }

    return err;
}

/**
 * {"ack": "successful"} or {"ack": "failed", "msg": "…"}
*/
ErrorCode BalancerProxy::UnRegisterComponent(std::string protocol, int port, int relay_port)
{
    ErrorCode err; 
    Net::Socket *bind_socket = nullptr;
    std::tie(err, bind_socket) = bm->DeleteBind(socket->fd);
    if(err != ErrorCode::None)
    {
        return err;
    }

    el->DelEvent(bind_socket);
    return ErrorCode::None;
}

ErrorCode BalancerProxy::HealthCheckComponent(std::string state, std::string protocol, int bind_port)
{
    BindComponent *bc = bm->LoadBindComponent(protocol, bind_port);
    if(bc == nullptr)
    {
        return ErrorCode::None;
    }

    std::vector<Component*> *comps = bc->GetComps();
    for(Component *comp: *comps)
    {
        if(socket->fd == comp->fd)
        {
            if(state == "good")
            {
                comp->state = true;
            }
            else
            {
                comp->state = false;
            }
        }
    }

    return ErrorCode::None;
}