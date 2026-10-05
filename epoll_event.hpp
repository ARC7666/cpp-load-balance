#ifndef __EPOLL_EVENT_H_
#define __EPOLL_EVENT_H_

#include <sys/epoll.h>
#include <vector>
#include <algorithm>
#include "sock.hpp"
#include "common.hpp"


#define MAX_EVENT_SIZE 512

namespace Epoll
{
    struct ev_t
    {
        int fd; 
        int mask;
    };

    class EventLoop
    {
        private:
            int epfd;

        public:
            std::vector<Net::Socket*> events;
            ev_t *fired = nullptr;

            EventLoop();
            ~EventLoop();
            int CreateEventLoop();
            int AddEvent(Net::Socket *socket);
            int DelEvent(Net::Socket *socket);
            Net::Socket *LoadSocket(socket_t fd);
            int FetchEvent();
    };
}

#endif 