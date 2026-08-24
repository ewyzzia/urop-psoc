import socket
import time
import matplotlib.pyplot as plt

SERVER_IP_ADDR = "192.168.10.1"
SERVER_PORT = 50009

CLIENT_IP_ADDR = "192.168.10.55"
CLIENT_PORT = 50009

my_socket = socket.socket(socket.AddressFamily.AF_INET, socket.SocketKind.SOCK_DGRAM)
SOCKET_TIMEOUT = 5.0
my_socket.settimeout(SOCKET_TIMEOUT)
my_socket.bind((CLIENT_IP_ADDR, CLIENT_PORT))
my_socket.connect((SERVER_IP_ADDR, SERVER_PORT))


if __name__ == "__main__":

    

    try:
        while True:
            bytes_received = []
            for i in range(1, 25):
                data = my_socket.recv(16384)
                bytes_received.extend(data)
            sorted_list = []
            for i in range(0, len(bytes_received), 4):
                sorted_list.append( int.from_bytes(bytes_received[i:i+4], "little") )
            sorted_list.sort()

            drops = 0
            for i in range(1, len(sorted_list)):
                drops += 1 if sorted_list[i] != sorted_list[i-1] + 1 else 0 
            x_series = [i for i in range(0, len(sorted_list))]

            print(f"{drops = }")

            #plt.plot( x_series, sorted_list )
            #plt.show()

            time.sleep(1.0)

        

    except KeyboardInterrupt:
        my_socket.close()