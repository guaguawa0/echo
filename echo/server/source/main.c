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

static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const char* SERVER_PID_FILE = "/dev/myEcho/serpid";
const char* SERVER_DOMAIN_NAME = "/dev/myEcho/echo_server";
static int g_InfoFd = 0;
static int g_uniqueLock = 0;

typedef enum _BOOL {
    TRUE = 0,
    FALSE,
} BOOL;

static inline void PrintHelp()
{
    printf("Please Use \"Server start\" or \"Server stop\"\n");
}

/* 该程序第一次执行 */
static inline BOOL CreateInfoFile()
{
    int ret = 0;
    if(access(EXEC_DIR_PATH, F_OK) != 0) {      
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

int main(int args, char* argv[])
{
    int ntsk = 0;
    int clfd = 0;
    int ret = 0;
    struct sockaddr_un local;
    struct msghdr msg;
    struct iovec iov;
    if (args != 2) {
        PrintHelp();
        return 0;
    }
    if (CheckRunEnv() == FALSE) {
        return 0;
    }
    ntsk = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ntsk < 0) {
        printf (" why? == %d ==", errno);
        return 0;
    }
    /* daemon(0, 0); */
    if (SavePid() == -1) {
        printf("Save Service Err\n");
        return 0;
    }
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
        struct sockaddr_un client_addr;
	char rbuf[100];
        memset(rbuf, 0, sizeof(char) * 100);
	clfd = accept(ntsk, NULL, NULL);
        if (clfd < 0) {
            printf("accept fail\n");
	    return -1;
	}
        ret = recv(clfd, rbuf, 100, 0);
	if (ret < 0) {
            printf("recv failed\n");
	}
        printf("===%s===\n", rbuf);
        close(clfd);
	break;
    }
    close(ntsk);
    printf("run success\n");
    return 0;
}
