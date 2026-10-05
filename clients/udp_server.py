import threading
from socket import *
import msgpack 
import struct 
import json
import time
import sys

ApiPORT = 15000
bindPORT = 20000

def create_msgpack(value):
    byte = msgpack.packb(value)
    length = struct.pack('i', len(byte))
    msg = length + byte
    return msg 

def register(server_port, bind_port):
    msg = create_msgpack({
        "cmd" : "register", 
        "protocol" : "udp", 
        "port" : bind_port, 
        "relay_port" : server_port
    })
    
    return msg

def healthcheck(bind_port):
    msg = create_msgpack({
        "cmd" : "healthcheck",
        "protocol" : "udp", 
        "port" : bind_port,
        "state" : "good"
    })
    
    return msg


def unregister(server_port, bind_port):
    msg = create_msgpack({
        "cmd" : "unregister", 
        "protocol" : "udp", 
        "port" : bind_port, 
        "relay_port" : server_port
    })
    
    return msg

# Control Channel
def send_handler(client_socket, msg):
    client_socket.send(msg)


def recv_handler(client_socket, bind_port):
    while True:
        data = client_socket.recv(4096)
        if len(data) < 4:
            return 
        
        while True:
            offset = 0
            unpack_data = struct.unpack("i", data[offset:4])
            length_r = unpack_data 
            decoded_data = msgpack.unpackb(data[offset+4:length_r[0]+4], raw=False)
            json_data = json.dumps(decoded_data, indent=2)
            
            response = json.loads(json_data)

            if "cmd" in response:
                if response["cmd"] == "healthcheck":
                    #print("Health Check!")
                    s_thr = threading.Thread(target=send_handler, args=(client_socket, healthcheck(bind_port), ))
                    s_thr.start()
                    
                else:
                    pass
                    
            else:
                pass
                #print(response)
                
            
            offset = 4 + length_r[0]
            if offset >= len(data):
                break; 


def connect_server(ip, port):
    client_socket = socket(AF_INET, SOCK_STREAM)
    client_socket.connect((ip, port))
    return client_socket

def echo_server(server_port):
    server_socket = socket(AF_INET, SOCK_DGRAM)
    address = ('0.0.0.0', server_port)
    server_socket.bind(address)
    
    while True:
        data, address = server_socket.recvfrom(4096)
        #print(f"수신한 데이터: {data.decode()} from Client")
        data = b"hello world" * 800
        server_socket.sendto(data, address)
        print("전송 완료")

if __name__ == '__main__':
    
    if len(sys.argv) != 5:
        print("[Usage] python3 udp_server.py [LoadBalancer IP] [LoadBalancer Port] [API Server Port] [Bind Port(Relay)]")
        exit(1)
    # Control Channel run 
    client_socket = connect_server(sys.argv[1], int(sys.argv[2]))
    
    r_thr = threading.Thread(target=recv_handler, args=(client_socket, int(sys.argv[4]), ))
    s_thr = threading.Thread(target=send_handler, args=(client_socket, register(int(sys.argv[3]), int(sys.argv[4])), ))
    
    r_thr.start()
    s_thr.start()
    
    time.sleep(1)
    # Echo Server Run 
    echo_server(int(sys.argv[3]))