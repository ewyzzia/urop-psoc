import socket
import time


SERVER_IP_ADDR = "192.168.10.1"
SERVER_PORT = 50009

CLIENT_IP_ADDR = "192.168.10.55"
CLIENT_PORT = 50009

my_socket = socket.socket(socket.AddressFamily.AF_INET, socket.SocketKind.SOCK_DGRAM)
SOCKET_TIMEOUT = 1.0
my_socket.settimeout(SOCKET_TIMEOUT)
my_socket.bind((CLIENT_IP_ADDR, CLIENT_PORT))
my_socket.connect((SERVER_IP_ADDR, SERVER_PORT))

my_socket.send(b"START\x00")

bytes_recvd = 0
start_time = time.perf_counter()
try:
    while bytes_recvd < 1024 * 1024 * 20:
        data = my_socket.recv(16384)
        bytes_recvd += len(data)
    end_time = time.perf_counter()
    total_dur = end_time - start_time - SOCKET_TIMEOUT
    print(f"Received {bytes_recvd} bytes in {total_dur} sec " \
        + f"({round(bytes_recvd/total_dur/(1024**2), 3)} MiB/sec)")
    my_socket.close()
except KeyboardInterrupt:
    my_socket.close()