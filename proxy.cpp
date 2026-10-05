/**
 * Epoll Event Loop & Proxy Engine
 * The core event loop using epoll to multiplex thousands of non-blocking sockets.
 * Dispatches read/write events to the respective TCP/UDP proxy handlers.
 */
#include "proxy.hpp"
#include "balancer.hpp"
#include "tcp_proxy.hpp"
#include "udp_proxy.hpp"

Proxy::Proxy()
{
    if(el.CreateEventLoop() == C_ERR)
    {
        std::cerr<<"Failed CreateEventLoop for Balancer"<<std::endl;
        return;
    }
    try
    {
        bm = new BindManager();
        tcp_proxy = new TcpProxy(&el, bm);
        udp_proxy = new UdpProxy(&el, bm);
    }
    catch(std::exception& e)
    {
        std::cout<<"Failed Run Proxy"<<std::endl;
        return;
    }
    
}

int Proxy::DeleteSocket(Net::Socket *socket)
{
    if(socket->sock_type == SockType::BalancerProxyClient)
    {
        // call bm->DeleteBind();
        ErrorCode err; 
        Net::Socket *bind_socket = nullptr;
        std::tie(err, bind_socket) = bm->DeleteBind(socket->fd);
        if(err != ErrorCode::None)
        {
            std::cout<<"[DEBUG] isn't bind socket"<<std::endl;
        }
        else
        {
            if(bind_socket != nullptr)
            {
                el.DelEvent(bind_socket);
                std::cout<<"[DEBUG] remove bind socket"<<std::endl;
            }
            else
            {
                std::cout<<"[DEBUG] reduce bind reference"<<std::endl;
            }
        }
    }

    el.DelEvent(socket);
    return C_OK;
}

Message *Proxy::ParseMessage(Net::Socket *socket)
{
    if(socket->querybuf == nullptr)
    {
        return nullptr;
    }

    Message *message = new Message(socket->querybuf);
    if(message->Parse(socket->querylen) == C_ERR)       
    {
        delete message;
        return nullptr;
    }

    return message;
}

void Proxy::Run(int port)
{
    std::cout<<"Start LoadBalancer Using Port: "<<port<<std::endl;
    if(BindTcpSocket(port, SockType::BalancerProxyServer) == C_ERR)
    {
        std::cerr<<"Failed Bind Balancer Tcp Socket"<<std::endl;
        return;
    }
    
    while(true)
    {
        time_clock::time_point start = time_clock::now();
        int retval = el.FetchEvent();
        if(retval > 0 )
        {
            ProcessEvent(retval);
        }

        time_clock::time_point end = time_clock::now();
        std::chrono::milliseconds ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start); 

        tick += ms_duration.count();
        if(tick >= 5000)
        {
            SendHealthCheck();
            tick = 0;
        }
    }
}

void Proxy::SendHealthCheck()
{
    json req = {
        {"cmd", "healthcheck"},
    };

    std::vector<BindComponent*> *binds = bm->GetBinds();

    for(BindComponent *bc: *binds)
    {
        std::vector<Component*> *comps = bc->GetComps();
        for(Component *comp: *comps)
        {
            Net::Socket *socket = el.LoadSocket(comp->fd); 
            if(socket == nullptr)
            {
                continue;
            }

            //std::cout<<ntohs(socket->sock_addr->adr.sin_port)<<std::endl;
            if(socket->SendMsgPackToSocket(req) == C_ERR)
            {
                continue;
            }
        }
    }

    return;
}

void Proxy::ProcessEvent(int retval)
{
    for(int i=0; i<retval; i++)
    {
        Epoll::ev_t e = el.fired[i];
        Net::Socket *socket = el.LoadSocket(e.fd);

        /* EPOLLERR */
        /**
         * NOTE! 
        */
        if(e.mask != EPOLLIN)
        {
            //std::cout<<"[DEBUG] Client disconnected"<<std::endl;
            DeleteSocket(socket);
            continue;
        }

        /* EPOLLIN */
        switch(socket->sock_type)
        {
            case SockType::BalancerProxyServer:
            {
                if(tcp_proxy->TcpClientAccept((Net::TcpSocket*)socket, SockType::BalancerProxyClient) == C_ERR)
                {
                    continue;
                }

                std::cout<<"[DEBUG] Accept Load Balance Component"<<std::endl;
                break;
            }

            case SockType::BalancerProxyClient:
            {
                int err; 
                json res; 
                std::tie(err, res) = ProcessControlChannel(socket);
                if(err == C_ERR)
                {
                    DeleteSocket(socket);
                    continue; 
                }

                if(socket->SendMsgPackToSocket(res) == C_ERR)
                {
                    DeleteSocket(socket);
                    continue;
                }
                
                break;
            }

            case SockType::TcpProxyServer:
            {
                if(tcp_proxy->TcpClientAccept((Net::TcpSocket*)socket, SockType::TcpProxyClient) == C_ERR)
                {
                    continue;
                }

                break;
            }

            case SockType::TcpProxyClient:
            {   
                int ret = tcp_proxy->TcpSendToRealServer(socket);
                if(ret == C_ERR)
                {
                    DeleteSocket(socket);
                    continue;
                }

                else if(ret == C_YET)
                {
                    continue;
                }
                break;
            }

            case SockType::TcpRelayClient:
            {
                int ret = tcp_proxy->TcpSendToClient((Net::TcpSocket *)socket);
                if(ret == C_ERR)
                {
                    DeleteSocket(socket);

                    int cfd = ((Net::TcpSocket *)socket)->connection_pair_fd;
                    Net::Socket *client_socket = el.LoadSocket(cfd);
                    if(client_socket != nullptr)
                    {
                        DeleteSocket(client_socket);
                    }

                    continue;
                }

                else if(ret == C_YET)
                {

                }

                break;
            }

            case SockType::UdpProxyServer:
            {
                int ret = udp_proxy->UdpSendToRealServer((Net::UdpSocket*)socket);
                if(ret == C_ERR)
                {
                    continue;
                }
                break;
            }

            case SockType::UdpProxyClient:
            {
                int ret = udp_proxy->UdpSendToClient((Net::UdpSocket*)socket);
                if(ret == C_ERR)
                {
                    std::cout<<"[Log] Failed UDP Process"<<std::endl;
                }

                DeleteSocket(socket);
                break;
            }

            default:
                break;
        }
    }

    delete el.fired;
}

/**
 * Create Load Balancer Socket
*/
int Proxy::BindTcpSocket(int port, int sock_type)
{
    Net::SockAddr *addr = new Net::SockAddr(port);
    Net::TcpSocket *socket = new Net::TcpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    if(socket->CreateSocket(sock_type, SOCK_STREAM) == C_ERR)
    {
        delete socket;
        return C_ERR;
    } 

    if(socket->BindSocket() == C_ERR)
    {
        delete socket;
        return C_ERR;
    }

    if(socket->ListenSocket() == C_ERR)
    {
        delete socket;
        return C_ERR;
    }

    /* Add Load Balancer Socket in epoll */
    if(el.AddEvent(socket) == C_ERR)
    {
        delete socket;
        return C_ERR;
    }

    return C_OK;
}

int Proxy::BindUdpSocket(int port, int sock_type)
{
    Net::SockAddr *addr = new Net::SockAddr(port);
    Net::UdpSocket *socket = new Net::UdpSocket(addr, EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR);

    if(socket->CreateSocket(sock_type, SOCK_DGRAM) == C_ERR)
    {
        delete socket;
        return C_ERR;
    } 

    if(socket->BindSocket() == C_ERR)
    {
        delete socket;
        return C_ERR;
    }

    return C_OK;
}

std::tuple<int, json> Proxy::ProcessControlChannel(Net::Socket *socket)
{
    if(socket->ReadMsgPackFromSocket() == C_ERR)
    {
        std::cout<<"[DEBUG] Client read Error"<<std::endl;
        return std::make_tuple(C_ERR, json());
    }

    Message *message = ParseMessage(socket);
    if(message == nullptr)
    {
        std::cout<<"[DEBUG] Client Parse Error"<<std::endl;
        return std::make_tuple(C_ERR, json()); 
    }

    std::cout<<message->msg<<std::endl;
    BalancerProxy bproxy = BalancerProxy(&el, bm, socket); 
    json res = bproxy.Controller(message->msg);

    delete message;
    return std::make_tuple(C_OK, res);
}

Proxy::~Proxy()
{
    if(bm != nullptr)
    {
        delete bm;
    }

    if(tcp_proxy != nullptr)
    {
        delete tcp_proxy;
    }

    if(udp_proxy != nullptr)
    {
        delete udp_proxy;
    }
}