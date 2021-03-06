#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/file.h>
#include <errno.h>

typedef enum _BOOL {
    TRUE = 0,
    FALSE,
} BOOL;

typedef enum _MOD {  
    TO_UP = 0,
    TO_LOW,
    NO_CHANGE,
} MOD;

static int g_mode = TO_UP;
static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const char* SERVER_PID_FILE = "/dev/myEcho/serpid";

struct sockaddr_nl local;
struct sockaddr_nl dest;
struct nlmsghdr* message = NULL;

static BOOL IsServerRunning()
{
    int tmpfd = 0;
    if(access(EXEC_DIR_PATH, F_OK) != 0) {
       printf("dir not exit\n");  
        return FALSE;
    }
    tmpfd = open(EXEC_FILE_PATH, O_RDWR | O_CREAT);
    if (tmpfd == -1) {
        printf("open file err\n");
        return FALSE;
    }
    if (flock(tmpfd, LOCK_EX | LOCK_NB) == 0) {
        close(tmpfd);
        return FALSE;
    }
    close(tmpfd);
    return TRUE;    
}

static pid_t GetServerPid()
{
    FILE* pidFile = NULL;
    pid_t out = -1;
    int ret = 0;
    if (IsServerRunning() == FALSE) {
        printf("ser not run\n");
        return out;
    }
    if (access(SERVER_PID_FILE, F_OK) != 0) {
        return out;
    }
    pidFile = fopen(SERVER_PID_FILE, "r");
    if (pidFile == NULL) {
        return out;
    }
    ret = fread(&out, sizeof(out), 1, pidFile);
    if (ret == 0) {
        fclose(pidFile);
        return -1;
    }
    fclose(pidFile);
    return out;
}

static int DealChangeMod(char* input)
{
    int mod = 0;
    if ((input[0] != ':') || (input[1] == 'q')) {
        return -1;
    }
    sscanf(input, ":set mod %d", &mod);
    switch (mod)
    {
    case TO_UP:
        g_mode = TO_UP;
        break;
    case TO_LOW:
        g_mode = TO_LOW;
        break;
    case NO_CHANGE:
        g_mode = NO_CHANGE;
        break;
    default:
        printf("unknow mod = %d, please check\n", mod);
        break;
    }
    return 0;
}

void SendStrToSer(char* sendbuf, int sendLen, char* myPid)
{
    static int isInitEd = 0;
    int skfd = 0;
    int len = 0;
    const char* server_file = "/dev/myEcho/echo_server";
    struct sockaddr_un un;
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, server_file);
    skfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(skfd < 0){
        printf("can not create a netlink socket, %d\n", skfd);
        return;
    }
    if (connect(skfd, (struct sockaddr *)&un, sizeof(un)) < 0) {
        printf("connet err, errno = %d\n", errno);
        return;
    }
    isInitEd = 1;
    send(skfd, sendbuf, sendLen, 0);
    close(skfd);
    return;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Please use \"clinet start\" or \"client end\"\n");
        return 0;
    }
    int ret = 0;
    printf("clinet start\n");
    char strbuf[10000];
    char myName[30];
    int flag = 0;
    char strpid[5] = {'\0'};
    int refd = 0;
    struct sockaddr_un local;
    pid_t serPid = 0;
    pid_t myPid = getpid();
    serPid = GetServerPid();
    if (serPid == -1) {
        printf("ERROR, Server Not Work, Exit\n");
        return 0;
    }
    printf("pid = %d\n", serPid);
    memcpy(strpid, &myPid, 4);
    sprintf(myName, "/dev/myEcho/client%d", myPid);
    refd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (refd < 0) {
        printf ("create socket err\n");
        return -1;
    }
    unlink(myName);
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, myName);
    if (bind(refd, (struct sockaddr*)&local, sizeof(local)) != 0) {
        printf("bind err\n");
        return -1;
    }
    if (listen(refd, 10) < 0) {
        printf("listen err\n");
        return -1;
    }
    while(1) {
        char a;
        int n = 0;
        int clfd = 0;
        flag = 0;
        memset(strbuf, 0, 10000);
        while ((a = getchar()) != '\n') {
            strbuf[n] = a;
            if (((a != ' ') && (a != ':') && (a != '1') && (a != '0') && (a != '2')) && ((a < 'A') || ((a > 'Z') && (a < 'a')) || (a > 'z'))) {
                printf("input err, please input \'a-z\' or \'A-Z\' or \" \", ==%c==\n", a);
                flag = 1;
                break;
            }
            n++;
        }
        strbuf[n] = '\0';
        if (flag == 1) {
            continue;
        }
        if (strcmp(strbuf, ":quit") == 0) {
            return 0;
        }
        if (DealChangeMod(strbuf) == 0) {
            continue;
        }
        memmove(&strbuf[8], strbuf, strlen(strbuf) + 1);
        memcpy(strbuf, strpid, sizeof(int));
        memcpy(&strbuf[4], &g_mode, sizeof(int));
        SendStrToSer(strbuf, n + 8, strpid);
        printf("send end, wait for rp\n");
        memset(strbuf, 0 ,sizeof(strbuf));
        clfd = accept(refd, NULL, NULL);
        if (clfd < 0) {
            printf("accept fail\n");
            return 0;
        }
        ret = recv(clfd, strbuf, 100, 0);
        printf("rp is %s\n", strbuf);
    }
    return 0;
}
