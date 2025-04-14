w2gc is a watch-together app where you can watch videos in real-time with your friends! **NOTE** this project is still a W.I.P, feedback and critizism is apprecciated.

**NOTE** this project is meant for Linux and running / compiling it on windows won't work. Using the python client is recommended but you will need WSL for the server.

## Instructions

### Depencies:

`sdl2`, `vlc`, `libvlc`

Installing on arch

```sh
sudo pacman -S sdl2 vlc libvlc
```

### Compiling

Compiling client:

```sh
gcc server.c -o server $(pkg-config --cflags --libs libvlc)
```

Compiling server:

```sh
gcc client.c -o client $(pkg-config --cflags --libs libvlc) $(sdl2-config --cflags --libs)
```

### Running

Before you run the server, you need to specify some arguements so the server knows what to do... You could run help, it displays all the commands but doesn't really give much information about them.

- `-ip` IP address, where the server will be hosted
- `-p, -port` port of the server
- `-m, -max-connections` maximum amount of connections, at a time
- `-h, -help` displays a help message
- `-f, -input-file` path to the video used in the watch-together

An example of running a server on localhost:25565 with the video video.mp4

```sh
./server -ip localhost -p 25565 -m 5 -f video.mp4
```

Now, running the client is pretty straight forward, same with the server, you can run help but it doesn't display that much information.

- `-ip` IP address, ip of the server to join
- `-p, -port` port of the server
- `-n, -username` username of the client
- `-h, -help` displays a help message
- `-b, -max-bytes` for downloading the video, maximum amount of bites to recieve from the server in chunks, max and default = `0xFFFF`
- `-v, -verify` W.I.P, verify if your video is the same as the servers
- `-u, -update-video` when joining the server, replace the current video.mp4 with the server's video, not recommended to run if you already have the same video as the server.

An example of joining the server we just ran

```sh
./client -ip localhost -p 25565 -n 54p
```

### Usage

#### Client usage

The client is pretty simple, once you've connected, 2 windows should pop-up, 1 window being the media player (VLC) and the other, a video control "panel" (SDL)

Focus on the video control "panel" to use it, you can do the following:

- `SPACE` pause the video for everyone, including you
- `LEFT` skip back 5 seconds
- `RIGHT` skip forward 5 seconds
- `UP` increase volume by 5%
- `DOWN` decrease volume by 5%

If the video control window is closed, the client will automaticly quit and close the media player as well as clean up. It is recommend to close the video control window so you get a clean exit.