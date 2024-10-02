#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "protocol.h" // custom types




void error_handling(char *message);

int main(int argc, char* argv[]) {
    int listenfd, connfd, sockfd;  // 클라이언트 소켓 배열
    int nready, client[FD_SETSIZE];
    unsigned int clilen;
    struct sockaddr_in servaddr, cliaddr;
    fd_set rset, allset;
    struct Message msg;
    int msg_byte = 0;
    
    int i, maxfd, maxi;

    char client_ip[FD_SETSIZE][INET_ADDRSTRLEN]; // 클라이언트별 IP 주소를 저장하는 배열
    int client_port[FD_SETSIZE]; // 클라이언트별 포트 번호를 저장하는 배열
    char client_names[FD_SETSIZE][NAME_SIZE] = {0};  // 각 클라이언트의 이름을 저장


    if (argc!=2){
		printf("Usage : %s <port>\n", argv[0]);
        fflush(stdout);
		exit(1);
	}


    // 소켓 생성
    listenfd = socket(PF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        error_handling("socket() error");
    }


    // 소켓에 넣어줄 IP와 포트 정보를 servaddr 구조체에 매핑
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	servaddr.sin_port=htons(atoi(argv[1]));


    // 소켓에 IP와 포트 정보를 바인드
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        error_handling("server bind() error");
    }

    // 소켓을 listen 상태로 전환, 대기 큐 길이 5.
    // listen 함수 덕분에 사용자의 요청이 들어오면 listenfd가 ready상태가 되어 하단의 반복문에서 select문이 nready값을 반환함을 인지할 수 있음을 주의.
    if (listen(listenfd, MAX_CLNT) == -1){ // 소켓 연결요청 받아들일 수 있는 상태가 됨
		error_handling("server listen() error");
	}

    
    // 초기화
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
        // select()는 커널에 요청하여, 지정된 파일 디스크립터가 ready 상태인지 블로킹(blocking) 상태로 감시.
        // 그래서 rset = allset;와 같이 구조체 값 복사를 하고 allset이 아닌 rset을 넣어도 select(maxfd + 1, &rset, ..)에 포함되는 listenfd의 변화를 감지 가능함.
        if ((nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) { 
            error_handling("error select");
            exit(1);
        }

        // 클라이언트가 연결시도시, 클라이언트 소켓을 서버 소켓과 연결하는 부분.
        // select함수는 동기적 함수로, 해당 함수가 nready값을 반환하면 이 조건문이 실행됨.
        if (FD_ISSET(listenfd, &rset)) {
            clilen = sizeof(cliaddr);
            if ((connfd = accept(listenfd,(struct sockaddr *)&cliaddr,&clilen)) < 0) {
                error_handling("error accept");
                exit(1);
            }

            for (i = 0; i < FD_SETSIZE; i++) {
                if (client[i] < 0) {
                    client[i] = connfd;

                    // 서버에 등록 후 클라이언트 이름 설정
                    msg.header.type = INIT_USER;
                    strcpy(msg.buf, "Input your name: ");
                    send(client[i], &msg, sizeof(msg), 0);


                    // 클라이언트 IP 주소와 포트 번호 추출
                    inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip[i], sizeof(client_ip[i]));  // IP 주소 추출
                    client_port[i] = ntohs(cliaddr.sin_port);  // 포트 번호 추출

                    printf("client %d connected from %s:%d\n", i+1, client_ip[i], client_port[i]);
                    fflush(stdout);

                    break;
                }
            }

            if (i == FD_SETSIZE) {
                // 서버에 클라이언트 등록 실패
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
            // nready는 ready상태여서 처리해야할 fd의 수. 따라서 마지막에 -1을 함으로써 정상적으로 ready의 fd를 처리했음을 알림.
            if (--nready <= 0) {
                continue;
            }
        }

        // tcp 통신 제어
        for (i = 0; i <= maxi; i++) {

            if ( (sockfd = client[i]) < 0) {
                continue;
            }

            if (FD_ISSET(sockfd, &rset)) {
                int n = read(sockfd, &msg, sizeof(msg)); // 클라이언트로부터 데이터 읽기
                
                if (n == 0) { // 연결 종료
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

                    // 모든 클라이언트에게 전송 (브로드캐스트)
                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1 && client[j] != sockfd) {
                            send(client[j], &msg, sizeof(msg), 0);
                        }
                    }
                }
                
                else if (msg.header.type == WHISPER) {
                    // 귓속말 처리
                    printf("Received whisper request from client %d to %s: %s\n", i + 1, msg.header.dest_name, msg.buf);
                    // 대상 클라이언트를 찾아 메시지 전송
                    int target_found = 0;
                    
                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1 && strncmp(client_names[j], msg.header.dest_name, NAME_SIZE) == 0) {
                            // 대상 클라이언트에게 메시지 전송
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
                    // /list 요청 처리 - 클라이언트에게 IP:PORT 정보 전송
                    printf("Received list request from client %d\n", i + 1);
                    char list_buf[BUF_SIZE] = "[Connected clients]\n";
                    
                    for (int j = 0; j <= maxi; j++) {
                        if (client[j] != -1) {
                            char client_info[64];
                            if (client[j] != sockfd) { // /list를 요청한 유저가 아닌 모든 유저
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