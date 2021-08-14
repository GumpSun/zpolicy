#include "policyWatcher.h"
#include "utility.h"

//#define LENGTH 1024

char *replace(char * src,char oldChar,char newChar)
{
    char * head=src;
    while(*src!='\0')
    {
        if(*src==oldChar) *src=newChar;
            src++;
    }
    return head;
}

void stringtrim(char *str)  
{  
    int len = strlen(str);  
    if(str[len-1] == '\n' || str[len-1] == ' ')  
    {  
        len--; 
        str[len] = 0;  
    }  
}

void cleanFile(char *path)
{
    int fd;
    fd = open(path, O_RDWR);
    if(fd < 0)
    {
        printf("open %s failed\n", path);
    }
    else
    {
        printf("open clean %s successful\n", path);
        ftruncate(fd,0);
        lseek(fd,0,SEEK_SET);
        close(fd);
    }
}

void writeFile(char *path, char* content)
{
    FILE *fd = fopen(path, "a+");
    if(fd != NULL)
    {
        fprintf(fd, "%s", content);
    }
    else
    {
        return;
    }
    fclose(fd);
}

void traceEvent(char *content, const char *name, char *type)
{
    char msg[LENGTH], path[LENGTH];
    char ti[32];
    time_t now = time(NULL);

    sprintf(path, "/var/log/ad-policy/watcher-%d-%d-%d", localtime(&now)->tm_year + 1900, localtime(&now)->tm_mon + 1, localtime(&now)->tm_mday);

    FILE *fp = fopen(path, "a+");
    sprintf(msg, "%s-%s", content, name);
    strftime(ti, sizeof(ti), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(fp, "[%s] [%s] %s\n", ti, type, msg);
    fflush(fp);
    fclose(fp);
}

void check_is_running(char *process)
{
	FILE *fp;
	unsigned char *p;	
	char command[256];
	int is_runing_flag = 0;
	
	memset(command, 0, 256);
	sprintf(command, "ps acx|grep policyWatcher|wc -l");
	
	fp = popen(command, "r");
	if (fp == NULL)
	{
		perror("popen error");
		exit (0);
	}
	fscanf(fp, "%lu", &is_runing_flag);
	if(is_runing_flag >= 2)
	{
		printf("There is a %s is running, and only one can run,so we exit\n",process);
		traceEvent("policywatcher is running, and only one can run,so we exit", process, "WARN");
		exit (0);
	}
	pclose(fp);	
	//printf("is_runing_flag = %d\n", is_runing_flag);
} 
