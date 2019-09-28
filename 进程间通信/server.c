#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define  MY_FIFO_NAME    "/tmp/talk/myfifo"
#define  MSG_BUF_UNIT   128
#define  MSG_BUF_HEAD   (sizeof(char)+sizeof(int)*3)
#define  MSG_BUF_CONTENT  (MSG_BUF_UNIT-MSG_BUF_HEAD)
#define  USER_MAX_NUM  3
#define  USER_PASSWD_LEN  8
#define  TMP_FIFO_NAME_LEN   64
#define  SUCCESS_LOGIN   "successfully login!"
#define  KEEP_HEARTBEATS "keep connected"

#define  LOGIN_MSG  0x28
#define  SESSION_MSG 0x29
#define  LOGOUT_MSG 0x30
#define  KEEPBEATS_MSG 0x31

#define  INVALID_FIFO  -1
#define  SERVER_IDENTY_ID  0

#define  HEARTBEATS_TIMES  5
#define  AGING_TIMES  3

enum {
    OFFLINE,
    ONLINE,
}enLoginState;

typedef struct stMsg {
    char protocolId;
    int src;
    int dst;
    int len;
    char Content[MSG_BUF_CONTENT];
}stMsg;

typedef struct stUsrInfo {
    int usrId;
    char passwd[USER_PASSWD_LEN];
    int state;
    int fifo;
    char agingTime;
}stUsrInfo;

stUsrInfo g_usrTable[USER_MAX_NUM] = {
    1, "admin123", OFFLINE, INVALID_FIFO, 0,
    2, "test123", OFFLINE, INVALID_FIFO, 0,
    3, "root123", OFFLINE, INVALID_FIFO, 0,
};

/*if get, return index; or not, return -1*/
int find_usr_from_table(int usrId)
{
    int ret = -1;
    int i;

    for (i=0; i<USER_MAX_NUM; i++) {
        if (usrId == g_usrTable[i].usrId) {
            ret = i;
            return ret;
        }
    }
    printf("find not log user[%d] information!\r\n", usrId);
    return ret;
}

/*check usr is or not legal*/
int check_usr_legal(int index, char *pContent)
{
    if (index < 0) {
        printf("%d is not legal!\r\n", index);
        return index;
    }

    if (strncmp(pContent, g_usrTable[index].passwd, strlen(g_usrTable[index].passwd)) == 0) {
        return 0;
    }
    printf("%d login fail!\r\n", index);
    return -1;
}

void handle_heartbeats(int sig)
{
    char heart_msg[MSG_BUF_UNIT] = {0};
    stMsg *pHeartMsg = NULL;
    int index = 0;
    int ret = 0;

    if (sig == SIGALRM) {
        pHeartMsg = (stMsg*)heart_msg;
        pHeartMsg->protocolId = KEEPBEATS_MSG;
        pHeartMsg->src = 0;
        pHeartMsg->len = strlen(KEEP_HEARTBEATS);
        memcpy(pHeartMsg->Content, KEEP_HEARTBEATS, pHeartMsg->len);
        for (index=0; index <USER_MAX_NUM; index++) {
            if (g_usrTable[index].state == ONLINE) {
                pHeartMsg->dst = g_usrTable[index].usrId;
                ret = write(g_usrTable[index].fifo, heart_msg, MSG_BUF_UNIT);
                if (ret <= 0) {
                    printf("\r\n %d heart beats abnormal!\r\n",  g_usrTable[index].usrId);
                }

                if (g_usrTable[index].agingTime == 0) {
                    printf("\r\n agingTime is zero!\r\n");
                    g_usrTable[index].state = OFFLINE;
                    close(g_usrTable[index].fifo);
                    g_usrTable[index].fifo = INVALID_FIFO;
                } else {
                    g_usrTable[index].agingTime--;
                    printf("\r\n[%d]: usrId=%d, agingTime=%d!\r\n", __LINE__, index, g_usrTable[index].agingTime);
                }
            }
        }

        alarm(HEARTBEATS_TIMES);
    }
    return;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int fdCtrl = 0;
    char recv_buffer[MSG_BUF_UNIT] = {'\0'};
    char send_buffer[MSG_BUF_UNIT] = {'\0'};
    char tmpFifoName[TMP_FIFO_NAME_LEN];
    int tmpFifoFd = -1;
    int len = 0;
    stMsg *pReqMsg = NULL;
    stMsg *pRspMsg = NULL;
    int indexSrc = -1;
    int indexDst = -1;

    if (argc > 1) {
        printf("Usage: ./server \r\n");
        return 1;
    }

    if (access(MY_FIFO_NAME, F_OK) != 0) {
        ret = mkfifo(MY_FIFO_NAME, 0777);
        if (ret != 0) {
            perror("mkfifo");
            return -1;
        }
    }

    fdCtrl = open(MY_FIFO_NAME, O_RDWR);
    if (fdCtrl < 0) {
        perror("open ctrlFifo");
        return 1;
    }

    memset(recv_buffer, 0, sizeof(recv_buffer));
    /* block read ctrl fifo*/
    while (1) {
        len = read(fdCtrl, recv_buffer, sizeof(recv_buffer));
        if (len == -1) {
            perror("read ctrl fifo");
            return 1;
        } else if (len == 0) {
            continue;
        }

        /* once, read a packet*/
        pReqMsg = (stMsg*)recv_buffer;
        indexSrc = find_usr_from_table(pReqMsg->src);
        if (indexSrc < 0) {
            printf("%d is not exist!\r\n", indexSrc);
        }

        switch (pReqMsg->protocolId) {
            case LOGIN_MSG:
                if (check_usr_legal(indexSrc, pReqMsg->Content) == 0) {
                        /* however login successfully, client would receive notify msg*/
                        memset(tmpFifoName, 0, sizeof(tmpFifoName));
                        sprintf(tmpFifoName, "/tmp/talk/%d", pReqMsg->src);
                        if (access(tmpFifoName, R_OK) != 0) {
                            ret = mkfifo(tmpFifoName, 0777);
                            if (ret != 0) {
                                perror("fifo");
                                break;
                            }
                        }

                        tmpFifoFd = open(tmpFifoName, O_RDWR);
                        if (tmpFifoFd < 0) {
                            perror("open");
                            break;
                        }

                        g_usrTable[indexSrc].state = ONLINE;
                        g_usrTable[indexSrc].fifo = tmpFifoFd;
                        g_usrTable[indexSrc].agingTime = AGING_TIMES;
                        /*send login successfully msg to client*/
                        memset(send_buffer, 0, MSG_BUF_UNIT);
                        pRspMsg = (stMsg*)send_buffer;
                        pRspMsg->protocolId = LOGIN_MSG;
                        pRspMsg->src = 0;
                        pRspMsg->dst = pReqMsg->src;
                        pRspMsg->len = strlen(SUCCESS_LOGIN);
                        memcpy(pRspMsg->Content, SUCCESS_LOGIN, strlen(SUCCESS_LOGIN));
                        ret = write(tmpFifoFd, send_buffer, MSG_BUF_UNIT);
                        if (ret < 0) {
                           perror("write login msg"); 
                           break;
                        }
                        signal(SIGALRM, handle_heartbeats);
                        alarm(HEARTBEATS_TIMES);
                }
                break;
            case SESSION_MSG:
                /*get dst address index, in order to find send msg fifo*/
                indexDst = find_usr_from_table(pReqMsg->dst);
                if (g_usrTable[indexDst].state != ONLINE) {
                    /*if dst is offline, notify src*/
                    printf("%d usr state is offline!\r\n", pReqMsg->dst);
                    memset(send_buffer, 0, MSG_BUF_UNIT);
                    pRspMsg = (stMsg*)send_buffer;
                    pRspMsg->protocolId = SESSION_MSG;
                    pRspMsg->src = SERVER_IDENTY_ID;
                    pRspMsg->dst = pReqMsg->src;
                    sprintf(pRspMsg->Content, "%d state is offline.", pReqMsg->dst);
                    pRspMsg->len = strlen(pRspMsg->Content);

                    ret = write(g_usrTable[indexSrc].fifo, send_buffer, MSG_BUF_UNIT);
                    if (ret < 0) {
                        perror("write dst offline!");
                        break;
                    }
                    break;
                }
                memcpy(send_buffer, recv_buffer, MSG_BUF_UNIT);
                ret = write(g_usrTable[indexDst].fifo, send_buffer, MSG_BUF_UNIT);
                if (ret < 0) {
                    perror("write rsp msg!");
                    break;
                }
                break;
            case LOGOUT_MSG:
                if (check_usr_legal(indexSrc, pReqMsg->Content) == 0) {
                    g_usrTable[indexSrc].state = OFFLINE;
                    close(g_usrTable[indexSrc].fifo);
                    g_usrTable[indexSrc].fifo = INVALID_FIFO;
                }
                break;
            case KEEPBEATS_MSG:
                printf("\r\n recv keep beats msg from client!\r\n");
                if (g_usrTable[indexSrc].state == ONLINE) {
                    printf("\r\n[%d]: usrId=%d, agingTime=%d!\r\n", __LINE__, indexSrc, g_usrTable[indexSrc].agingTime);
                    g_usrTable[indexSrc].agingTime = AGING_TIMES;
                }
                break;
            default:
                printf("unknown msg %d packet\r\n", pReqMsg->protocolId);
                break;
        }

    }
    return 0;
}
