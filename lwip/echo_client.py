import socket
import cv2
import numpy as np
import matplotlib.pyplot as plt
from time import time

'''
Link speed  : 1 Gbps
Speed       : 42.81 MB/s = 342.463819 Mbps
Time for 100 RGB frames (384,384) = 2.09 seconds
'''

HOST = '192.168.1.10'
PORT = 7

RECEIVE_SIZE = 1024*1024
PACKET_LENGTH = 1446
# PACKET_LENGTH = 9000 # Jumbo

TRIES = 100

tx_img = cv2.imread('tx.png')
tx_img = cv2.resize(tx_img, (384, 384))

shape = tx_img.shape
size = tx_img.size

tx_img = tx_img.flatten()
tx_bytes = tx_img.tobytes()

rx_bytes_list = []
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))

    time_start = time()
    for _ in range(TRIES):
        s.sendall(tx_bytes)
        rx_bytes_len = 0

        while rx_bytes_len != size:
            rx_packet = s.recv(RECEIVE_SIZE)
            if not rx_packet:
                break
            else:
                rx_bytes_list += [rx_packet]
                rx_bytes_len += len(rx_packet)

    t = time() - time_start
    mbps = 2*TRIES*size/(t*1e6)
    print(f"{t:.2f} seconds for {TRIES} tries => ",
          f"{mbps:.2f} MB/s ", f"{8*mbps:2f} Mbps")

print('Received')

shape_rx = [TRIES] + list(shape)

rx_bytes = b''.join(rx_bytes_list)
rx_img = np.frombuffer(rx_bytes, np.uint8).reshape(shape_rx)[0]

plt.imshow(rx_img[:, :, ::-1])
plt.show()
