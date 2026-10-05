#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "main.hpp"

#include "balancer.hpp"
#include "tcp_proxy.hpp"
#include "udp_proxy.hpp"
#include "common.hpp"
#include "component.hpp"
#include "proxy.hpp"

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        std::cerr<<"./lander needs two argument."<<std::endl;
        return 1;
    }
    
    /* Load Port From Argv[1] */
    int err, port;
    std::tie(err, port) = LoadPort(argv[1]);
    if(err == C_ERR)
    {
        std::cerr<<"Invalid Port Number: "<<argv[1]<<std::endl;
        return 1;
    }

    try
    {
        Proxy *proxy = new Proxy();
        proxy->Run(port);

        delete proxy;
    }
    catch(std::exception& e)
    {
        std::cout<<"Bad Alloc"<<std::endl; 
        return 1;
    }
    
    return 0;
}

std::tuple<int, int> LoadPort(char *argv)
{
    std::string arg = argv;
    int port; 

    try
    {
        std::size_t pos; 
        port = std::stoi(arg, &pos);
        if(pos < arg.size())
        {
            return std::make_tuple(C_ERR, 0);
        }        
    }
    catch (std::invalid_argument const &ex) 
    {
        std::make_tuple(C_ERR, 0);
    } 
    catch (std::out_of_range const &ex) 
    {
        std::make_tuple(C_ERR, 0);
    }

    return std::make_tuple(C_OK, port);
}
