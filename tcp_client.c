#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "protocol.h"



void error_handling(char *message);
void help();
void communicate(int sockfd);
void init_usr(int sockfd, struct User *usr, struct Message *msg);
void whisper(struct Message *msg, struct User *usr);
void setting(struct User *usr);


int main(int argc, char* argv[]) {
    int sockfd;
    struct sockaddr_in cliaddr;

    if(argc != 3){
		printf("Usage : %s <IP> <port>\n",argv[0]);
		exit(1);
	}

    printf("Check Usage with \"/help\" command.\n");
    fflush(stdout);
   
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
		error_handling("client socket() error");
	}

    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr=inet_addr(argv[1]);
	cliaddr.sin_port=htons(atoi(argv[2]));
    memset(&(cliaddr.sin_zero), '\0', 8);

    if(connect(sockfd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))==-1) {
        error_handling("client connect() error");
    }
    

    communicate(sockfd);


    close(sockfd);
}

void error_handling(char *message){
	fputs(message, stderr);
	fputc('\n',stderr);
	exit(1);
}

void communicate(int sockfd) {
    fd_set rset, allset;
    int maxfd, maxi, nready;
    struct User usr;
    struct Message msg;
    int n;
    int fin;
    

    fin = 0;
    FD_ZERO(&rset);
	FD_ZERO(&allset);
    FD_SET(sockfd, &allset);
    FD_SET(fileno(stdin), &allset);
    maxfd = max(fileno(stdin), sockfd);



    while(1) {
        memset(&(msg), 0, sizeof(msg));
        rset = allset;

        if ((nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) { 
            error_handling("error select");
            exit(1);
        }


        if (FD_ISSET(sockfd, &rset)) {
            memset(&msg, 0, sizeof(msg));
            n = recv(sockfd, &msg, sizeof(msg), 0);

            if (n == 0) {
                if (fin == 1) {
                    printf("[QUIT]: Successfully disconnected!\n");
                }
                
                else {
                    printf("[QUIT]: Unexpected disconnection!\n");
                }
                return;
            }
            
            else if (n < 0) {
                error_handling("recv() error");
            }


            if (msg.header.type == INIT_USER) {
                init_usr(sockfd, &usr, &msg);
            }
            else if (msg.header.type == MSG) {
                printf("(All)[%s]: %s\n", msg.header.src_name, msg.buf);
            }
            else if (msg.header.type == WHISPER) {
                printf("(Whisper)[%s]: %s\n", msg.header.src_name, msg.buf);
            }
            else if (msg.header.type == LIST) {
                printf("[Client List]\n%s\n", msg.buf);
            }
        }


        if (FD_ISSET(fileno(stdin), &rset)) {
            memset(&msg, 0, sizeof(msg));
            if (fgets(msg.buf, BUF_SIZE, stdin) == NULL)
                break;

            msg.buf[strcspn(msg.buf, "\n")] = '\0';

            
            if (strncmp(msg.buf, "/w", 2) == 0) {
                msg.header.type = WHISPER;
                whisper(&msg, &usr);
            }

            else if (strncmp(msg.buf, "/list", 5) == 0) {
                msg.header.type = LIST;
                strcpy(msg.header.src_name, usr.name);
                strcpy(msg.buf, "/list");
            }

            else if (strncmp(msg.buf, "/setting", 8) == 0) {
                setting(&usr);
                continue;
            }

            else if (strncmp(msg.buf, "/me", 3) == 0) {
                printf("[ME]: %s\n", usr.name);
                continue;
            }

            else if (strncmp(msg.buf, "/help", 5) == 0) {
                help();
                continue;
            }

            else if (strncmp(msg.buf, "/quit", 5) == 0) {
                printf("[QUIT]: Closing connection to the server...\n");
                shutdown(sockfd, SHUT_WR);
                FD_CLR(fileno(stdin), &rset);
                fin = 1;
                continue;
            }

            else {
                msg.header.type = MSG;
                strcpy(msg.header.src_name, usr.name);
            }

            if (send(sockfd, &msg, sizeof(msg), 0) == -1) {
                error_handling("send() error");
            }
        }
    }
}

void help() {
    printf("[usage]\n \
    1. (message) : Send a message to all users\n \
    2. /w target message : Whisper a message to a target user\n \
    3. /me : Check your current username\n \
    4. /list : List all connected users\n \
    5. /setting : Change your settings (e.g., change your username)\n \
    6. /quit : Exit the program\n\n");
}

void init_usr(int sockfd, struct User *usr, struct Message *msg) {
    if (msg->header.type == MAX_CLIENT_ERROR) {
        error_handling("too many clients..");
    }
    memset(usr, 0, sizeof(*usr));
    printf("input your name: ");
    scanf("%s", usr->name);
    getchar();
    strcpy(msg->buf, usr->name);
    msg->header.type = INIT_USER;

    if (send(sockfd, msg, sizeof(*msg), 0) == -1) {
        error_handling("send() error");
    }
}

void whisper(struct Message *msg, struct User *usr) {
    char dest_name[NAME_SIZE];

    sscanf(msg->buf, "/w %s %[^\n]", dest_name, msg->buf);
    strcpy(msg->header.dest_name, dest_name);
    strcpy(msg->header.src_name, usr->name);
}

void setting(struct User *usr) {
    printf("[SETTING]: %s\n1. change my name\n2. exit\ninput: ",usr->name);
    int select = 0;
    scanf("%d", &select);
    getchar();
    
    if (select == 1) {
        printf("[SETTING]: input new name: ");
        scanf("%s", usr->name);
        getchar();
        return;
    }
    else if (select == 2) {
        printf("[SETTING]: exit.\n");
        return;
    }
    else {
        printf("[SETTING]: wrong input.\n");
        return;
    }
}