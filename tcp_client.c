#include <stdio.h>
#include <signal.h> // signal
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <unistd.h> // close
#include <sys/types.h>
#include <sys/socket.h> // sockaddr
#include <arpa/inet.h> // inet_addr
#include "protocol.h" // custom types



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
   

    // 1. create client socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1){
		error_handling("client socket() error");
	}

    // 2. connect client socket
    // 참고: 클라이언트가 서버에 연결할 때 중요한 것은 서버의 IP 주소와 포트 번호이지, 로컬의 포트 번호가 아니다.
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_addr.s_addr=inet_addr(argv[1]); // IPv4 주소 입력 → 32bit 숫자(네트워크 바이트 정렬)로 리턴
	cliaddr.sin_port=htons(atoi(argv[2])); // 네트워크 바이트 정렬
    memset(&(cliaddr.sin_zero), '\0', 8); // 구조체 나머지 부분을 0으로 초기화

    // 소켓을 IP와 포트에 바인딩 (오류 체크 필요)
    if(connect(sockfd, (struct sockaddr*)&cliaddr, sizeof(cliaddr))==-1) {
        error_handling("client connect() error");
    }
    
    // 메시지 송수신 함수 호출
    communicate(sockfd);

    // 연결을 닫고 프로그램 종료
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
    int str_len;
    

    FD_ZERO(&rset);
	FD_ZERO(&allset);
    FD_SET(sockfd, &allset);
    FD_SET(fileno(stdin), &allset);
    maxfd = max(fileno(stdin), sockfd);



    // [상호작용 시작]
    while(1) {
        memset(&(msg), 0, sizeof(msg));
        rset = allset;

        if ((nready = select(maxfd + 1, &rset, NULL, NULL, NULL)) < 0) { 
            error_handling("error select");
            exit(1);
        }

        // 서버로부터 메시지를 받는 경우
        if (FD_ISSET(sockfd, &rset)) {
            memset(&msg, 0, sizeof(msg));
            str_len = recv(sockfd, &msg, sizeof(msg), 0);
            if (str_len == -1) {
                error_handling("recv() error");
            }

            // 받은 메시지 출력
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
                // /list 명령에 대한 응답 처리: 서버에서 받은 클라이언트 목록 출력
                printf("[Client List]\n%s\n", msg.buf);
            }
        }

        // 클라이언트가 메시지를 입력하는 경우
        if (FD_ISSET(fileno(stdin), &rset)) {
            memset(&msg, 0, sizeof(msg));
            if (fgets(msg.buf, BUF_SIZE, stdin) == NULL)
                break;

            // 개행 문자 제거
            msg.buf[strcspn(msg.buf, "\n")] = '\0';  // 개행 문자가 있으면 제거

            
            // 귓속말 여부 확인 ("/whisper 대상이름 메시지" 형식)
            if (strncmp(msg.buf, "/w", 2) == 0) {
                msg.header.type = WHISPER;
                whisper(&msg, &usr);  // whisper 함수 호출
            }

            // 접속 중인 유저 확인
            else if (strncmp(msg.buf, "/list", 5) == 0) {
                msg.header.type = LIST;
                strcpy(msg.header.src_name, usr.name); // (me) 표시용
                strcpy(msg.buf, "/list");
            }

            // self communication
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

            else {
                msg.header.type = MSG; // default
                strcpy(msg.header.src_name, usr.name); // 자기 이름 표시용
            }

            // 서버로 메시지 전송
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
    5. /setting : Change your settings (e.g., change your username)\n\n");
}

// init_usr 함수 구현
void init_usr(int sockfd, struct User *usr, struct Message *msg) {
    // 초기 사용자 이름 설정하기 위해 호출 (서버가 클라이언트 요청받은 후 서버의 요청)
    if (msg->header.type == MAX_CLIENT_ERROR) {
        error_handling("too many clients..");
    }
    memset(usr, 0, sizeof(*usr));
    printf("input your name: ");
    scanf("%s", usr->name);
    getchar();
    strcpy(msg->buf, usr->name);
    msg->header.type = INIT_USER;

    // 서버로 메시지 전송
    if (send(sockfd, msg, sizeof(*msg), 0) == -1) {
        error_handling("send() error");
    }
}


// whisper 함수 구현
void whisper(struct Message *msg, struct User *usr) {
    char dest_name[NAME_SIZE];  // 귓속말 대상자 이름 저장용

    // 귓속말 대상자와 메시지 추출
    sscanf(msg->buf, "/w %s %[^\n]", dest_name, msg->buf);

    // 대상 이름을 메시지 헤더에 저장
    strcpy(msg->header.dest_name, dest_name);
    
    // 자신의 이름을 메세지 헤더에 저장
    strcpy(msg->header.src_name, usr->name);
}

// setting 함수 구현
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