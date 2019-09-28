#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define MSG_BUF_LEN  128
#define FIFO_NAME_MAX_LEN  32
#define RECV_FIFO_FORMAT "/tmp/talk/%d"
#define SEND_FIFO_NAME "/tmp/talk/myfifo"
#define MSG_BUF_HEAD (sizeof(char)+sizeof(int)*3)
#define MSG_BUF_CONTENT (MSG_BUF_LEN-MSG_BUF_HEAD)

#define lOGIN_MSG  0x28
#define SESSION_MSG 0x29
#define LOGOUT_MSG 0x30
#define KEEPBEATS_MSG 0x31

#define SUCCESS_LOGIN_RSP  "successfully login!"
#define KEEP_HEARTBEATS "keep connected"
#define LOGIN_TIMEOUT 20

typedef struct stMsg {
    char protocolId;
    int src;
    int dst;
    int len;
    char Content[MSG_BUF_CONTENT];
}stMsg;

int set_io_state(int fd, int mode)
{
    int current_mode = 0;
    int ret = 0;

    current_mode = fcntl(fd, F_GETFL, 0);
    if (current_mode < 0) {
        printf("fcntl get flag %d\r\n", fd);
        return -1;
    }

    ret = fcntl(fd, F_SETFL, current_mode | mode);
    if (ret < 0) {
        printf("fcntl set flag %d\r\n", fd);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int size = 0;
    char my_private_fifo[FIFO_NAME_MAX_LEN] = {0};
    char in_buffer[MSG_BUF_LEN] = {0};
    char recv_buffer[MSG_BUF_LEN] = {0};
    char send_buffer[MSG_BUF_LEN] = {0};
    int send_fd;
    int private_fd;
    int in_fd;
    int mode = 0;
    const char sep[2] = "-";
    char *token = NULL;
    stMsg *pRecvMsg = NULL;
    stMsg *pSendMsg = NULL;
    int loginTimeOut = 0;

    if (argc < 2) {
        printf("Usage: ./client [fifo_id]\r\n");
        return -1;
    }

    /*init recv fifo name*/
    sprintf(my_private_fifo, RECV_FIFO_FORMAT, atoi(argv[1]));

    /*judge server master fifo if exist*/
    ret = access(SEND_FIFO_NAME, F_OK);
    if (ret != 0) {
        printf("\r\nserver is not started!");
        return -1;
    }

    send_fd = open(SEND_FIFO_NAME, O_RDWR);
    if (send_fd < 0) {
        perror("open");
        return -1;
    }

    in_fd = open("/dev/tty", O_RDWR);
    if (in_fd < 0) {
        printf("[%d]: open /dev/tty fail", __LINE__);
        return -1;
    }

    /*input io state*/
    ret = set_io_state(in_fd, O_NONBLOCK);
    if (ret < 0) {
        return ret;
    }

    /*first, connect and login server*/
    memset(send_buffer, 0, sizeof(send_buffer));
    pSendMsg = (stMsg*)send_buffer;
    pSendMsg->protocolId = lOGIN_MSG;
    pSendMsg->src = 1;
    /*tmp, server address set 0*/
    pSendMsg->dst = 0;
    pSendMsg->len = strlen("admin123");
    memcpy(pSendMsg->Content, "admin123",  strlen("admin123"));
    size = write(send_fd, send_buffer, MSG_BUF_LEN);
    if (size < 0) {
        perror("write");
        return -1;
    }
    /*block read rsp what login successfuly msg from server*/
    while (1) {
        if (loginTimeOut++ > LOGIN_TIMEOUT) {
            printf("\r\n login time out!");
            return -1;
        }

        ret = access(my_private_fifo, F_OK);
        if (ret == 0) {
            printf("\r\n server connected client !");
            break;
        }
        sleep(1);
    }
    private_fd = open(my_private_fifo, O_RDWR);
    if (private_fd < 0) {
        printf("\r\n open private fd fail!");
        return -1;
    }
    unlink(my_private_fifo);

    size = read(private_fd, recv_buffer, sizeof(recv_buffer));
    if (size <= 0) {
        printf("\r\n client auto login timeout, please try login again!");
        return -1;
    }
    pRecvMsg = (stMsg*)recv_buffer;
    ret = strcmp(pRecvMsg->Content, SUCCESS_LOGIN_RSP);
    if (ret != 0) {
        printf("\r\n server reject client login, please check it config!");
        return -1;
    }

    printf("\r\n client login successfully, please start talk each other!\r\n");

    /*input io state*/
    ret = set_io_state(in_fd, O_NONBLOCK);

    /*set recv fifo is nonblock*/
    ret |= set_io_state(private_fd, O_NONBLOCK);
    if (ret < 0) {
        return -1;
    }

    /*second, accept from stardinput msg*/
    while (1) {
        memset(in_buffer, 0, sizeof(in_buffer));
        size = read(in_fd, in_buffer, sizeof(in_buffer));
        if (size > 0) {
            memset(send_buffer, 0, sizeof(send_buffer));
            pSendMsg = (stMsg*)send_buffer;
            pSendMsg->protocolId = SESSION_MSG;
            pSendMsg->src = atoi(argv[1]);
            //copy src address to send buffer
            token = strtok(in_buffer, sep);
            pSendMsg->dst = atoi(token);
            //tmp, not support : character
            token = strtok(NULL, sep);
            pSendMsg->len = strlen(token);
            memcpy(pSendMsg->Content, token, pSendMsg->len);
            //send request to server
            ret = write(send_fd, pSendMsg, MSG_BUF_LEN);
            if (ret <= 0) {
                printf("\r\n The request that send to server fail, the reason %u!", ret);
                return -1;
            }
        } else if (size < 0) {
            if (errno == EAGAIN) {
                goto recvrsp;
            }
            printf("\r\n read private_fd");
            return -1;
        }

recvrsp:
        //recv rsp from server
        memset(recv_buffer, 0, sizeof(recv_buffer));
        ret = read(private_fd, recv_buffer, sizeof(recv_buffer));
        if (ret < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            return -1;
        } else if (ret > 0) {
            pRecvMsg = (stMsg*)recv_buffer;
            if (pRecvMsg->protocolId == KEEPBEATS_MSG) {
                printf("\r\n recv keep beats msg from server!\r\n");
                memset(send_buffer, 0, sizeof(send_buffer));
                pSendMsg = (stMsg*)send_buffer;
                pSendMsg->protocolId = KEEPBEATS_MSG;
                pSendMsg->src = atoi(argv[1]);
                pSendMsg->dst = 0;
                pSendMsg->len = strlen(KEEP_HEARTBEATS);
                memcpy(pSendMsg->Content, KEEP_HEARTBEATS, pSendMsg->len);
                ret = write(send_fd, send_buffer, sizeof(send_buffer));
                if (ret <= 0) {
                    printf("send heart beats fail!\r\n");
                    return -1;
                }
            }
            printf("\r\n client recv server rsp!\r\n");
        }
    }

    close(send_fd);
    close(in_fd);
    close(private_fd);
    return 0;
}
