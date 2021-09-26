#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const int g_InfoFd = 0;

enum BOOL {
    TRUE = 0,
    FALSE,
};

inline void PrintHelp()
{
    printf("Please Use \"Server start\" or \"Server stop\"\n");
}

/* 该程序第一次执行 */
inline BOOL CreateInfoFile()
{
    g_InfoFd = open(EXEC_FILE_PATH, O_RDWR | O_CREAT);
    if (g_InfoFd == -1) {
        return FALSE;
    }
    return TRUE;
}

/* 通过文件锁检查是不是独一份的服务器，如果是的话会清空记录文件 */
inline BOOL IsUniqueServer()
{
    if (access(EXEC_FILE_PATH, 0) == 0) {

    }
    return TRUE;
}

inline BOOL CheckRunEnv()
{
    if (access(EXEC_FILE_PATH, 0) == 0) {
        return CreateInfoFile();
    } else {
        return IsUniqueServer();
    }
}

int main(int args, char* argv[])
{
    if (args != 3) {
        PrintHelp();
        return 0;
    }
    if (CheckRunEnv() == FALSE) {
        printf("run error");
        return 0;
    }
    printf("run success");
    return 0;
}
