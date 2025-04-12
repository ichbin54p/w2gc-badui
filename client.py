import os
import socket
import struct
import threading
import selectors
from sys import argv
from time import sleep

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);
run = 0;

arguements = {
    "ip": "127.0.0.1",
    "n": "",
    "p": 25565,
    "u": False,
    "v": False,
    "b": 0xFFFF
}

def send(data):
    sock.send(data)

def recv(bs: int):
    return sock.recv(bs)

def hc():
    while run:
        pass

def dv():
    print("checking for video...")

    if os.path.exists("video.mp4") and arguements['u']:
        os.remove("video.mp4")

    if not os.path.exists("video.mp4"):
        send(struct.pack('<i', 1))
        send(struct.pack('<i', arguements['b']))

        progress = 0
        fsize = struct.unpack('<q', recv(8))[0]
        poll = selectors.DefaultSelector()
        poll.register(sock, selectors.EVENT_READ)

        print(f"downloading {fsize}B video...")

        with open("video.mp4", "ab") as f:
            while poll.select(timeout=1):
                r = recv(arguements['b'])

                print(f"{int((progress / fsize) * 100)}%", end="\r")

                if not r:
                    break

                f.write(r)

                progress += len(r)
    

def auth():
    send(struct.pack('<i', len(arguements['n'])))
    send(arguements['n'].encode())

def help():
    exit(f"Usage: {argv[0]} [ARGS]\n-ip [ADDRESS]\n-p, -port [INT]\n-n, -username [NAME]\n-h, -help\n-b -max-bytes [INT]\n-v, -verify [0 | 1]\n-u, -update-video [0 | 1]\n")

for i in range(1, len(argv), 2):
    if argv[i] == "-ip":
        arguements['ip'] = argv[i+1]
    elif argv[i] == "-p" or argv[i] == "-port":
        arguements['p'] = int(argv[i+1])
    elif argv[i] == "-n" or argv[i] == "-username":
        arguements['n'] = argv[i+1]
    elif argv[i] == "-h" or argv[i] == "-help":
        help()
    elif argv[i] == "-b" or argv[i] == "-max-bytes":
        arguements['b'] = int(argv[i+1])
    elif argv[i] == "-v" or argv[i] == "-verify":
        arguements['v'] = bool(int(argv[i+1]))
    elif argv[i] == "-u" or argv[i] == "-update-video":
        arguements['u'] = bool(int(argv[i+1]))
    else:
        print(f"Unkown arguemnt {argv[i]}")

if arguements['n'] == None or len(arguements['n']) < 2 or len(arguements['n']) > 64 or arguements['b'] > 0xFFFF or arguements['b'] < 1 or arguements['p'] < 1024:
    help()

print(f"connecting to {arguements['ip']}:{arguements['p']}")

ctrys = 0

while True:
    try:
        sock.connect((arguements['ip'], arguements['p']))

        break
    except ConnectionRefusedError as e:
        print(f"there was an error connecting to server {e}");
        print(f"attempting to connect, tried to connect {ctrys} times.", end="\r");

        ctrys += 1;

        if ctrys > 7:
            exit(f"attempted to connect {ctrys} times and failed, quitting...\n");

        sleep(0.5)

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM);

auth()
dv()