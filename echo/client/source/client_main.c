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
static const char* EXEC_DIR_PATH = "/dev/myEcho/";
static const char* EXEC_FILE_PATH = "/dev/myEcho/echo.if";
static const char* SERVER_PID_FILE = "/dev/myEcho/serpid";

struct sockaddr_nl local;
struct sockaddr_nl dest;
struct nlmsghdr* message = NULL;

typedef enum _BOOL {
	TRUE = 0,
	FALSE,
} BOOL;

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

void SendStrToSer(char* sendbuf, int sendLen, pid_t destpid)
{
    int skfd = 0;
    int len = 0;

    message = (struct nlmsghdr *)malloc(sizeof(struct nlmsghdr));       
    skfd = socket(PF_NETLINK, SOCK_RAW, 0);
    if(skfd < 0){
        printf("can not create a netlink socket, %d\n", skfd);
	return;
    }
    memset(&local, 0, sizeof(local));
    local.nl_family = AF_NETLINK;
    local.nl_pid = getpid();    
    local.nl_groups = 0;
    if(bind(skfd, (struct sockaddr *)&local, sizeof(local)) != 0){
        printf("bind() error\n");
        return;
    }
    memset(&dest, 0, sizeof(dest));
    dest.nl_family = AF_NETLINK;
    dest.nl_pid = destpid;
    dest.nl_groups = 0;
    memset(message, '\0', sizeof(struct nlmsghdr));
    message->nlmsg_len = NLMSG_SPACE(125);
    message->nlmsg_flags = 0;
    message->nlmsg_type = 0;
    message->nlmsg_seq = 0;
    message->nlmsg_pid = local.nl_pid;
    memcpy(NLMSG_DATA(message), sendbuf, strlen(sendbuf)-1);
    printf("send  to  kernel: %s,  send_id: %d   send_len: %d\n", \
    (char *)NLMSG_DATA(message),local.nl_pid, strlen(sendbuf)-1);
    len = sendto(skfd, message, message->nlmsg_len, 0,(struct sockaddr *)&dest, sizeof(dest));
    if(!len){
	perror("send pid:");
	exit(-1);
    }

    return;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Please use \"clinet start\" or \"client end\"\n");
        return 0;
    }
    printf("clinet start\n");
    char strbuf[10000];
    int flag = 0;
    pid_t serPid = 0;
    serPid = GetServerPid();
    printf("Get Pid is =%d=", serPid);
    if (serPid == -1) {
	printf("ERROR, Server Not Work, Exit\n");
	return 0;
    }
    while(1) {
        char a;
        int n = 0;
        flag = 0;
        memset(strbuf, 0, 10000);
        while ((a = getchar()) != '\n') {
            strbuf[n] = a;
            if (((a < 'A') || ((a > 'Z') && (a < 'a')) || (a > 'z')) && (a != ' ')) {
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
        if (strcmp(strbuf, "quit") == 0) {
            return 0;
        }
	SendStrToSer(strbuf, n, serPid);
	printf("send end, wait for rp");
	
        printf("rp is =%s=", strbuf);
    }
    return 0;
}
