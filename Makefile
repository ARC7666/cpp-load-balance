LDLIBS = -lpthread

all: lander

lander: main.o sock.o epoll_event.o udp_proxy.o tcp_proxy.o balancer.o component.o proxy.o message.o
	g++ -o lander main.o sock.o epoll_event.o tcp_proxy.o udp_proxy.o balancer.o component.o proxy.o message.o $(LDLIBS)

main.o: main.hpp main.cpp 
	g++ -c -g -o main.o main.cpp

epoll_event.o: epoll_event.hpp epoll_event.cpp 
	g++ -c -g -o epoll_event.o epoll_event.cpp 

sock.o: sock.hpp sock.cpp
	g++ -c -g -o sock.o sock.cpp

balancer.o: balancer.hpp balancer.cpp
	g++ -c -g -o balancer.o balancer.cpp

tcp_proxy.o: tcp_proxy.hpp tcp_proxy.cpp
	g++ -c -g -o tcp_proxy.o tcp_proxy.cpp

udp_proxy.o: tcp_proxy.hpp udp_proxy.cpp
	g++ -c -g -o udp_proxy.o udp_proxy.cpp

component.o: component.hpp component.cpp
	g++ -c -g -o component.o component.cpp

proxy.o: proxy.hpp proxy.cpp 
	g++ -c -g -o proxy.o proxy.cpp 

message.o: message.hpp message.cpp
	g++ -c -g -o message.o message.cpp

clean: 
	rm *.o lander