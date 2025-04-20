import os
import vlc
import socket
import struct
import pygame
import selectors
from sys import argv
from time import sleep
from threading import Thread

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
run = 1
vol = 50
time = 0
pause = 0
rtime = 0
rpause = 0
tvidlen = 0

media = None
player = None
instance = None

arguements = {
    "ip": "127.0.0.1",
    "n": "",
    "p": 25565,
    "u": False,
    "v": False,
    "b": 0xFFFF
}

def send(data: bytes):
    sock.send(data)

def recv(bs: int):
    return sock.recv(bs)

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

def ttt(seconds: int) -> list[int, int, int]:
    o = [0, 0, 0]

    for _ in range(seconds):
        o[2] += 1;

        if o[2] >= 60:
            o[2] = 0;
            o[1] += 1;

        if o[1] >= 60:
            o[1] = 0;
            o[0] += 1;

    return o;

def help():
    exit(f"Usage: {argv[0]} [ARGS]\n-ip [ADDRESS]\n-p, -port [INT]\n-n, -username [NAME]\n-h, -help\n-b -max-bytes [INT]\n-v, -verify [0 | 1]\n-u, -update-video [0 | 1]\n")

def video_player():
    global run
    global vol
    global time
    global media
    global player
    global tvidlen
    global instance

    instance = vlc.Instance("--fullscreen")
    player = vlc.MediaPlayer("video.mp4")
    media = instance.media_new("video.mp4")

    player.set_media(media)
    player.audio_set_volume(vol)
    player.play()
    sleep(0.2)

    while player.is_playing:        
        if not run:
            break

        vol = player.audio_get_volume()
        time = player.get_time()

        if tvidlen < 1:
            tvidlen = player.get_length()

        sleep(0.05)

    if run:
        run = False

def video_control():
    sleep(0.2)

    global run
    global vol
    global time
    global pause

    pygame.init()

    pgps = 0
    window = pygame.display.set_mode((800, 300))
    clock = pygame.time.Clock()
    pygame.display.set_caption("Video Control")

    while run:
        for e in pygame.event.get():
            if e.type == pygame.QUIT:
                print("pygame quit")

                run = False
            if e.type == pygame.KEYDOWN:
                key = pygame.key.get_pressed()

                if key[pygame.K_SPACE]:
                    send(struct.pack('<i', 2))
                    send(struct.pack('<q', time))
                if key[pygame.K_LEFT]:
                    player.set_time(time - 5000)
                if key[pygame.K_RIGHT]:
                    player.set_time(time + 5000)
                if key[pygame.K_UP]:
                    if vol < 100:
                        player.audio_set_volume(vol + 5)
                if key[pygame.K_DOWN]:
                    if vol > 0:
                        player.audio_set_volume(vol - 5)

        vpg = (time + 1) / tvidlen * 800
        vopg = vol * 8
        
        pygame.draw.rect(window, pygame.Color(80, 80, 80), (0, pgps, 800, 30))
        pygame.draw.rect(window, pygame.Color(0, 255, 0), (0, pgps, vpg, 30))
        pygame.draw.rect(window, pygame.Color(80, 80, 80), (0, pgps + 30, 800, 30))
        pygame.draw.rect(window, pygame.Color(0, 0, 255), (0, pgps + 30, vopg, 30))
        
        if rpause:
            pygame.draw.rect(window, pygame.Color(255, 0, 0), (0, pgps + 60, 30, 30))
        else:
            pygame.draw.rect(window, pygame.Color(255, 255, 255), (0, pgps + 60, 30, 30))

        clock.tick(60)
        pygame.display.flip()
    
    pygame.quit()

    if run:
        run = False

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
        print(f"there was an error connecting to server {e}")
        print(f"attempting to connect, tried to connect {ctrys} times.", end="\r")

        ctrys += 1

        if ctrys > 7:
            exit(f"attempted to connect {ctrys} times and failed, quitting...\n")

        sleep(0.5)

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

if not os.path.exists("volume"):
    with open("volume", "w") as f:
        f.write(str(vol))
else:
    with open("volume", "r") as f:
        try:
            vol = int(f.read())

            if 1 > vol > 100:
                vol = 50
        except TypeError:
            vol = 50

auth()
dv()

Thread(target=video_player, daemon=True).start()
Thread(target=video_control, daemon=True).start()

sleep(0.2)

while run:
    send(struct.pack('<i', 3))

    data = recv(16)

    if len(data) < 8:
        print(f"recieved incorrect data {data} ({len(data)})")
        
        run = 0
    
    rtime, rpause = struct.unpack('<qixxxx', data)

    formatted_time = ttt(int(time / 1000))
    total_time = ttt(int(tvidlen / 1000))

    if rpause:
        if player.is_playing():
            player.pause()
            player.set_time(time)
    else:
        if not player.is_playing():
            player.play()

    print(f"Time: {formatted_time[0]} {formatted_time[1]}:{formatted_time[2]} / {total_time[0]} {total_time[1]}:{total_time[2]} Volume: {vol}%", end="     \r");

    sleep(0.2)

if run:
    run = False
    
print("quitting program")

player.stop()
player.release()
instance.release()

with open("volume", "w") as f:
    f.write(str(vol))

# hello >.<