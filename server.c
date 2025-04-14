#include <poll.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <vlc/vlc.h>
#include <arpa/inet.h>
#include <sys/socket.h>

struct client {
    int tup;
    int sock;
    int online;
    int joined;
};

struct video {
    int64_t time;
    int pause;
};

void help(char* argv_0){
    printf("Usage: %s [ARGS]\n-ip [ADDRESS]\n-p, -port [INT]\n-m, -max-connections [INT]\n-h, -help\n-f, -input-file [PATH]\n", argv_0);
}

volatile int run = 1;

int max_conn = 5;
int sock;
char* video_path = NULL;

pthread_t* client_threads;
pthread_t handle_clients_thread;

struct video svideo;
struct client* clients;
struct sockaddr_in addr;

int exists(char* path) {
    FILE* f = fopen(path, "r");

    if (f != NULL) {
        fclose(f);
        return 1;
    }

    return 0;
}

void* handle_threads(void* args){
    printf("[SERVER] [THREAD] handling threads...\n");

    while (run){
        for (int i = 0; i < max_conn; i++){
            if (!clients[i].online && clients[i].tup && !clients[i].joined){
                printf("[CLEANUP] [HANDLE_THREADS] joining handle_client(%d)\n", i);

                if (client_threads[i]){
                    pthread_join(client_threads[i], NULL);
                    printf("[CLEANUP] [HANDLE_THREADS] handle_client(%d) has finished\n", i);
                } else {
                    printf("[CLEANUP] [HANDLE_THREADS] handle_client(%d) is not active\n", i);
                }

                clients[i].tup = 0;
                clients[i].joined = 1;
            }
        }

        usleep(5000);
    }

    printf("[SERVER] [HANDLE_THREADS] thread finished\n");
}

int h_recv(int sock, void* ptr, size_t s, int f){
    ssize_t bs = recv(sock, ptr, s, f);

    printf("[SERVER] [H_RECV] recieved %ld bytes\n", bs);

    return bs;
}

int h_send(int sock, void* ptr, size_t s, int f){
    ssize_t bs = send(sock, ptr, s, f);

    printf("[SERVER] [H_SEND] sent %ld bytes\n", bs);

    return bs;
}

void* handle_client(void* args){
    int id = *(int*) args;
    
    printf("[SERVER] [HANDLE_CLIENT_%d] handling client...\n", id);

    int c;
    int op;
    int fq = 0;
    int usize;
    int vidrecv = 0;
    char auth = 0;
    char* name = NULL;
    struct pollfd pfd;

    pfd.fd = clients[id].sock;
    pfd.events = POLLIN;

    while (run){
        if (fq){
            break;
        }

        int ready = poll(&pfd, 1, 1000);

        if (ready > 0){
            if (auth == 0){
                c = h_recv(clients[id].sock, &usize, 4, 0);

                if (c <= 0){
                    break;
                }

                printf("[SERVER] [HANDLE_CLIENT_%d] username length %d\n", id, usize);

                name = malloc(usize);
                auth = 1;

                for (int i = 0; i < usize; i++){
                    name[i] = 0;
                }
            } else if (auth == 1){
                c = h_recv(clients[id].sock, name, usize, 0);

                if (c <= 0){
                    break;
                }

                printf("[SERVER] [HANDLE_CLIENT_%d] %s: authorized successfully\n", id, name);

                auth = 2;
            } else if (auth == 2){
                c = recv(clients[id].sock, &op, 4, 0);

                if (c <= 0){
                    break;
                }

                if (op != 3){
                    printf("[SERVER] [HANDLE_CLIENT_%d] %s: Recieved op %d\n", id, name, op);
                }

                libvlc_time_t time;
                long fsize;

                switch(op){
                    case 0:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Client sent NOOP \n", id, name);

                        break;
                    case 1:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Sending video? (%d)\n", id, name, vidrecv);

                        if (!vidrecv){
                            FILE* f = fopen(video_path, "rb");

                            fseek(f, 0, SEEK_END);

                            fsize = ftell(f);
                            int mb = 0xFFFF;
                            char* chunk;

                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: sending %ldB file\n", id, name, fsize);

                            fseek(f, 0, SEEK_SET);
                            h_recv(clients[id].sock, &mb, 4, 0);

                            if (1 > mb > 0xFFFF){
                                printf("[SERVER] [HANDLE_CLIENT_%d] %s: wants to recieve above or below mb limit (%d)\n", id, name, mb);

                                fq = 1;

                                break;
                            }

                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: sending video in %dB chunks\n", id, name, mb);

                            int fsst = send(clients[id].sock, &fsize, 8, 0);

                            if (fsst <= 0){
                                printf("[SERVER] [HANDLE_CLIENT_%d] %s: disconnected during video transfer\n", id, name);
                                
                                fq = 1;

                                break;
                            }

                            if (fsize > mb){
                                size_t br;
                                chunk = malloc(mb);

                                while ((br = fread(chunk, 1, mb, f)) > 0){
                                    ssize_t status = send(clients[id].sock, chunk, br, 0);

                                    if (status < 0){
                                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: disconnected during video transfer\n", id, name);

                                        fq = 1;

                                        break;
                                    }
                                }
                            } else {
                                chunk = malloc(fsize);

                                fread(chunk, 1, fsize, f);
                                h_send(clients[id].sock, chunk, fsize, 0);
                            }

                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: finished sending video in %dB chunks\n", id, name, mb);

                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: cleaning up\n", id, name);
                            free(chunk);
                            fclose(f);

                            vidrecv = 1;
                        } else {
                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: client already has video\n", id, name);

                            fq = 1;
                        }

                        break;
                    case 2:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Client sent PAUSE\n", id, name);
                        h_recv(clients[id].sock, &time, 8, 0);

                        svideo.pause = !svideo.pause;
                        svideo.time = time;

                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: (%d) time %ld\n", id, name, svideo.pause, time);

                        break;
                    case 3:
                        send(clients[id].sock, &svideo, 16, 0);

                        break;
                    /* case 4:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Client sent SKIP\n", id, name);
                        h_recv(clients[id].sock, &time, sizeof(libvlc_time_t), 0);

                        svideo.time = time;

                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: (%d) time %ld\n", id, name, svideo.pause, time);

                        break; */
                    case 5:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Verifying if client has video\n", id, name);

                        FILE* vf = fopen(video_path, "r");
                        long chid = 0;
                        int vcs = 0xFFFF;
                        char* cc;
                        char* sc;
                        
                        h_recv(clients[id].sock, &fsize, 8, 0);
                        h_recv(clients[id].sock, &vcs, 4, 0);

                        if (1 > vcs > 0xFFFF){
                            printf("[SERVER] [HANDLE_CLIENT_%d] %s: wants to recieve above or below mb limit (%d)\n", id, name, vcs);

                            fq = 1;
                            break;
                        }

                        cc = malloc(vcs);
                        sc = malloc(vcs);

                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: differences in chunks: ", id, name);

                        if (fsize > vcs){
                            struct pollfd pfd;
                            pfd.fd = sock;
                            pfd.events = POLLIN;

                            printf("\n");

                            while(poll(&pfd, 1, 1000)){
                                ssize_t r = recv(sock, cc, vcs, 0);

                                recv(clients[id].sock, &cc, 4, 0);
                                fread(sc, 1, vcs, vf);

                                int status = strcmp(cc, sc);

                                if (status != 0){
                                    printf("%ld ", chid);
                                }

                                if (r <= 0) break;

                                send(clients[id].sock, &status, 4, 0);
                            }
                        } else {
                            recv(clients[id].sock, &cc, 4, 0);
                            fread(sc, 1, vcs, vf);

                            int status = strcmp(cc, sc);

                            if (status != 0){
                                printf("0 ");
                            }

                            send(clients[id].sock, &status, 4, 0);
                        }

                        printf("\n[SERVER] [HANDLE_CLIENT_%d] %s: finished verifying video in %dB chunks\n", id, name, vcs);

                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: cleaning up\n", id, name);
                        free(cc);
                        free(sc);
                        fclose(vf);

                        break;
                    default:
                        printf("[SERVER] [HANDLE_CLIENT_%d] %s: Unknown op %d\n", id, name, op);

                        // fq = 1;

                        break;
                }
            }
        } else if (ready < 0){
            printf("[SERVER] [HANDLE_CLIENT_%d] ", id);
            perror("Error: ");
            break;
        }
    }

    clients[id].online = 0;

    close(clients[id].sock);

    if (name == NULL) free(name);

    printf("[SERVER] [HANDLE_CLIENT_%d] thread finished\n", id);
}

void* handle_clients(void* args){
    printf("[SERVER] [THREAD] listening for connections...\n");

    struct sockaddr* h_addr = (struct sockaddr*)&addr;
    socklen_t addr_len = sizeof(addr);

    while (run){
        struct pollfd pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;

        int ready = poll(&pfd, 1, 1000);

        if (ready > 0){
            int cid = -1;
            int* cidp;

            printf("[SERVER] [HANDLE_CLIENTS] poll success\n");

            int current_client = accept(sock, h_addr, &addr_len);

            if (!run){
                break;
            }

            for (int i = 0; i < max_conn; i++){
                if (clients[i].online == 0){
                    cid = i;
                    clients[i].online = 1;
                    clients[i].sock = current_client;
                    clients[i].tup = 1;
                    clients[i].joined = 0;

                    break;
                }
            }

            if (cid < 0){
                printf("[SERVER] [HANDLE_CLIENTS] 1: client unable to join, no available space\n");
            } else {
                *cidp = cid;

                printf("[SERVER] [HANDLE_CLIENTS] 0: client %d successfully joined\n", *cidp);

                pthread_create(client_threads, NULL, handle_client, cidp);
            }
        } else {
            /* printf("Poll count: %d\n", poll_count);
            poll_count += 1; */
        }

    }

    printf("[SERVER] [HANDLE_CLIENTS] thread finished\n");
}

void* handle_server_terminal(void* args){
    char cmd_buf[20];
    struct pollfd ifd[1];

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    ifd[0].fd = STDIN_FILENO;
    ifd[0].events = POLLIN;

    while (run){
        if (poll(ifd, 1, 1000) > 0 && (ifd[0].revents & POLLIN)){
            if (fgets(cmd_buf, 20, stdin) != NULL){
                cmd_buf[strcspn(cmd_buf, "\n")] = 0;

                if (strcmp(cmd_buf, "stop") == 0){
                    printf("[SERVER] [STOP] triggered\n");

                    run = 0;
                } else {
                    printf("Unkown command: %s\n", cmd_buf);
                }
            }
        }

        // usleep(500000);
    }
}

void handle_quit(int sig){
    printf("\n[SERVER] [STOP] triggered\n");

    run = 0;
}

int main(int argc, char** argv){
    int port = 25565;
    int ulen;
    char* ip = NULL;

    for (int i = 1; i < argc; i+=2){
        if (argv[i][0] == '-'){
            if (strcmp(&argv[i][1], "ip") == 0){
                ip = malloc(strlen(argv[i+1]));
                memcpy(ip, argv[i+1], strlen(argv[i+1]));
            } else if (strcmp(&argv[i][1], "input-file") == 0 || strcmp(&argv[i][1], "f") == 0){
                video_path = malloc(strlen(argv[i+1]));
                memcpy(video_path, argv[i+1], strlen(argv[i+1]));
            } else if (strcmp(&argv[i][1], "port") == 0 || strcmp(&argv[i][1], "p") == 0){
                port = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "port") == 0 || strcmp(&argv[i][1], "p") == 0){
                port = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "max-connections") == 0 || strcmp(&argv[i][1], "m") == 0){
                max_conn = atoi(argv[i+1]);
            } else if (strcmp(&argv[i][1], "help") == 0 || strcmp(&argv[i][1], "h") == 0){
                help(argv[0]);

                return 0;
            }
        } else {
            printf("Unkown arguement %s\n", argv[i]);
        }
    }

    if (ip == NULL || video_path == NULL){
        help(argv[0]);

        if (ip){
            free(ip);
        } if (video_path){
            free(video_path);
        }

        return -1;
    }

    signal(SIGINT, handle_quit);

    clients = malloc(max_conn * sizeof(struct client));
    client_threads = malloc(max_conn * sizeof(pthread_t));

    printf("[SERVER] [START] using %d bytes for clients and client_threads. using %d bytes for video\n", max_conn * sizeof(struct client) + max_conn * sizeof(int), sizeof(struct video));

    for (int i = 0; i < max_conn; i++){
        clients[i].online = 0;
        clients[i].tup = 0;
        clients[i].joined = 0;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    svideo.pause = 1;
    svideo.time = 5;
    inet_pton(AF_INET, ip, &addr.sin_addr);

    printf("[SERVER] [START] starting server at %s:%d, max connections: %d\n", ip, port, max_conn);

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, max_conn);

    pthread_t handle_clients_id;
    pthread_t handle_threads_id;
    // pthread_t handle_server_terminal_id;

    pthread_create(&handle_clients_id, NULL, handle_clients, NULL);
    pthread_create(&handle_threads_id, NULL, handle_threads, NULL);
    // pthread_create(&handle_server_terminal_id, NULL, handle_server_terminal, NULL);

    while (run){}

    close(sock);

    printf("[CLEANUP] [MAIN] stopping threads\n");
    // pthread_join(handle_server_terminal_id, NULL);
    // printf("[CLEANUP] [MAIN] handle_server_terminal thread stopped...\n");
    pthread_join(handle_clients_id, NULL);
    printf("[CLEANUP] [MAIN] handle_clients thread stopped...\n");
    pthread_join(handle_threads_id, NULL);
    printf("[CLEANUP] [MAIN] handle_threads thread stopped...\n");

    for (int i = 0; i < max_conn; i++){
        if (client_threads[i]){
            printf("[CLEANUP] [MAIN] stopping thread handle_client(%d)\n", i);
            pthread_join(client_threads[i], NULL);
            printf("[CLEANUP] [MAIN] thread handle_client(%d) stopped\n", i);
        }
    }

    printf("[CLEANUP] [MAIN] freeing memory\n");
    free(ip);
    free(clients);
    free(video_path);
    free(client_threads);

    return 0;
};

// hello o.o