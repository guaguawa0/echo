#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
typedef void (*DEAL)(char*);
static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const char* SERVER_PID_FILE = "/dev/myEcho/serpid";
const char* SERVER_DOMAIN_NAME = "/dev/myEcho/echo_server";
const char* SERVER_DEAMON_NAME = "/dev/myEcho/echo_demon";
static int g_InfoFd = 0;
static int g_uniqueLock = 0;
struct ClientNode {
    pid_t pid;
    int value;
    struct ClientNode* next;
};

typedef enum _BOOL {
    TRUE = 0,
    FALSE,
} BOOL;

void Transfrom2Up(char* in)
{
    int i = 0;
    for (i = 0; in[i] != '\0'; i++) {
        if ((in[i] >= 'a') && (in[i] <= 'z')) {
            in[i] -= 32;
        }
    }
    return;
}

void Transfrom2Low(char* in)
{
    int i = 0;
    for (i = 0; in[i] != '\0'; i++) {
        if ((in[i] >= 'A') && (in[i] <= 'Z')) {
            in[i] += 32;
        }
    }
    return;
}

void NoChange(char* in)
{
    return;
}

DEAL deal[3] = {Transfrom2Up, Transfrom2Low, NoChange};

static void Sleep_Ms(unsigned int secs)
{
    struct timeval tval;
    tval.tv_sec = secs / 1000;
    tval.tv_usec = (secs * 1000) % 1000000;
    select(0, NULL, NULL, NULL, &tval);
}

static inline void PrintHelp()
{
    printf("Please Use \"Server start\" or \"Server stop\"\n");
}

/* 该程序第一次执行 */
static inline BOOL CreateInfoFile()
{
    int ret = 0;
    if (access(EXEC_DIR_PATH, F_OK) != 0) {      
        mkdir(EXEC_DIR_PATH, 0777);      
    }
    g_InfoFd = open(EXEC_FILE_PATH, O_RDWR | O_CREAT);
    if (g_InfoFd == -1) {
        return FALSE;
    }
    ret = write(g_InfoFd, "kai shi le", 10);
    return TRUE;
}

/* 通过文件锁检查是不是独一份的服务器，如果是的话会清空记录文件 */
static inline BOOL IsUniqueServer()
{
    g_InfoFd = open(EXEC_FILE_PATH, O_RDWR | O_CREAT);
    if (g_InfoFd == -1) {
        return FALSE;
    }
    if (flock(g_InfoFd, LOCK_EX | LOCK_NB) != 0) {
        printf("server running!!\n");
        return FALSE;
    }
    return TRUE;
}

static inline BOOL CheckRunEnv()
{
    if (access(EXEC_FILE_PATH, 0) != 0) {
        return CreateInfoFile();
    } else {
        return IsUniqueServer();
    }
}

static int SavePid()
{
    FILE* pidFile = NULL;
    pid_t myPid = 0;
    int ret = 0;
    pidFile = fopen(SERVER_PID_FILE, "w");
    if (pidFile == NULL) {
        return -1;
    }
    myPid = getpid();
    ret = fwrite(&myPid, sizeof(myPid), 1, pidFile);
    if (ret <= 0) {
        fclose(pidFile);
        return -1;
    }
    fclose(pidFile);
    return 0;
}

static void SendStr2Clinet(int sid, char* str)
{
    struct sockaddr_un desk;
    int dk = 0;
    int ret = 0;
    char dest[30];
    (void)sprintf(dest, "/dev/myEcho/client%d", sid);
    dk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dk < 0) {
        printf("creat rp socket error, errno = %d", errno);
        return;
    }
    desk.sun_family = AF_UNIX;
    strcpy(desk.sun_path, dest);
    if (connect(dk, (struct sockaddr *)&desk, sizeof(desk)) < 0) {
        printf("connect to rp err, errno = %d", errno);
        close(dk);
        return;
    }
    ret = send(dk, str, strlen(str) + 1, 0);
    printf("send info = %d\n ", ret);
    close(dk);
    return;
}

static BOOL CheckProcessLive(pid_t pid)
{
    char* name[100];
    sprintf(name, "/proc/%d", pid);
    if (access(name, F_OK) != 0) {
        return FALSE;
    }
    return TRUE;
}

static void DropOneNode(struct ClientNode* last, struct ClientNode* tmp)
{
    last->next = tmp->next;
    free(tmp);
    tmp = last;
}

static BOOL QueryOneNode(struct ClientNode* head, pid_t pid)
{
    char rp[30];
    struct ClientNode* tmp = head;
    for (; tmp != NULL; tmp = tmp->next) {
        if (tmp->pid == pid) {
            sprintf(rp, "%d", tmp->value);
            SendStr2Clinet(pid, rp);
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL AddOneNode(struct ClientNode* head, pid_t pid, int value)
{
    struct ClientNode* tmp = head->next;
    for (; tmp->next != NULL; tmp = tmp->next) {
        if (tmp->pid == pid) {
            tmp->value += value;
            return TRUE;
        }
    }
    if (tmp->pid == pid) {
        tmp->value += value;
        return TRUE;
    } else {
        struct ClientNode* tmp2 = (struct ClientNode*)malloc(sizeof(struct ClientNode));
        if (tmp2 == NULL) {
            return FALSE;
        }
        tmp->next = tmp2;
        tmp2->pid = pid;
        tmp2->value = value;
        tmp2->next = NULL;
    }
    ClientStatistics(head);
    return TRUE;
}

static void ClientStatistics(struct ClientNode* head)
{
    if ((head == NULL) || (head->next == NULL)) {
        return;
    }
    struct ClientNode* tmp = head->next;
    struct ClientNode* last = head;
    for (; tmp != NULL; last = tmp, tmp = tmp->next) {
        if(CheckProcessLive(tmp->pid) == FALSE) {
            DropOneNode(last, tmp);
        }
    }
}

static void AddToClientList(pid_t pid, int value)
{
    struct sockaddr_un desk;
    int dk = 0;
    int ret = 0;
    int info[3];
    info[0] = 0;
    info[1] = (int)pid;
    info[2] = value;
    dk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (dk < 0) {
        printf("creat rp socket error, errno = %d", errno);
        return;
    }
    desk.sun_family = AF_UNIX;
    strcpy(desk.sun_path, SERVER_DEAMON_NAME);
    if (connect(dk, (struct sockaddr *)&desk, sizeof(desk)) < 0) {
        printf("connect to rp err, errno = %d", errno);
        close(dk);
        return;
    }
    ret = send(dk, info, sizeof(int) * 3, 0);
    printf("send info = %d\n ", ret);
    close(dk);
    return;
}

static int shouhu(pid_t pid)
{
    struct sockaddr_un local;
    int ret = 0;
    int shsk = 0;
    int refd = 0;
    int buf[3];
    struct timeval tval;
    fd_set rdfds;
    struct ClientNode* head = (struct ClientNode*)malloc(sizeof(struct ClientNode));
    if (head == NULL) {
        return -1;
    }
    head->pid = pid;
    head->value = -1;

    shsk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (shsk < 0) {
        printf ("Create socket err, errno = %d\n", errno);
        return 0;
    }
    unlink(SERVER_DEAMON_NAME);
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SERVER_DEAMON_NAME);
    if (bind(shsk, (struct sockaddr*)&local, sizeof(local)) != 0) {
        printf("bind error\n");
        return -1;
    }
    if (listen(shsk, 10) < 0) {
        printf("listen error\n");
        return -1;
    }
    tval.tv_sec = 5;
    tval.tv_usec = 0;

    while (1) {
        FD_ZERO(&rdfds);
        FD_SET(shsk, &rdfds);
        ret = select(1, &rdfds, NULL, NULL, &tval);
        if (ret < 0) {
            printf("select err = %d", errno);
            return -1;
        } else if (ret != 0) {
            refd = accept(shsk, NULL, NULL);
            if (shsk < 0) {
                printf("accept fail\n");
                return -1;
            }
            ret = recv(refd, buf, 3, 0);
            if (ret < 0) {
                printf("recv failed\n");
            }
            if (buf[0] == 0) {
                (void)AddOneNode(head, buf[1], buf[2]);
            } else {
                QueryOneNode(head, buf[1]);
            }
        } else {
            ClientStatistics(head);
        }
    }

}

int main(int args, char* argv[])
{
    int ntsk = 0;
    int clfd = 0;
    int ret = 0;
    struct sockaddr_un local;
    struct msghdr msg;
    struct iovec iov;
    pid_t mypid = 0;
    if (args != 2) {
        PrintHelp();
        return 0;
    }
    if (CheckRunEnv() == FALSE) {
        return 0;
    }
    ntsk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ntsk < 0) {
        printf ("Create socket err, errno = %d\n", errno);
        return 0;
    }
    daemon(0, 0);
    if (SavePid() == -1) {
        printf("Save Service Err\n");
        return 0;
    }
    mypid = getpid();
    signal(SIGCLD, SIG_IGN);
    shouhu(mypid);
    unlink(SERVER_DOMAIN_NAME);
    local.sun_family = AF_UNIX;
    strcpy(local.sun_path, SERVER_DOMAIN_NAME);
    if (bind(ntsk, (struct sockaddr*)&local, sizeof(local)) != 0) {
        return -1;
    }
    printf("bind success\n");
    if (listen(ntsk, 10) < 0) {
        return -1;
    }
    printf("listen success\n");
    while(1) {
        int clientid = 0;
        struct sockaddr_un client_addr;
        char rbuf[1000];
        int mod = 0;
        pid_t tmpPid = 0;
        memset(rbuf, 0, sizeof(char) * 1000);
        clfd = accept(ntsk, NULL, NULL);
        if (clfd < 0) {
            printf("accept fail\n");
            return -1;
        }
        ret = recv(clfd, rbuf, 1000, 0);
        if (ret < 0) {
            printf("recv failed\n");
        }
        memcpy(&clientid, rbuf, sizeof(int));
        memcpy(&mod, rbuf + sizeof(int), sizeof(int));
        printf("===%s===%d\n", (rbuf + sizeof(int) * 2), clientid);
        deal[mod]((rbuf + sizeof(int) * 2));
        tmpPid = fork();
        if (tmpPid < 0) {
            printf("fork err, errno = %d", errno);
            return -1;
        } else if (tmpPid == 0) {
            Sleep_Ms(200);
            SendStr2Clinet(clientid, (rbuf + sizeof(int) * 2));
            AddToClientList(clientid, strlen(rbuf + sizeof(int) * 2));
            close(clfd);
            return 0;
        } else {
            close(clfd);
        }
    }
    close(ntsk);
    printf("run success\n");
    return 0;
}
