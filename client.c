#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <vlc/vlc.h>
#include <SDL2/SDL.h>
#include <arpa/inet.h>
#include <sys/socket.h>

volatile int run = 1;
volatile int vlc_ready = 0;

struct video {
    int64_t time;
    int pause;
};

struct rdbtimestamp {
    int seconds;
    int minutes;
    int hours;
};

int vol;
int sock;
int cpause = 0;
int spause = 0;

libvlc_instance_t* instance;
libvlc_media_t* video;
libvlc_media_player_t* player;
libvlc_time_t vtime;
libvlc_time_t tvs = 0;

struct video svideo;
struct rdbtimestamp tsrf;
struct rdbtimestamp ttrf;

void help(char* argv_0){
    printf("Usage: %s [ARGS]\n-ip [ADDRESS]\n-p, -port [INT]\n-n, -username [NAME]\n-h, -help\n-b -max-bytes [INT]\n-v, -verify [0 | 1]\n-u, -update-video [0 | 1]\n", argv_0);
}

int h_recv(int sock, void* ptr, size_t s, int f){
    ssize_t bs = recv(sock, ptr, s, f);

    printf("[CLIENT] [H_RECV] recieved %ld bytes\n", bs);

    return bs != -1;
}

int h_send(int sock, void* ptr, size_t s, int f){
    ssize_t bs = send(sock, ptr, s, f);

    printf("[CLIENT] [H_SEND] sent %ld bytes\n", bs);

    return bs != -1;
}

int exists(char* path) {
    FILE* f = fopen(path, "r");

    if (f != NULL) {
        fclose(f);
        return 1;
    }

    return 0;
}

struct rdbtimestamp ttt(libvlc_time_t seconds){
    struct rdbtimestamp o = {0, 0, 0};

    for (int i = 0; i < seconds; i++) {
        o.seconds += 1;

        if (o.seconds >= 60) {
            o.seconds = 0;
            o.minutes += 1;
        }

        if (o.minutes >= 60) {
            o.minutes = 0;
            o.hours += 1;
        }
    }

    return o;
}

void rect(SDL_Renderer* r, int x, int y, int w, int h){
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

void* video_control(void*){
    if (SDL_Init(SDL_INIT_VIDEO) != 0){
        printf("SDL init error\n");

        return NULL;
    }

    SDL_Window* window = SDL_CreateWindow("Video Control", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 300, SDL_WINDOW_SHOWN);

    if (!window){
        printf("SDL window error\n");
        SDL_Quit();

        return NULL;
    }

    SDL_Renderer* r = SDL_CreateRenderer(window, -1, 0);

    if (!r){
        printf("SDL renderer creation error\n");
        SDL_Quit();

        return NULL;
    }

    SDL_Event e;
    int pgps = 0;

    while (run){
        vtime = libvlc_media_player_get_time(player);
        vol = libvlc_audio_get_volume(player);

        while (SDL_PollEvent(&e)){
            if (e.type == SDL_QUIT){
                printf("SDL quit\n");

                run = 0;
            } if (e.type == SDL_KEYDOWN){
                int op = 2;

                switch (e.key.keysym.sym){
                    case SDLK_SPACE:
                        send(sock, &op, 4, 0);
                        send(sock, &vtime, 8, 0);

                        break;
                    case SDLK_RIGHT:
                        libvlc_media_player_set_time(player, vtime + 5000);

                        break;
                    case SDLK_LEFT:
                        libvlc_media_player_set_time(player, vtime - 5000);

                        break;
                    case SDLK_UP:
                        if (vol < 100){
                            libvlc_audio_set_volume(player, vol + 5);
                        }

                        break;
                    case SDLK_DOWN:
                        if (vol > 0){
                            libvlc_audio_set_volume(player, vol - 5);
                        }

                        break;
                }
            }
        }

        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderClear(r);

        int vpg = (int)((float)vtime / tvs * 800);
        int vopg = vol * 8;

        if (vpg > 800){
            vpg = 800;
        } if (vpg < 0){
            vpg = 0;
        } if (vopg > 800){
            vopg = 800;
        } if (vopg < 0){
            vopg = 0;
        }

        SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
        rect(r, 0, pgps, 800, 30);
        SDL_SetRenderDrawColor(r, 0, 255, 0, 255);
        rect(r, 0, pgps, vpg, 30);

        SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
        rect(r, 0, pgps + 30, 800, 30);
        SDL_SetRenderDrawColor(r, 0, 0, 255, 255);
        rect(r, 0, pgps + 30, vopg, 30);

        if (svideo.pause){
            SDL_SetRenderDrawColor(r, 255, 0, 0, 255);
        } else {
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        }

        rect(r, 0, pgps + 60, 30, 30);

        SDL_RenderPresent(r);
        SDL_Delay(16);
    }

    run = 0;

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void* video_player(void*){
    instance = libvlc_new(0, NULL);

    printf("created instance\n");

    if (!instance) {
        printf("Failed to initialize VLC instance\n");
        return NULL;
    }

    video = libvlc_media_new_path(instance, "video.mp4");

    printf("created video\n");

    if (!video) {
        printf("Failed to load video\n");
        libvlc_release(instance);
        return NULL;
    }

    player = libvlc_media_player_new_from_media(video);

    printf("created player\nvlc ready\n");

    if (!player) {
        printf("Failed to create media player\n");
        libvlc_media_release(video);
        libvlc_release(instance);
        return NULL;
    }

    libvlc_media_player_play(player);
    vlc_ready = 1;

    while (libvlc_media_player_get_state(player) != libvlc_Ended && run){
        if (tvs == 0){
            tvs = libvlc_media_player_get_length(player);
        }

        usleep(1000000);
    }

    libvlc_media_player_stop(player);
    libvlc_media_player_release(player);
    libvlc_media_release(video);
    libvlc_release(instance);

    printf("player stopped\n");

    if (run){
        run = 0;
    }
}

void cvpause(){
    if (libvlc_media_player_is_playing(player)){
        libvlc_media_player_pause(player);
        libvlc_media_player_set_time(player, svideo.time);
    }
}

void cvplay(){
    if (!libvlc_media_player_is_playing(player)){
        libvlc_media_player_play(player);
    }
}

int main(int argc, char** argv){
    int verify = 0;
    int update = 0;
    int port = 25565;
    int ulen = 0;
    int iup = 0;
    int mb = 0xFFFF;
    char* ip = NULL;
    char* username = NULL;
    FILE* tvf;

    if (!exists("volume")){
        tvf = fopen("volume", "w");

        fprintf(tvf, "50");
    } else {
        tvf = fopen("volume", "r");

        char vs[3];

        fgets(vs, 3, tvf);

        vol = atoi(vs);

        if  (1 > vol > 100){
            vol = 50;
        }
    }

    fclose(tvf);

    for (int i = 1; i < argc; i+=2){
        if (argv[i][0] == '-'){
            if (strcmp(&argv[i][1], "ip") == 0){
                ip = malloc(strlen(argv[i+1]));

                memcpy(ip, argv[i+1], strlen(argv[i+1]));
            } else if (strcmp(&argv[i][1], "port") == 0 || strcmp(&argv[i][1], "p") == 0){
                port = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "max-bytes") == 0 || strcmp(&argv[i][1], "b") == 0){
                mb = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "verify") == 0 || strcmp(&argv[i][1], "v") == 0){
                verify = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "update") == 0 || strcmp(&argv[i][1], "u") == 0){
                update = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "username") == 0 || strcmp(&argv[i][1], "n") == 0){
                ulen = strlen(argv[i+1]);
                username = malloc(ulen);

                memcpy(username, argv[i+1], ulen);
            } else if (strcmp(&argv[i][1], "help") == 0 || strcmp(&argv[i][1], "h") == 0){
                help(argv[0]);

                if (username != NULL) free(username);
                if (ip != NULL) free(ip);
                
                return 0;
            }
        } else {
            printf("Unkown arguement %s\n", argv[i]);
        }
    }

    if (ip == NULL || username == NULL || port < 1024 || ulen < 3 || ulen > 64 || mb > 0xFFFF || mb < 1){
        if (ip){
            free(ip);
        } else if (username){
            free(username);
        }

        help(argv[0]);

        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 1){
        perror("there was an error connecting to the server");
        
        return -1;
    }

    int op;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = port;

    inet_pton(AF_INET, ip, &addr.sin_addr);
    printf("connecting to %s:%d\n", ip, port);

    op = 1;

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    h_send(sock, &ulen, 4, 0);
    h_send(sock, username, ulen, 0);

    long fsize;
    char* chunk = NULL;

    printf("checking for video\n");

    if (update){
        remove("video.mp4");
    }

    if (!exists("video.mp4")){
        h_send(sock, &op, 4, 0);
        h_send(sock, &mb, 4, 0);

        FILE* f = fopen("video.mp4", "wb");

        h_recv(sock, &fsize, 8, 0);

        if (fsize > mb){
            chunk = malloc(mb);

            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLIN;

            int progress = 0;

            printf("\n");

            while(poll(&pfd, 1, 1000)){
                ssize_t r = recv(sock, chunk, mb, 0);

                printf("%d%%\r", (int)((progress / (float)fsize) * 100));

                if (r <= 0) break;

                fwrite(chunk, 1, r, f);

                progress += r;
            }
        } else {
            chunk = malloc(fsize);

            h_recv(sock, chunk, fsize, 0);
            fwrite(chunk, 1, fsize, f);
        }

        fclose(f);
    }

    if (verify){
        /* printf("verifying file size\n");

        FILE* vf = fopen("video.mp4", "rb");
        int status = 0;
        long cid = 0;

        fseek(vf, 0, SEEK_END);

        op = 5;
        fsize = ftell(vf);

        printf("verifying %dB\n", fsize);
        h_send(sock, &op, 4, 0);
        h_send(sock, &fsize, 8, 0);
        h_send(sock, &mb, 4, 0);

        printf("differences in chunks: ");

        if (fsize >= mb){
            size_t br;

            if (chunk == NULL){
                chunk = malloc(mb);
            }

            while ((br = fread(chunk, 1, mb, vf)) > 0){
                h_send(sock, chunk, br, 0);
                h_recv(sock, &status, 4, 0);

                if (status){
                    printf("%ld ", cid);
                }
            }
        } else {
            if (chunk == NULL){
                chunk = malloc(fsize);
            }

            fread(chunk, 1, fsize, vf);
            h_send(sock, chunk, fsize, 0);

            h_recv(sock, &status, 4, 0);

            if (status){
                printf("0");
            }
        }

        fclose(vf);
        printf("\n"); */
    }

    if (chunk != NULL){
        printf("freeing chunk\n");

        free(chunk);
    }

    printf("defining threads\n");

    pthread_t vp_thread;
    pthread_t vc_thread;

    op = 3;

    printf("Creating threads\n");
    pthread_create(&vp_thread, NULL, video_player, NULL);

    while (!vlc_ready){}

    libvlc_media_player_pause(player);
    printf("vlc ready, continuing\n");
    pthread_create(&vc_thread, NULL, video_control, NULL);
    printf("Threads created \n");

    int prdy = 0;
    
    while (run){
        send(sock, &op, 4, 0);
        recv(sock, &svideo, 16, 0);

        if (svideo.pause){
            cvpause();
        } else {
            cvplay();
        }

        if (prdy < 2){
            printf("getting timestamp ready\n");
            prdy += 1;
        } else {
            tsrf = ttt(vtime / 1000);
            ttrf = ttt(tvs / 1000);

            printf("Time: %d %d:%d / %d %d:%d Volume: %d%         \r", tsrf.hours, tsrf.minutes, tsrf.seconds, ttrf.hours, ttrf.minutes, ttrf.seconds, vol);
            fflush(stdout);
        }

        usleep(500000);
    }

    printf("\n\n\ncleaning up\n");

    pthread_join(vp_thread, NULL);
    pthread_join(vc_thread, NULL);

    free(ip);
    free(username);

    FILE* vfo = fopen("volume", "w");
    char vas[3];

    sprintf(vas, "%d", vol);
    fprintf(vfo, vas);
    fclose(vfo);
    close(sock);

    return 0;
}