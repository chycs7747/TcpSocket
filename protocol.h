#ifndef PROTOCOL_H
#define PROTOCOL_H

#define BUF_SIZE 1024
#define MAX_CLNT 5  // 최대 5명의 클라이언트 처리
#define NAME_SIZE 8 // 8-byte

#define WAITING_FOR_NAME 0  // 클라이언트가 이름을 입력 중인 상태
#define READY_FOR_MSG 1     // 클라이언트가 메시지를 주고받을 준비가 된 상태

#define min(x, y) (x) < (y) ? (x) : (y)
#define max(x, y) (x) > (y) ? (x) : (y)

enum {
    // server communicate
    INIT_USER,
    MSG,
    WHISPER,
    LIST,
    // self client communicate
    HELP,
    SETTING,
    ME,
    // error type
    SOCKET_ERROR,
    CONN_ERROR,
    RECV_ERROR,
    SEND_ERROR,
    MAX_CLIENT_ERROR
};

struct User {
    char name[NAME_SIZE];
};

struct Header {
    char src_name[NAME_SIZE];
    char dest_name[NAME_SIZE];
    int chatroom;
    int type;
};

struct Message {
    struct Header header;
    char buf[BUF_SIZE];
};

#endif /* PROTOCOL_H */