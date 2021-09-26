#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
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

int main(int args, char* argv[])
{
    if (args != 2) {
        PrintHelp();
        return 0;
    }
    if (CheckRunEnv() == FALSE) {
        printf("run error\n");
        return 0;
    }
    for (;;) {
    }
    printf("run success\n");
    return 0;
}
