# C++ Layer 4 Load Balancer

A high-performance, single-threaded Layer 4 (Transport Layer) Load Balancer built in C++. This project demonstrates advanced Linux systems programming, utilizing `epoll` for highly concurrent, non-blocking I/O event multiplexing to distribute TCP and UDP traffic efficiently across multiple backend servers.

## 🚀 Key Features

- **Protocol Agnostic L4 Routing**: Seamlessly handles and routes both TCP and UDP traffic.
- **Single-Threaded Event Loop**: Employs a non-blocking architecture using Linux's `epoll` (level-triggered) API. This avoids the overhead, race conditions, and context-switching penalties associated with multi-threaded architectures, allowing it to handle thousands of concurrent connections (similar to the C10k problem solution used by Nginx and Redis).
- **Dynamic Component Registration**: Backend servers can dynamically register and unregister themselves with the load balancer via a dedicated control channel utilizing Msgpack.
- **Health Checking**: Built-in 5-second interval health checks to ensure traffic is only routed to healthy backend nodes.
- **Round-Robin Load Balancing**: Evenly distributes incoming client requests across available servers to optimize resource utilization and maximize throughput.

## 🧠 Architecture Overview

Unlike traditional threaded servers where one thread handles one client, this load balancer relies entirely on asynchronous sockets multiplexed onto a single event loop.

1. **Control Channel**: A TCP socket listens for backend servers to register themselves. They provide their IP, protocol (TCP/UDP), and the port they want the load balancer to expose.
2. **Dynamic Binding**: When a server registers, the load balancer dynamically creates and binds a new socket to listen for incoming client traffic on the requested relay port.
3. **Traffic Relay**: As client packets arrive, `epoll` triggers a read event. The load balancer reads the packet, determines the next healthy backend server via Round-Robin, and forwards the payload.

## 📊 Performance & Benchmarking

To validate the architecture, the load balancer was benchmarked against an industry-standard **Nginx** reverse proxy using custom Python load-testing scripts.

### Test Environment
- Backends: Python Flask Servers (TCP) and Python Echo Servers (UDP)
- Workload: Heavy CPU loops on the backend to simulate blocking operations.

### Results summary:
- **Low/Medium Concurrency**: At lower concurrent request rates (up to 300 concurrent requests), this custom load balancer performed competitively, occasionally matching or slightly edging out Nginx response times (e.g., 100 requests across 2 servers: Nginx at 15.99s, Custom LB at 16.12s).
- **High Concurrency Burst Limits**: During extreme burst tests (10,000 simultaneous TCP connections), the single-threaded nature hit its buffering limits, dropping packets to a ~57% success rate compared to Nginx's robust connection queuing. *This served as an excellent case study in TCP backlog queues, socket buffering limits, and advanced connection tracking.*

## 🛠️ Tech Stack & Dependencies

- **Language**: C/C++ (C++11+)
- **Systems Concepts**: Socket Programming, POSIX APIs, Linux `epoll`
- **Build System**: Make
- **Testing & Tooling**: Python 3.10+, Flask, Msgpack

## 💻 Getting Started

### 1. Build the Load Balancer
```bash
make
./lander [CONTROL_PORT]
```

### 2. Run Backend Servers (in separate terminals)
Ensure you have the required python packages (`pip install flask requests msgpack`).

```bash
cd clients
# Start a TCP (Flask) Server (binds to 30000 locally, requests Load Balancer to relay on 50000)
python3 api_server.py 127.0.0.1 [CONTROL_PORT] 30000 50000

# Start a UDP Echo Server (binds to 17000 locally, requests Load Balancer to relay on 20000)
python3 udp_server.py 127.0.0.1 [CONTROL_PORT] 17000 20000
```

### 3. Run Load Tests
```bash
cd clients
# TCP Test (100 concurrent requests)
python3 dummy_client.py 127.0.0.1 50000 100

# UDP Test (100 concurrent requests)
python3 udp_dummy_client.py 127.0.0.1 20000 100
```
