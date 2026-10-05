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

// bind목록 컴포넌트 목록등 확인하여 제거 필요
int Proxy::DeleteSocket(Net::Socket *socket)
{
    // bind 목록에서 먼저 제거
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
         * 연결이 끊어졌을 경우 컴포넌트 리스트에서 삭제
         * 현재 바인드하고 있는 포트 레퍼런스 제거
        */
        if(e.mask != EPOLLIN)
        {
            //std::cout<<"[DEBUG] Client disconnected"<<std::endl;
            DeleteSocket(socket);
            continue;
        }

        /* EPOLLIN */
        /* 이벤트가 발생한 socket에서 어떤 이벤트가 발생했는지 확인 필요 */
        switch(socket->sock_type)
        {
            /* 컴포넌트 소켓 추가 */
            case SockType::BalancerProxyServer:
            {
                if(tcp_proxy->TcpClientAccept((Net::TcpSocket*)socket, SockType::BalancerProxyClient) == C_ERR)
                {
                    continue;
                }

                std::cout<<"[DEBUG] Accept Load Balance Component"<<std::endl;
                break;
            }

            /* 컴포넌트의 메시지 처리 */
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

            /* 연결된 TCP 서버로 릴레이 */
            case SockType::TcpProxyServer:
            {
                if(tcp_proxy->TcpClientAccept((Net::TcpSocket*)socket, SockType::TcpProxyClient) == C_ERR)
                {
                    continue;
                }

                break;
            }

            /* 컴포넌트에 접근하기를 원하는 외부 요청 */
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

            /* 처음 요청한 클라이언트에게 전달 */
            /* 이때 socket은 API 서버에 연결한 socket */
            /* connection_pair_fd는 로드밸런서에 연결 시도한 사용자 */
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

            /* 연결된 UDP 서버로 릴레이 */
            /* UDP는 클라이언트없이 여기서 바로 하는 듯 */
            case SockType::UdpProxyServer:
            {
                int ret = udp_proxy->UdpSendToRealServer((Net::UdpSocket*)socket);
                if(ret == C_ERR)
                {
                    continue;
                }
                break;
            }

            /* 컴포넌트에 접근하기를 원하는 외부 요청 */
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