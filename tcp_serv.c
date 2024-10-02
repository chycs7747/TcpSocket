#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "protocol.h"



void error_handling(char *message);

int main(int argc, char* argv[]) {
    int listenfd, connfd, sockfd;
    int nready, client[FD_SETSIZE];
    unsigned int clilen;
    struct sockaddr_in servaddr, cliaddr;
    fd_set rset, allset;
    struct Message msg;
    int msg_byte = 0;
    
    int i, maxfd, maxi;

    char client_ip[FD_SETSIZE][INET_ADDRSTRLEN];
    int client_port[FD_SETSIZE];
    char client_names[FD_SETSIZE][NAME_SIZE] = {0};


    if (argc!=2){
		printf("Usage : %s <port>\n", argv[0]);
        fflush(stdout);
		exit(1);
	}


    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    
    if (listenfd == -1) {
        error_handling("socket() error");
    }


    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(atoi(argv[1]));


    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        error_handling("server bind() error");
    }

    if (listen(listenfd, MAX_CLNT) == -1){
		error_handling("server listen() error");
	}

    
    for (i = 0; i < FD_SETSIZE; i++) {
		client[i] = -1;
    }
    FD_ZERO(&rset);
	FD_ZERO(&allset);
	FD_SET(listenfd, &allset);
    maxfd = listenfd;
    maxi = -1;


    while(1) {
        memset(&(msg), 0, sizeof(msg));
        rset = allset;

        if ((nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) { 
            error_handling("error select");
            exit(1);
        }


        if (FD_ISSET(listenfd, &rset)) {
            clilen = sizeof(cliaddr);
            if ((connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen)) < 0) {
                error_handling("error accept");
                exit(1);
            }

            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = connfd;

                    msg.header.type = INIT_USER;
                    strcpy(msg.buf, "Input your name: ");
                    send(client[i], &msg, sizeof(msg), 0);

                    inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip[i], sizeof(client_ip[i]));
                    client_port[i] = ntohs(cliaddr.sin_port);

                    printf("client %d connected from %s:%d\n", i+1, client_ip[i], client_port[i]);
                    fflush(stdout);
                    break;
                }
            }

            if (i == FD_SETSIZE) {
                msg.header.type = -1;
                strcpy(msg.buf, "too many clients");
                send(connfd, &msg, sizeof(msg), 0);
                error_handling("too many clients");
                exit(1);
            }
            
            FD_SET(connfd,&allset);
            
            if (connfd > maxfd) {
                maxfd = connfd;
            }
            
            if (i > maxi) {
                maxi = i;
            }
            
            if (--nready <= 0) {
                continue;
            }
        }

        for (i = 0; i <= maxi; i++) {

            if ( (sockfd = client[i]) < 0) {
                continue;
            }

            if (FD_ISSET(sockfd, &rset)) {
                int n = read(sockfd, &msg, sizeof(msg));
                
                if (n == 0) {
                    printf("Client %d(%s) disconnected\n", i + 1, client_names[i]);
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                }

                else if (msg.header.type == INIT_USER) {
                    printf("Client %d made init user name: \"%s\" from client\n", i + 1, msg.buf);
                    strcpy(client_names[i], msg.buf);
                }
                
                else if (msg.header.type == MSG) {
                    printf("Received request from client %d(%s): %s\n", i + 1, msg.header.src_name, msg.buf);

                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1 && client[j] != sockfd) {
                            send(client[j], &msg, sizeof(msg), 0);
                        }
                    }
                }
                
                else if (msg.header.type == WHISPER) {
                    printf("Received whisper request from client %d to %s: %s\n", i + 1, msg.header.dest_name, msg.buf);
                    int target_found = 0;
                    
                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1 && strncmp(client_names[j], msg.header.dest_name, NAME_SIZE) == 0) {
                            send(client[j], &msg, sizeof(msg), 0);
                            printf("Whisper sent to %s (client %d)\n", client_names[j], j + 1);
                            target_found = 1;
                            break;
                        }
                    }
                    
                    if (!target_found) {
                        printf("Whisper target not found: %s\n", msg.header.dest_name);
                    }
                }

                else if (strncmp(msg.buf, "/list", 5) == 0) {
                    printf("Received list request from client %d\n", i + 1);
                    char list_buf[BUF_SIZE] = "[Connected clients]\n";
                    
                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1) {
                            char client_info[64];
                            
                            if (client[j] != sockfd) {
                                snprintf(client_info, sizeof(client_info), "%s:%d(%s)\n", client_ip[j], client_port[j], client_names[j]);
                            }
                            
                            else {
                                snprintf(client_info, sizeof(client_info), "%s:%d(me)\n", client_ip[j], client_port[j]);
                            }
                            
                            strcat(list_buf, client_info);
                        }
                    }
                    strcpy(msg.buf, list_buf);
                    send(client[i], &msg, sizeof(msg), 0);
                }

                else {
                    printf("Unknown request from client %d\n", i + 1);
                }
            }
        }
    }
    return 0;
}

void error_handling(char *message){
	fputs(message, stderr);
	fputc('\n',stderr);
	exit(1);
}