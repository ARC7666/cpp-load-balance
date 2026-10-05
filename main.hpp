#ifndef __LOAD_BALANCER_H_
#define __LOAD_BALANCER_H_

#include "common.hpp"
#include "epoll_event.hpp"
#include "component.hpp"
#include "sock.hpp"

#include <tuple>
#include <thread>

std::tuple<int, int> LoadPort(char *argv);


#endif