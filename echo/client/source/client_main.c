#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[])
{
    if (argc != 2) {
        printf("Please use \"clinet start\" or \"client end\"\n");
        return 0;
    }
    printf("clinet start\n");
    char strbuf[10000];
    int flag = 0;
    while(1) {
        char a;
        int n = 0;
        flag = 0;
        memset(strbuf, 0, 10000);
        while ((a = getchar()) != '\n') {
            strbuf[n] == a;
            if ((((int)a < 65) || (((int)a > 90) && ((int)a < 97)) || ((int)a > 122)) || (a != ' ')) {
                printf("input err, please input \'a-z\' or \'A-Z\' or \" \"");
                flag = 1;
                break;
            }
        }
        if (flag == 1) {
            continue;
        }
        if (strcmp(strbuf, "quit") == 0) {
            return 0;
        }
        printf("\n your input str is %s\n", strbuf);
    }
    return 0;
}
