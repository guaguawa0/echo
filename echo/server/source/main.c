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
#include <linux/netlink.h>

static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const char* SERVER_PID_FILE = "/dev/myEcho/serpid";
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
    char rbuf[1000];
    struct sockaddr_nl cur;
    struct msghdr msg;
    struct iovec iov;

    if (args != 2) {
        PrintHelp();
        return 0;
    }
    if (CheckRunEnv() == FALSE) {
        return 0;
    }
    ntsk = socket(PF_NETLINK, SOCK_RAW, 0);
    if (ntsk < 0) {
        printf (" why? == %d ==", errno);
        return 0;
    }
    daemon(0, 0);
    if (SavePid() == -1) {
	printf("Save Service Err\n");
	return 0;
    }

    iov.iov_base = (void*)rbuf;
    iov.iov_len = 1000;

    msg.msg_name = (void*)&(cur);
    msg.msg_namelen = sizeof(cur);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    memset(&cur, 0, sizeof(cur));
    cur.nl_family = PF_NETLINK;
    cur.nl_pid = getpid();
    cur.nl_groups = 0;
    if (bind(ntsk, (struct sockaddr*)&cur, sizeof(cur)) != 0) {
	 return -1;
    }
    for (;;) {
        memset(rbuf, 0, sizeof(char) * 1000);
        recvmsg(ntsk, &msg, 0);
        write(g_InfoFd, "wo mo dao le", 100);	
    }
    printf("run success\n");
    return 0;
}
