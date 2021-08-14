#include "policyWatcher.h"

struct mylist groupList;
struct String_vector injectrouteList;

int initStatus = 0, curStatus = 0;
static int mycount = 0, num = 0;

int32_t current_version;
zhandle_t* zkhandle;

char myInjectRoute[LENGTH] = "", myInjectInterface[LENGTH] = "";
char myMac[LENGTH] = "", myStatus[LENGTH] = "";

char groupThreshold[LENGTH] = "", injectNexthop[LENGTH] = "";

static char hostName[LENGTH] = "";
static char myid[128] = "", localIP[128] = "";
char* groupThresholdStr;

static int loopRun = 1;

static int connected = 0;
static int expired = 0;
char zkEnv[LENGTH] = "";

int count = 0, count1 = 0;
int checkOutLimit = 0;
//int groupPolicyNum = 0;

static int32_t  globalVersion = 0;
static int32_t  macVersion = 0;
static int32_t  interfaceVersion = 0;

pthread_mutex_t group_mutex;
pthread_mutex_t injectroute_mutex;

//struct timeval t_start, t_end;

void get_zookeeper_env()
{
    sprintf(zkEnv ,"%s", getenv("ZOOKEEPER_HOME"));
    printf("zookeeper path = %s\n", zkEnv);

}

void main_watcher (zhandle_t *zkh, int type, int state, const char *path, void* context)
{
    if (type == ZOO_SESSION_EVENT) 
    {
        if (state == ZOO_CONNECTED_STATE) 
        {
            connected = 1;
            printf("connected...\n");
        } 
        else if (state == ZOO_CONNECTING_STATE) 
        {
            if(connected == 1) 
            {
                  traceEvent("Connecting zookeeper ", "disconnected", "ERROR");
                  printf("connecting...\n");
            }
            connected = 0;
        } 
        else if (state == ZOO_EXPIRED_SESSION_STATE) 
        {
            expired = 1;
            connected = 0;
            printf("connect expired...\n");
            traceEvent("Connect expired ", "session expired", "WARN");
            //zookeeper_close(zkh);
            //zkhandle = zookeeper_init("localhost:2181", main_watcher, 300000, 0, "hello zookeeper.", 0);
            //watchGetThread();
            //zookeeper_close(zkh);
        }
    }
}

void zkgroupolicy_watch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    char policyFile[LENGTH] = "";
    char policyData[DATA_LENGTH] = "";
    int dataLen = sizeof(policyData);
    struct String_vector curStrings;	

    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch group policy create", path, "INFO");
    }
    if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch group policy delete", path, "INFO");
    }
    if(type == ZOO_CHANGED_EVENT)
    {
        int i;
        struct Stat stat;
        zoo_wget(zkhandle, path, zkgroupolicy_watch, "watch_policy", policyData, &dataLen, &stat);	
        traceEvent("Watch group policy changed", path, "INFO");
        char *ptr = strtok((char*)path, "/group/policy");

        for(i=0;i<groupList.count;i++)
        {
            if(!strcmp(groupList.data[i], ptr))
            {
                groupList.mzxid[i] = stat.mzxid;
            }   	
        }
        parsePolicy(policyData);
    }
    if(type == ZOO_CHILD_EVENT)
    {
        traceEvent("Watch group policy child event ", path, "INFO");
    }
}

void zk_gp_child_watch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    int i, j;
    int flag;
    char childbuf[LENGTH] = "";
 
    struct Stat stat;
    struct Stat newStat;
    struct String_vector childStrings;

    struct mylist tmpStr;
    char newPolicy[DATA_LENGTH] = "";
    int newPolicyLen = sizeof(newPolicy);

    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch group policy create ", path, "INFO");
    }
    else if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch group policy delete event", path, "INFO");
    }
    else if(type == ZOO_CHANGED_EVENT)
    {
        traceEvent("Watch group policy changed event", path, "INFO");
    }
    else if(type == ZOO_SESSION_EVENT)
    {
        traceEvent("Watch group policy session faild event", path, "INFO");
    }
    else if(type == ZOO_NOTWATCHING_EVENT)
    {
        traceEvent("Remove group policy watch", path, "INFO");
    }
    
    flag = zoo_wget_children2(zkhandle, path, zk_gp_child_watch, watcherCtx, &childStrings, &stat);
    if(type == ZOO_CHILD_EVENT && childStrings.count > groupList.count)
    {
        //gettimeofday (&t_start , NULL);
        pthread_mutex_lock(&group_mutex);

        for(i=0;i<childStrings.count;i++)
        {
            int findcount = 0;
            for(j=0;j<groupList.count;j++)
            {
                if(strcmp(childStrings.data[i], groupList.data[j]))
                {
                    findcount++;
                }
            }
            if(findcount == groupList.count )
            {
                loopRun = 0;
                //struct mylist tmpStr;
                char newzkNode[DATA_LENGTH] = "";
                sprintf(newzkNode, "%s/%s", GROUP_POLICY_ZK, childStrings.data[i]);

                zoo_wget(zkhandle, newzkNode, zkgroupolicy_watch, "watch_policy", newPolicy, &newPolicyLen, &newStat);
                num++;
                //groupPolicyNum++;
                current_version++;
                traceEvent("New group policy ", newzkNode, "INFO");
                parsePolicy(newPolicy);		
		printf("new create group policy is :%s\n", newzkNode);		
		
                tmpStr.count = groupList.count+1;
                tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));

                memcpy(tmpStr.data, groupList.data, groupList.count*sizeof(char *));
                memcpy(tmpStr.mzxid, groupList.mzxid, groupList.count*sizeof(int64_t));
                tmpStr.data[tmpStr.count - 1] = strdup(childStrings.data[i]);
                tmpStr.mzxid[tmpStr.count - 1] = newStat.mzxid;	

                free(groupList.data);
                groupList = tmpStr;
                loopRun = 1;
            }
        }
        pthread_mutex_unlock(&group_mutex);
        //gettimeofday (&t_end , NULL);
        //printf("花费时间:%d\n",  t_end.tv_usec - t_start.tv_usec);
    }
    else if(type == ZOO_CHILD_EVENT && childStrings.count < groupList.count)
    {
        char zkNode[DATA_LENGTH] = "";
        char delCmd[LENGTH] = "";
        int count = 0;
	
        pthread_mutex_lock(&group_mutex);
        for(i=0; i<groupList.count;i++) 
        {
            count = 0;
            for (j=0;j<childStrings.count;j++)
            {
                if(strncmp(groupList.data[i], childStrings.data[j], strlen(groupList.data[i])) && strlen(groupList.data[i]) != 0)
                {
                    count++;	
                }
            }
            if(count == childStrings.count)
            {
                //struct mylist tmpStr;
                int m, n = 0;
                loopRun = 0;
                sprintf(zkNode, "%s/%s", GROUP_POLICY_FILE_DIR, groupList.data[i]);
                sprintf(delCmd, "/usr/local/bin/groupparser -D %s", zkNode);
                system(delCmd);
                traceEvent("Delete group policy ", zkNode, "INFO");
                //groupPolicyNum--;
		printf("delete group policy is :%s\n", zkNode);	

                remove(zkNode);
                current_version++;
                if(groupList.data[i] != NULL)
                {	
                    free(groupList.data[i]);
                    groupList.data[i] = NULL;
                }
                groupList.mzxid[i] = 0;
                tmpStr.count = groupList.count - 1;
                tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));	
                m = 0;
                for(n=0;n<groupList.count;n++)
                { 
                    if(groupList.data[n] != NULL && groupList.mzxid[n] != 0)
                    {
                        tmpStr.data[m] = (char *)malloc(strlen(groupList.data[n])+1);	
                        memset(tmpStr.data[m], 0, strlen(groupList.data[n])+1);
                        memcpy(tmpStr.data[m], groupList.data[n], strlen(groupList.data[n]));
                        tmpStr.mzxid[m] = groupList.mzxid[n];
                        m++;
                    }
                }
                free(groupList.data);
                groupList = tmpStr;	
                loopRun = 1;	
            }   
        }
        pthread_mutex_unlock(&group_mutex);
    }   
}

void zkconfig_watch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    int flag;
    char buffer1[LONG_DATA_LENGTH] = "", modifyip[DATA_LENGTH] = "";
    int bufferlen1=sizeof(buffer1);
    FILE *fp = NULL;
    char strLine[DATA_LENGTH] = "";
    
    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch create event ", path, "INFO");
    }
    if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch delete node event ", path, "INFO");
        if(!strcmp(watcherCtx, "injectInterface_watch"))
        {
            char interfaceDel[LENGTH] = "";
            sprintf(interfaceDel, "/usr/local/bin/interface -d %s", interfaceIp);
            system(interfaceDel);
            traceEvent("Inject interface policy delete event ", interfaceDel, "INFO");
        }
        else if(!strcmp(watcherCtx, "mac_watch"))
        {
            char macDel[LENGTH] = "";
            sprintf(macDel, "/usr/local/bin/gethopmac -d %s", macIp);
            system(macDel);
            memset(macIp, 0, sizeof(macIp));
            traceEvent("Mac policy delete event ", macDel, "INFO");
        }
        else if(!strcmp(watcherCtx, "globalpolicy_watch"))
        {
            remove(GLOBAL_POLICY);
            traceEvent("Global policy delete event ", GLOBAL_POLICY, "INFO");
        }
    }
    if(type == ZOO_CHANGED_EVENT | type == ZOO_CREATED_EVENT)
    {
        struct Stat stat;
        flag = zoo_get(zh, path, 0, buffer1, &bufferlen1, &stat);
        if(flag == ZOK)
        {
            if(!strcmp(watcherCtx, "injectInterface_watch"))
            {
                interfaceVersion = stat.version;
                //printf("interface version %d %s\n", interfaceVersion, buffer1);
                traceEvent("Inject interface policy changed/create ", path, "INFO");
                system("interface -F");
                cleanFile(INTERFACE_CONFIG_FILE);
                parseInterface(buffer1);		
                //system("getipmac -1");
            }
            else if(!strcmp(watcherCtx, "mac_watch"))
            {
                macVersion = stat.version;
                traceEvent("Mac policy changed/create ", path, "INFO");
                //printf("mac version %d policy create/changed %s\n", macVersion, buffer1);
                system("gethopmac -F");
                remove(IPMAC_CONFIG_FILE);
                parseMac(buffer1);
            }
            else if(!strcmp(watcherCtx, "globalpolicy_watch"))
            {
                globalVersion = stat.version;
                traceEvent("GLobal policy changed/create ", path, "INFO");
                //printf("global version %d\n", globalVersion);
                parseGlobalPolicy(buffer1);
                if((fp = fopen(GLOBAL_POLICY, "r")) == NULL)
                {
                    traceEvent("Open global policy file faild ", GLOBAL_POLICY, "INFO");
                    return;
                }
                while(fgets(strLine, DATA_LENGTH, fp))
                {
                    strLine[strlen(strLine) -1] = '\0';
                    system(strLine);
                }
                fclose(fp);
            }
            else if(!strcmp(watcherCtx, "globalpolicy_watch") && checkOutLimit != globalOutLimit)
            {
                globalVersion = stat.version;
                //printf("global version %d\n", globalVersion);
                traceEvent("GLobal policy changed/create ", path, "INFO");
                parseGlobalPolicy(buffer1);
                if((fp = fopen(GLOBAL_POLICY, "r")) == NULL)
                {
                    traceEvent("Open global policy file faild ", GLOBAL_POLICY, "INFO");
                    return;
                }
                while(fgets(strLine, DATA_LENGTH, fp))
                {
                    strLine[strlen(strLine) -1] = '\0';
                    system(strLine);
                }
                fclose(fp);
                change_group_policy(GROUP_POLICY_ZK);
                checkOutLimit = globalOutLimit;
            }
        }
    }
    if(type == ZOO_CHILD_EVENT)
    {
        traceEvent("Watch child event ", path, "INFO");
    }   
    flag = zoo_wexists(zh, path, zkconfig_watch, watcherCtx, NULL);
    if(flag != ZOK)
        traceEvent("zoo_wexists return faild ", path, "WARN");
}

void watch_config(zhandle_t *zh, char *path, char *ctx)
{
    int flag;
    flag = zoo_wexists(zkhandle, path, zkconfig_watch, ctx, NULL);
    sleep(1);
    if(flag != ZOK)
        traceEvent("zoo_wexistst config node return faild ", path, "WARN");
}

void injectroute_policy_watch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    char policyData[DATA_LENGTH] = "";
    int dataLen = sizeof(policyData);
    int flag;

    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch inject route policy create ", path, "INFO");
    }
    if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch inject route policy delete ", path, "INFO");
    }
    if(type == ZOO_CHANGED_EVENT)
    {
        struct Stat stat;
        flag = zoo_wget(zkhandle, path, injectroute_policy_watch, "watch_inject", policyData, &dataLen, &stat);
        if(flag != ZOK)
            traceEvent("zoo_wget inject route policy faild ", path, "WARN");
        else
            traceEvent("Inject route policy changed ", path, "INFO");
    }
    if(type == ZOO_CHILD_EVENT)
    {
        traceEvent("Watch inject route policy child event ", path, "INFO");
    }
}

void zkstatus_watch(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    int i=0;
    char childbuf[128] = "";
    char buffer1[64] = "";
    int bufferlen1=sizeof(buffer1);
    struct Stat stat;
    struct String_vector strings;

    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Create status event ", path, "INFO");
    }
    if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("监测到节点退出 ", path, "INFO");
    }
    if(type == ZOO_CHANGED_EVENT)
    {
        traceEvent("Watch status node changed ", path, "INFO");
    }
    if(type == ZOO_CHILD_EVENT)
    {
        int flag = zoo_wget_children2(zh, path, zkstatus_watch, watcherCtx, &strings, &stat);
        if(flag==ZOK)
        {
            for(i=0;i<strings.count;++i)
            {
                sprintf(childbuf,"%s/%s", path, strings.data[i]);

                int zwflag = zoo_wexists(zh, childbuf, zkstatus_watch, "zkstatus", &stat);
                curStatus = stat.pzxid;
                if(initStatus == curStatus)
                {
                    int zgflag=zoo_get(zkhandle, childbuf, 0, buffer1, &bufferlen1, NULL);
                    if(zgflag == ZOK)
                    {
                        traceEvent("监测到新节点加入 ", childbuf, "INFO");
                    }
                 }
             } 
        }
    }
}

void watch_status(char *str)
{
    int flag;
    struct String_vector strings;
    struct Stat stat;
    flag = zoo_wget_children2(zkhandle, str, zkstatus_watch, "status_watch", &strings, &stat);
    if(flag != ZOK)
    {
        traceEvent("zoo_wget_children2 status node faild ", str, "INFO");
    }
}

int check_exists( zhandle_t *zh, const char *path, char *nodeData, int zooNodeType)
{
    int flag;
    char createPath[DATA_LENGTH] = "";
    int pathLen = sizeof(createPath);

    struct Stat stat;
    int existsFlag = zoo_exists(zh, path, 0, &stat);
    
    if(existsFlag == ZOK)
    {
        return EXISTS;
    }
    else
    {
	flag = zoo_create(zkhandle, path, nodeData, strlen(nodeData), &ZOO_OPEN_ACL_UNSAFE, zooNodeType,/*ZOO_EPHEMERAL|ZOO_SEQUENCE,*/createPath, pathLen);
        if(flag!=ZOK)
        {
            traceEvent("Create node faild ", createPath, "ERROR");
            return FAIL;
        }
        else
        {
            traceEvent("Create node successful", createPath, "INFO");
            return OK;
        }
    }
}

int init_check_zknode(zhandle_t *zkhandle)
{
    int sock;
    int hostfd, myidfd;
    struct sockaddr_in sin;
    struct ifreq ifr;
    int flag;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
    {
        perror("socket");
        traceEvent("socket error ", "local ip", "ERROR");
        return 0;
    }
    strncpy(ifr.ifr_name, ETH_NAME, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
 
    if(ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        traceEvent("ioctl error ", "", "ERROR");
        return 0;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    sprintf(localIP, "%s", inet_ntoa(sin.sin_addr));

    if((hostfd = open(HOST_NAME_FILE, O_RDONLY))<0)perror("open");
    if((myidfd = open(ZOOKEEPER_ID_FILE, O_RDONLY))<0)perror("open");
    
    read(hostfd, hostName, sizeof(hostName));
    read(myidfd, myid, sizeof(myid));

    close(myidfd);
    close(hostfd);
    close(sock);
    
    sprintf(myStatus, "/status/%s", myid);
    stringtrim(myStatus);

    sprintf(myInjectRoute, "/config/injectroute/%s", myid);
    stringtrim(myInjectRoute);

    sprintf(myInjectInterface, "/config/injectinterface/%s", myid);
    stringtrim(myInjectInterface);
    
    sprintf(myMac, "/config/mac/%s", myid);
    stringtrim(myMac);

    sprintf(groupThreshold, "/group/threshold/%s", myid);
    stringtrim(groupThreshold);

    remove(INJECT_CONFIG_FILE); 
    remove(INTERFACE_CONFIG_FILE);
    remove(IPMAC_CONFIG_FILE);
   
    mkdir("/var/log/ad-policy", S_IRWXU);
 
    flag = check_exists(zkhandle, DEV_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", DEV_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", DEV_NODE, "INFO");
    else
        traceEvent("Create node faild ", DEV_NODE, "WARN");

    flag = check_exists(zkhandle, GLOBAL_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", GLOBAL_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", GLOBAL_NODE, "INFO");
    else
        traceEvent("Create node faild ", GLOBAL_NODE, "WARN");

    flag = check_exists(zkhandle, GROUP_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", GROUP_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", GROUP_NODE, "INFO");
    else
        traceEvent("Create node faild ", GROUP_NODE, "WARN");

    flag = check_exists(zkhandle, CONFIG_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", CONFIG_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", CONFIG_NODE, "INFO");
    else
        traceEvent("Create node faild ", CONFIG_NODE, "WARN");
   
    flag = check_exists(zkhandle, STATUS_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", STATUS_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", STATUS_NODE, "INFO");
    else
        traceEvent("Create node faild ", STATUS_NODE, "WARN");

    flag = check_exists(zkhandle, myStatus, localIP, ZOO_EPHEMERAL);
    if(flag == OK)
        traceEvent("Create node successful ", myStatus, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", myStatus, "INFO");
    else
        traceEvent("Create node faild ", myStatus, "WARN");
 
    flag = check_exists(zkhandle, GLOBAL_POLICY_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", GLOBAL_POLICY_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", GLOBAL_POLICY_NODE, "INFO");
    else
        traceEvent("Create node faild ", GLOBAL_POLICY_NODE, "WARN");

    flag = check_exists(zkhandle, GROUP_POLICY_NODE, "", 0);
    if(flag == OK)
        traceEvent("Create node successful ", GROUP_POLICY_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", GROUP_POLICY_NODE, "INFO");
    else
        traceEvent("Create node faild ", GROUP_POLICY_NODE, "WARN");

    flag = check_exists(zkhandle, CONFIG_INJECTROUTE_NODE, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", CONFIG_INJECTROUTE_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", CONFIG_INJECTROUTE_NODE, "INFO");
    else
        traceEvent("Create node faild ", CONFIG_INJECTROUTE_NODE, "WARN");
    
    flag = check_exists(zkhandle, myInjectRoute, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", myInjectRoute, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", myInjectRoute, "INFO");
    else
        traceEvent("Create node faild ", myInjectRoute, "WARN");
    
    flag = check_exists(zkhandle, CONFIG_INJECTINTERFACE_NODE, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", CONFIG_INJECTINTERFACE_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", CONFIG_INJECTINTERFACE_NODE, "INFO");
    else
        traceEvent("Create node faild ", CONFIG_INJECTINTERFACE_NODE, "WARN");

    flag = check_exists(zkhandle, myInjectInterface, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", myInjectInterface, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", myInjectInterface, "INFO");
    else
        traceEvent("Create node faild ", myInjectInterface, "WARN");

    flag = check_exists(zkhandle, CONFIG_MAC_NODE, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", CONFIG_MAC_NODE, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", CONFIG_MAC_NODE, "INFO");
    else
        traceEvent("Create node faild ", CONFIG_MAC_NODE, "WARN");

    flag = check_exists(zkhandle, myMac, "[]", 0);
    if(flag == OK)
        traceEvent("Create node successful ", myMac, "INFO");
    else if(flag == EXISTS)
        traceEvent("Exists node ", myMac, "INFO");
    else
        traceEvent("Create node faild ", myMac, "WARN");

    mkdir(GROUP_FILE_DIR, S_IRWXU);
    mkdir(GLOBAL_POLICY_FILE_DIR, S_IRWXU);
    mkdir(GROUP_THRESHOLD_FILE_DIR, S_IRWXU);
    mkdir(GROUP_POLICY_FILE_DIR, S_IRWXU);
    mkdir("/etc/conf/cluster", S_IRWXU);
    
    if(!access(ZOOKEEPER_CFG,0))
        traceEvent("Exists file ", "zoo.cfg", "INFO");
    else
	system("python ./init.py");   
    return 1; 
}

void isLeader()
{
    char tmpbuf[DATA_LENGTH] = "";
    FILE *pp = popen(ZOOKEEPER_STATUS, "r");
    if (!pp)
        return -1;

    fread( tmpbuf, sizeof(char), sizeof(tmpbuf),  pp);
    traceEvent("zookeeper run in ", tmpbuf, "INFO");
    pclose(pp);
}

void my_data_completion(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)
{
    traceEvent("Group threshold policy ", value, "INFO");
    strncat(groupThresholdStr, value, value_len); 
    strcat(groupThresholdStr, "\n"); 
    count++;
    free((void*)data);
}

void my_strings_stat_completion(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data)
{
    int i, flag;
    char line[LENGTH] = "";
    count1 = strings->count;
    zhandle_t* zkh;
    zkh = (zhandle_t*)data;
    
    for (i=0; i < strings->count; i++) 
    {
        sprintf(line, "%s/%s", GROUP_THRESHOLD_ZK, strings->data[i]);	    
        flag = zoo_aget(zkh, line, 1, my_data_completion, strdup(line));
        if(flag != ZOK)
        {
            traceEvent("zoo aget my strings stat faild ", line, "INFO");
            continue;
        }
    }
}

char* get_threshold(char* host, int timeout, char *path)
{
    int flag;
    zhandle_t* zk;
    zk = zookeeper_init(host, 0, timeout, 0, "hello zookeeper", 0);
    count = count1 = 0;

    groupThresholdStr = (char *)malloc(sizeof(char)*LONG_DATA_LENGTH);
    flag = zoo_aget_children2(zk, path, 1, my_strings_stat_completion, /*strdup(path)*/(void *)zk);
    if(flag != ZOK)
    {
        traceEvent("zoo aget aget threshold children2 faild ", path, "INFO");
        return;
    }
    
    while(1)
    {
        if(count == count1 != 0 && strlen(groupThresholdStr) > 0)
        {
            count = count1 = 0;
            return groupThresholdStr;
        }
        sleep(3);
    }
}

void group_policy_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data) 
{
    char buffer[5000] = "";
    if(!strcmp(data, "policy"))
    {
        memcpy(buffer, value, value_len);
        groupList.mzxid[mycount] = stat->mzxid;
        mycount++;
        parsePolicy(buffer);
    }
}

void group_policy_changed(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)
{
    char buffer[5000] = "";
    if(!strcmp(data, "policy"))
    {
        memcpy(buffer, value, value_len);
        parsePolicy(buffer);
    }
}

void global_threshold_changed(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data)
{
    int i, flag;
    char line[LENGTH] = "";
    int type;

    if (strings)
    {
        for (i=0; i < strings->count; i++)
        {
            sprintf(line, "/group/policy/%s", strings->data[i]);
            flag = zoo_awget(zkhandle, line, false, "", group_policy_changed, strdup("policy"));
            if(flag != ZOK)
            {
                traceEvent("zoo awget group policy name faild ", line, "INFO");
            }
        }
    }
}

void group_policy_name(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data) 
{
    int i, flag;
    char line[LENGTH] = "";
    int type;
				 
    if (strings && !strcmp(data, GROUP_POLICY_ZK))
    {
        groupList.count = strings->count;
        groupList.data = (char**)malloc(sizeof(char*) * groupList.count);
	
        current_version = stat->cversion;
        for (i=0; i < strings->count; i++)
        {
            sprintf(line, "/group/policy/%s", strings->data[i]);
	    //printf("get group policy :%s\n", line);

            groupList.data[i] = (char *)malloc(strlen(strings->data[i])+1);
            memset(groupList.data[i], 0, strlen(strings->data[i])+1);

            memcpy(groupList.data[i], strings->data[i], strlen(strings->data[i]));

            groupList.data[i][strlen(groupList.data[i])] = '\0';

            traceEvent("My group policy name list ", groupList.data[i], "INFO");
	    
            flag = zoo_awget(zkhandle, line, zkgroupolicy_watch, "watch_policy", group_policy_content, strdup("policy"));
            if(flag != ZOK)
            {
                traceEvent("zoo awget group policy name faild ", line, "INFO");
            }
        }
    }
    free((void*)data);
}

void get_group_policy(char *path, struct mylist* policylist)
{
    int flag;
    int i = 0;
    char childNodeName[LENGTH] = "";
    mycount = 0;

    flag = zoo_aget_children2(zkhandle, path, 1, group_policy_name, strdup(path));
    if(flag != ZOK)
    {
        traceEvent("zoo aget children2 get group policy node  faild ", path, "ERROR");
        return;
    }
    while(1)
    {
        sleep(2);
        if(groupList.count == mycount != 0)
        {
            mycount = 0;
            return;
        }
    } 
}

void change_group_policy(char *path)
{
    int flag = zoo_aget_children2(zkhandle, path, 1, global_threshold_changed, strdup(path));
    if(flag != ZOK)
    {
        traceEvent("zoo aget children2 change group policy node faild ", path, "ERROR");
        return;
    }
    sleep(3);
    return;
}

void get_global_policy(char *str)
{
    FILE *fp = NULL;
    int flag;
    char globalPolicy[DATA_LENGTH] = "";
    int policyLen = sizeof(globalPolicy);
    char strLine[DATA_LENGTH] = "";
    struct Stat stat;
    
    flag = zoo_wget(zkhandle, str, false, "global", globalPolicy, &policyLen, &stat);
    if(flag == ZOK)
    {
        globalVersion = stat.version;
        parseGlobalPolicy(globalPolicy);
        if((fp = fopen(GLOBAL_POLICY, "r")) == NULL)
        { 
            traceEvent("Can't open global policy file ", "", "ERROR");
            return; 
        } 
        while(fgets(strLine, DATA_LENGTH, fp))
        { 
            strLine[strlen(strLine) -1] = '\0';
            system(strLine);
        }
        fclose(fp); 
    }
    else
    {
        traceEvent("zoo get global policy faild ", str, "ERROR");
    }
}

char* get_global_threshold(char* zkNode, int timeout, char *str, void wfun(zhandle_t*, int, int, const char*, void*), void dfun(int, const char*, int, const struct Stat*, const void*))
{
    int flag;
    zhandle_t* zk;
    char globalPolicy[DATA_LENGTH] = "";
    int policyLen = sizeof(globalPolicy);
    char strLine[DATA_LENGTH] = "";
    
    zk = zookeeper_init(zkNode, 0, timeout, 0, "hello zookeeper", 0);
    flag = zoo_awget(zk, str, wfun, NULL, dfun, str);
    if(flag != ZOK)
        traceEvent("zoo awget global threshold node faild ", str, "ERROR");
}

void injectRoute_policy_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)  
{  
    traceEvent("Inject route policy content ", value, "INFO");
    parseInject((char *)value);  
} 

void injectRoute_policy_name(int rc, struct String_vector *strings, const void *data)
{
    int i, flag;
    char path[LENGTH] = "";
    struct String_vector str_vec = *strings;

    injectrouteList.count = strings->count;
    injectrouteList.data = (char**)malloc(sizeof(char*) * injectrouteList.count);

    for (i = 0; i < str_vec.count; i++) 
    {  
        sprintf(path ,"%s/%s", myInjectRoute, str_vec.data[i]);
        injectrouteList.data[i] = (char *)malloc(strlen(str_vec.data[i])+1);
        memset(injectrouteList.data[i], 0, strlen(str_vec.data[i])+1);
        memcpy(injectrouteList.data[i], str_vec.data[i], strlen(str_vec.data[i]));
        injectrouteList.data[i][strlen( injectrouteList.data[i])] = '\0';
        
        traceEvent("Inject route policy name ", path, "INFO");
        flag = zoo_awget(zkhandle, path, injectroute_policy_watch, NULL, injectRoute_policy_content, NULL);  
        if(flag != ZOK)
            traceEvent("zoo awget inject route policy faild ", path, "ERROR");
    }  
}

void watch_injectRoute(zhandle_t* zkh, int type, int state, const char* path, void* watcher)
{
    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch create event ", path, "INFO");
    }
    else if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch delete event ", path, "INFO");
    }
    else if(type == ZOO_CHANGED_EVENT)
    {
        traceEvent("Watch change event ", path, "INFO");
    }
    else if(type == ZOO_SESSION_EVENT)
    {
        traceEvent("Watch session fail event ", path, "INFO");
    }
    else if(type == ZOO_NOTWATCHING_EVENT)
    {
        traceEvent("Watch move watch event ", path, "INFO");
    }
    /*int ret = zoo_awget_children(zkhandle, path, watch_injectRoute, NULL, injectRoute_policy_name, NULL);  
    if (ZOK != ret) {  
        printf("zookeeper wget error\n");  
    }*/  
}
												
int get_children_policy(char *str, void wfun(zhandle_t*, int, int, const char*, void*), void dfun(int, struct String_vector*, const void*))
{
    int i = 0;
    int flag = zoo_awget_children(zkhandle, str, wfun, NULL, dfun, strdup(str));
    if(flag == ZOK)
    {
        return 1;
    }
    else
    {
        traceEvent("zoo awget children policy node fail ", str, "ERROR");
        return 0;
    }
}

void parseGroupThreshold(char *pMsg, char *path)
{
    int i, cnt, fd;
    char grothreshold[DATA_LENGTH] = "";
    char udpThreshold[LENGTH] = "";
    char httpThreshold[LENGTH] = "";
    char httpsThreshold[LENGTH] = "";
    char ackThreshold[LENGTH] = "";
    char synThreshold[LENGTH] = "";
    char dnsThreshold[LENGTH] = "";
    char lhttpThreshold[LENGTH] = "";
    char icmpThreshold[LENGTH] = "";
    char ip[LENGTH] = "group ip";    

    cJSON * pJson = cJSON_Parse(pMsg);
    if(NULL == pJson)
    {
        return ;
    }
    if((fd = open(path, O_RDWR | O_CREAT))<0)
    {
        perror("open");
        traceEvent("Open file faild ", path, "ERROR");
        return;
    }
    cJSON *threshold = cJSON_GetObjectItem(pJson, "threshold");
    if(threshold != NULL)
    {
        cJSON *udp = cJSON_GetObjectItem(threshold, "udp");
        sprintf(grothreshold ,"udp threshold %s\n", udp->valuestring);

        cJSON *http = cJSON_GetObjectItem(threshold, "http");
        sprintf(httpThreshold, "http hteshold %s\n", http->valuestring);
        strcat(grothreshold, httpThreshold);

        cJSON *https = cJSON_GetObjectItem(threshold, "https");
        sprintf(httpsThreshold, "https threshold %s\n", https->valuestring);
        strcat(grothreshold, httpsThreshold);
	
        cJSON *ack = cJSON_GetObjectItem(threshold, "ack");
        sprintf(ackThreshold, "ack threshold %s\n", ack->valuestring);
        strcat(grothreshold, ackThreshold);

        cJSON *syn = cJSON_GetObjectItem(threshold, "syn");
        sprintf(synThreshold, "syn threshold %s\n", syn->valuestring);
        strcat(grothreshold, synThreshold);
        cJSON *dns = cJSON_GetObjectItem(threshold, "dns");
        sprintf(dnsThreshold, "dns threshold %s\n", dns->valuestring);
        strcat(grothreshold, dnsThreshold);

        cJSON *lhttp = cJSON_GetObjectItem(threshold, "lhttp");
        sprintf(lhttpThreshold, "lhttp threshold %s\n", lhttp->valuestring);
        strcat(grothreshold, lhttpThreshold);

        cJSON *icmp = cJSON_GetObjectItem(threshold, "icmp");
        sprintf(icmpThreshold, "icmp threshold %s\n", icmp->valuestring);
        strcat(grothreshold, icmpThreshold);
    }
    cJSON *groupIp = cJSON_GetObjectItem(pJson, "groupIp");
    if(groupIp != NULL)
    {
       cnt = cJSON_GetArraySize(groupIp);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(groupIp, i);
            strcat(ip, pArrayItem->valuestring);
            strcat(ip, " ");
        }
        strcat(grothreshold, ip);	
    }
    if((write(fd, grothreshold, strlen(grothreshold))) == 0)
    {
        traceEvent("Writing to file fail ", path, "ERROR");
        return;
    }
    close(fd);
    cJSON_Delete(pJson);
}

void get_config_policy(char *str)
{
    int flag;
    char content[DATA_LENGTH] = "";
    int length = sizeof(content);
    struct Stat stat;   

    flag = zoo_get(zkhandle, str, 0, content, &length, &stat);
    if(flag ==ZOK)
    {
        if(!strcmp(str, myMac))
        {
            traceEvent("Mac policy data is ", content, "INFO");
            macVersion = stat.version;
            system("gethopmac -F");
            parseMac(content);
        }
        if(!strcmp(str, myInjectInterface))
        {
            interfaceVersion = stat.version;
            traceEvent("Inject interface policy ", content, "INFO");
            system("interface -F");
            parseInterface(content);
        }
    }
    else
    {
        traceEvent("zoo get config policy node fail ", str, "ERROR");
    }
}

void watch_group_policy(char *path, char *str)
{
    struct String_vector strings;
    int flag = zoo_wget_children(zkhandle, path, zk_gp_child_watch, str, &strings);
    if(flag != ZOK)
    {
        printf("zoo awget children group policy fail...\n");
    }
}

static void stop (int sig) 
{
    traceEvent("Catch ctr+c singal ", "", "INFO");
    int i;
    for(i=0;i<groupList.count;i++)
    {
        if(groupList.data[i] != NULL)
        {
            free(groupList.data[i]);
            groupList.data[i] = NULL;
        }
    }
    for(i=0;i<injectrouteList.count;i++)
    {
        if(injectrouteList.data[i] != NULL)
        {
            free(injectrouteList.data[i]);
            injectrouteList.data[i] = NULL;
        }
    }
    free(groupList.data);
    if(injectrouteList.data != NULL)
        free(injectrouteList.data);
    free(groupThresholdStr);
    zookeeper_close(zkhandle);
    traceEvent("Stop process ", "", "INFO");
    exit(0);
}

void get_interface_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)
{
    if(interfaceVersion != stat->version)
    {
        //printf("getlost interface version %d %s\n", interfaceVersion, value);
        system("interface -F");
        cleanFile(INTERFACE_CONFIG_FILE);
        parseInterface(value);                        
        //system("getipmac -1");

        interfaceVersion = stat->version;
        /*int flag = zoo_wexists(zkhandle, myInjectInterface, zkconfig_watch, "injectInterface_watch", NULL);
        if(flag != ZOK)
            traceEvent("zoo_wexists return faild ", "interface", "WARN");*/
    }
}

void get_global_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)
{
    if(globalVersion != stat->version)
    {
        FILE *fp;
        char getLine[DATA_LENGTH] = "";
        char buffer[5000] = "";
        snprintf(buffer, value_len+1, "%s", value);
    
        parseGlobalPolicy(buffer);
        if((fp = fopen(GLOBAL_POLICY, "r")) == NULL)
        {
            traceEvent("Open global policy file faild ", GLOBAL_POLICY, "INFO");
            return;
        }
        while(fgets(getLine, DATA_LENGTH, fp))
        {
            getLine[strlen(getLine) -1] = '\0';
            system(getLine);
        }
        fclose(fp);
        
        if(checkOutLimit != globalOutLimit)
        {
            change_group_policy(GROUP_POLICY_ZK);
            checkOutLimit = globalOutLimit; 
        }
        globalVersion = stat->version;
        //printf("get lost global policy %d\n", globalVersion);
    }
}

void get_mac_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data)
{
    if(macVersion != stat->version)
    {
        char buffer[DATA_LENGTH] = "";
        snprintf(buffer, value_len + 1, "%s", value);
        //printf("get lost mac version %d policy %s\n", macVersion, buffer);
        system("gethopmac -F");
        remove(IPMAC_CONFIG_FILE);
        parseMac(buffer);
        macVersion = stat->version;
    }
}

void loop_watch_policy()
{
    int num;
    int i, j;
    char newPolicy[DATA_LENGTH] = "";
    int newPolicyLen = sizeof(newPolicy);
    struct Stat curStat;
    struct String_vector curStrings;
    
    char curPolicy[DATA_LENGTH] = "";
    char curNode[LENGTH] = "";
    int curMzxid, mzxid;
    char delCmd[LENGTH] = "";
    
    while(1)//loopRun == 1)
    {
        struct Stat macStat;
        struct Stat globalStat;
        struct Stat interfaceStat;
        if(loopRun == 0)continue;       

        sleep(10);
        num = 0;
        int flag = zoo_get_children2(zkhandle, GROUP_POLICY_ZK, false, &curStrings, &curStat);
        if(flag == ZOK && curStat.cversion != current_version && curStrings.count > groupList.count || flag == ZOK && curStat.cversion != current_version)
        {
            pthread_mutex_lock(&group_mutex);
            for (i=0;i<curStrings.count;i++)
            {
                int findcount = 0;
                for(j=0;j<groupList.count;j++)
                {
                    if(strcmp(curStrings.data[i], groupList.data[j]))
                    findcount++;
                    if(!strcmp(curStrings.data[i], groupList.data[j]))
                    {
                        int dataLen = sizeof(curPolicy);
                        struct Stat curStat;

                        sprintf(curNode, "%s/%s", GROUP_POLICY_ZK, curStrings.data[i]);
                        zoo_get(zkhandle, curNode, false, curPolicy, &dataLen, &curStat);

                        curMzxid = curStat.mzxid;
                        mzxid = groupList.mzxid[j];
                        if(curMzxid != mzxid)
                        {
                            traceEvent("Get the lost set group policy name ", curStrings.data[i], "INFO");
                            parsePolicy(curPolicy);
                            groupList.mzxid[j] = curStat.mzxid;
                        }
                    }
                }
                if(findcount == groupList.count )
                {
                    struct mylist tmpStr;
                    char newzkNode[DATA_LENGTH] = "";
                    struct Stat curStat, newStat;

                    sprintf(newzkNode, "%s/%s", GROUP_POLICY_ZK, curStrings.data[i]);

                    num++;
                    zoo_wget(zkhandle, newzkNode, zkgroupolicy_watch, "groupolicy_watch", newPolicy, &newPolicyLen, &newStat);
                    parsePolicy(newPolicy);
                    traceEvent("Get the lost group policy name ", newzkNode, "INFO");
		    //groupPolicyNum++;
		    printf("get the lost created group policy :%s\n", newzkNode);

                    current_version++;
                    tmpStr.count = groupList.count+1;
                    tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));

                    memcpy(tmpStr.data, groupList.data, groupList.count*sizeof(char *));
                    memcpy(tmpStr.mzxid, groupList.mzxid, groupList.count);
                    tmpStr.data[tmpStr.count - 1] = strdup(curStrings.data[i]);
                    tmpStr.mzxid[tmpStr.count - 1] = newStat.mzxid;
	            free(groupList.data);
                    groupList = tmpStr;
                }
            }
            pthread_mutex_unlock(&group_mutex);
        }
        if(flag == ZOK && curStat.version != current_version && curStrings.count < groupList.count)
        {
            pthread_mutex_lock(&group_mutex);
            for(i=0;i<groupList.count;i++)
            {
                int count = 0;
                for (j=0;j<curStrings.count;j++)
                {
                    if(strcmp(groupList.data[i], curStrings.data[j]))
                        count++;
                }
                if(count == curStrings.count)
                {
	            int m = 0, n = 0;
                    struct mylist tmpStr;
                    char delFile[LENGTH] = "";
                    num++;
                    sprintf(delFile, "%s/%s", GROUP_POLICY_FILE_DIR, groupList.data[i]);
                    traceEvent("Get the lost delete group policy name ", delFile, "INFO");
                    sprintf(delCmd, "/usr/local/bin/groupparser -D %s", delFile);
                    system(delCmd);
                    remove(delFile);

		    //groupPolicyNum--;
		    printf("get the lost delete group policy :%s\n", delFile);

                    current_version++;
                    if(groupList.data[i] != NULL)
                    {
                        groupList.mzxid[i] = 0;
                        free(groupList.data[i]);
                        groupList.data[i] = NULL;
                    }
                    tmpStr.count = groupList.count - 1;
                    tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));		
                    for(n=0;n<groupList.count;n++)
                    {
                        if(groupList.data[n] != NULL && groupList.mzxid[n] != 0)
                        {
                            tmpStr.data[m] = (char *)malloc(strlen(groupList.data[n])+1);
                            memset(tmpStr.data[m], 0, strlen(groupList.data[n])+1);
                            memcpy(tmpStr.data[m], groupList.data[n], strlen(groupList.data[n]));
                            tmpStr.mzxid[m] = groupList.mzxid[n];
                            m++;
                        }
                    }
                    free(groupList.data);
                    groupList = tmpStr;
                }
            }
            pthread_mutex_unlock(&group_mutex);
        }
 
        int macFlag = zoo_awget(zkhandle, myMac, zkconfig_watch, "mac_watch", get_mac_content, strdup("check"));
        if(macFlag != ZOK)
        {
            printf("zoo_awget global fail....\n");
        }

        int globalFlag = zoo_awget(zkhandle, GLOBAL_POLICY_ZK, zkconfig_watch, "globalpolicy_watch", get_global_content, strdup("check"));
        if(globalFlag != ZOK)
        {
            printf("zoo_awget global fail....\n");
        }
        
        int interfaceFlag = zoo_awget(zkhandle, myInjectInterface, zkconfig_watch, "injectInterface_watch", get_interface_content, strdup("check"));
        if(interfaceFlag != ZOK)
        {
            printf("zoo_awget interface fail...\n");
        }
        int injectflag = zoo_awget_children(zkhandle, myInjectRoute, watch_injectroute, NULL, inject_child_node_event, strdup("check"));
        if(injectflag != ZOK) 
        {
            printf("zoo awget children injectroute fail...\n");
        }
    }
}

void injectroute_child_node_content(int rc, const char *value, int value_len, const struct Stat *stat, const void *data) 
{
    FILE *fp = NULL;
    char strLine[DATA_LENGTH] = "";
    char injectConfig[DATA_LENGTH] = "";
    char delCmd[DATA_LENGTH] = "";
    char buffer[5000] = "";
    char *changeNode = NULL;
    changeNode = (char*)data;
    snprintf(buffer, value_len+1, "%s", value);
    
    if(strstr(changeNode , myInjectRoute))
    {
        traceEvent("Inject route changed node ", changeNode+strlen(myInjectRoute)+1, "INFO");
        char *str = replace(changeNode+strlen(myInjectRoute)+1, '_', ' ');
	 	
        //traceEvent("Inject child changed  ", str, "INFO");
        if((fp = fopen(INJECT_CONFIG_FILE, "r")) == NULL)
        {
            traceEvent("Can't open inject route policy file ", INJECT_CONFIG_FILE, "ERROR");
            fclose(fp);
            return;
        }
        while(fgets(strLine, DATA_LENGTH, fp))
        {
            strLine[strlen(strLine) -1] = '\0';
            if(strncmp(strLine, str, strlen(str)))
            {
                strcat(injectConfig, strLine);
                strcat(injectConfig, "\n");
            }
            else
            {
                sprintf(delCmd, "/usr/local/bin/setinject -d %s", strLine);
                system(delCmd);
                traceEvent("Delete inject policy ", delCmd, "INFo");
            }
        }
        fclose(fp);
        if((fp = fopen(INJECT_CONFIG_FILE, "w")) == NULL)
        {
             traceEvent("Can't open inject route policy file ", INJECT_CONFIG_FILE, "ERROR");
             fclose(fp);
             return;
        }
        fwrite(injectConfig, strlen(injectConfig), 1, fp);
        fclose(fp);
    }
    parseInject(value);
}

void watch_inject_child_node(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    int flag;
    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch create event ", path, "INFO");
        flag = zoo_awget(zkhandle, path, watch_inject_child_node, NULL, injectroute_child_node_content, strdup("create"));
        if(flag != ZOK)
        {
            traceEvent("zoo awget watch inject child create node faild ", path, "ERROR");
        }
        else
        {
            system("getipmac -1"); 
        }
        return;
    }
    if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch delete event ", path, "INFO");
        //system("/usr/local/bin/setinject -F");
        //flag = zoo_awget(zkhandle, path, watch_inject_child_node, NULL, delete_all_injectroute, strdup("delete"));
        return;	
    }
    if(type == ZOO_CHANGED_EVENT)
    {
        traceEvent("Watch changed event ", path, "INFO");
        flag = zoo_awget(zkhandle, path, watch_inject_child_node, NULL, injectroute_child_node_content, strdup(path));
        if(flag != ZOK)
        {
            traceEvent("zoo awget inject child change node faild ", path, "ERROR");
        }
        else
        {
            system("getipmac -1"); 
        }
        return;
    }
}

void injectroute_child_node(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data)
{
    int i, flag;
    char line[LENGTH] = "";

    injectrouteList.count = strings->count;
    injectrouteList.data = (char**)malloc(sizeof(char*) * injectrouteList.count);

    for (i=0; i < strings->count; i++)
    {
        sprintf(line, "%s/%s", myInjectRoute, strings->data[i]);
        injectrouteList.data[i] = (char *)malloc(strlen(strings->data[i]) + 1);
        memset(injectrouteList.data[i], 0, strlen(injectrouteList.data[i]) + 1);

        memcpy(injectrouteList.data[i], strings->data[i], strlen(strings->data[i]) + 1);
        strings->data[i][strlen(strings->data[i])] = '\0';
        flag = zoo_awget(zkhandle, line, watch_inject_child_node, NULL, injectroute_child_node_content, strdup("policy"));
        if(flag !=ZOK)
        {
            traceEvent("zoo awget inject route child faild ", line, "ERROR");
            return ;
        }
    }
}

void inject_child_node_event(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data) 
{
    int i, j, count, flag;
    int m = 0, n = 0;
    char line[LENGTH] = "";
    char strLine[DATA_LENGTH] = "";
    char injectrouteConfig[50000] = "";

    char delzkNode[DATA_LENGTH] = "";
    char delCmd[LENGTH] = "";
    char delInjectChild[LENGTH] = "";
    FILE *fp = NULL;
   
    if(strings->count > injectrouteList.count)
    {
        pthread_mutex_lock(&injectroute_mutex);
        for(i=0;i<strings->count;i++)
        {
            count = 0;
            for(j=0;j<injectrouteList.count;j++)
            {
                if(strcmp(strings->data[i], injectrouteList.data[j]))
	            count++;
            }    
            if(count == injectrouteList.count)
            {
                traceEvent("fine the new injectroute node ", strings->data[i], "INFO");
                struct String_vector tmpStr;
                tmpStr.count = injectrouteList.count+1;

                tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));
                memcpy(tmpStr.data, injectrouteList.data, injectrouteList.count*sizeof(char *));
                tmpStr.data[tmpStr.count - 1] = strdup(strings->data[i]);
                free(injectrouteList.data);
                injectrouteList = tmpStr;
		
                sprintf(line, "%s/%s", myInjectRoute, strings->data[i]);
                //printf("new injectroute :%s\n", line);
                flag = zoo_awget(zkhandle, line, watch_inject_child_node, NULL, injectroute_child_node_content, strdup("policy"));
                if(flag != ZOK)
                {
                    traceEvent("zoo awexists faild ", line, "ERROR");
	        }
            }
        }
        pthread_mutex_unlock(&injectroute_mutex);
    }
    else if(strings->count < injectrouteList.count)
    {
        pthread_mutex_lock(&injectroute_mutex);
        for(i=0; i<injectrouteList.count; i++)
        {
            count = 0;
            for (j=0; j<strings->count; j++)
            {
                if(strcmp(injectrouteList.data[i], strings->data[j]))
                    count++;
            }
            if(count == strings->count )
            {
                struct String_vector tmpStr;
                m = 0;
                sprintf(delInjectChild, "%s", injectrouteList.data[i]);
               	char *str = delInjectChild; 
                sprintf(delzkNode, "%s/%s", myInjectRoute, injectrouteList.data[i]);
                   
                traceEvent("Delete the inject route node ", delzkNode, "INFO");
                str=replace(str,'_',' ');
	        //printf("delete injectroute :%s\n", delzkNode);	

                if((fp = fopen(INJECT_CONFIG_FILE, "r")) == NULL)
                {
                    traceEvent("Can't open file ", INJECT_CONFIG_FILE, "ERROR");
                    fclose(fp);
                    return;
                }
                while(fgets(strLine, DATA_LENGTH, fp))
                {
                    stringtrim(strLine);
                    if(strncmp(strLine, str, strlen(str)))
                    {
                        strcat(injectrouteConfig, strLine);
                        strcat(injectrouteConfig, "\n");
                    }
                    else
                    {
                        sprintf(delCmd, "/usr/local/bin/setinject -d %s", strLine);
                        system(delCmd);
                        traceEvent("Delete inject policy ", delCmd, "ERROR");
                    }
                }
                fclose(fp);
                fp = fopen(INJECT_CONFIG_FILE, "w");
                fwrite(injectrouteConfig, strlen(injectrouteConfig), 1, fp);	
                memset(injectrouteConfig, 0, 50000);
                fclose(fp);	
	
                if(injectrouteList.data[i] != NULL)
                {
                    free(injectrouteList.data[i]);
                    injectrouteList.data[i] = NULL;
                }
                tmpStr.count = injectrouteList.count - 1;
                tmpStr.data = (char**)malloc(tmpStr.count * sizeof(char *));
                for(n=0;n<injectrouteList.count;n++)
                {
                    if(injectrouteList.data[n] != NULL)
                    {
                        tmpStr.data[m] = (char *)malloc(strlen(injectrouteList.data[n])+1);
                        memset(tmpStr.data[m], 0, strlen(injectrouteList.data[n])+1);
                        memcpy(tmpStr.data[m], injectrouteList.data[n], strlen(injectrouteList.data[n]));
                        m++;
                    }
                }
                free(injectrouteList.data);
                injectrouteList = tmpStr;
    	    }
        }
        pthread_mutex_unlock(&injectroute_mutex);
    }	
}


void watch_injectroute(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx)
{
    if(type == ZOO_CREATED_EVENT)
    {
        traceEvent("Watch create event", path, "INFO");
    }
    else if(type == ZOO_DELETED_EVENT)
    {
        traceEvent("Watch delete event", path, "INFO");
        system("/usr/local/bin/setinject -F");
        check_exists(zkhandle, myInjectRoute, "[]", 0); 
    }
    else if(type == ZOO_CHANGED_EVENT)
    {
        traceEvent("Watch changed event", path, "INFO");
    }
    else if(type == ZOO_CHILD_EVENT)
    {
        traceEvent("Watch child event", path, "INFO");
        int flag = zoo_awget_children(zkhandle, path, watch_injectroute, NULL, inject_child_node_event, strdup(path));
        if(flag != ZOK)
        {
            traceEvent("zoo awget children watch inject route fail", path, "ERROR");
        }
    }
}

int get_watch_injectroute_policy(char *path)
{
    int flag = zoo_awget_children(zkhandle, path, watch_injectroute, NULL, injectroute_child_node, strdup(path));
    if(flag == ZOK)
    {
        sleep(1);
        return 1;
    }
    else
    {
        return 0;
    }
}

void* watchGetThread() 
{
    int i, j;
    int timeout = 30000;
    const char *host = "localhost:2181";
    
    get_zookeeper_env();
    zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);

    zkhandle = zookeeper_init(host, main_watcher, timeout, 0, "hello zookeeper.", 0);
    while(!connected) 
    {
        sleep(1);
    }
    if (zkhandle ==NULL)
    {
        traceEvent("When init connecting to zookeeper servers...", "", "ERROR");
        return;
    }

    if(init_check_zknode(zkhandle))
        traceEvent("Check node ", "over", "INFO");

    isLeader();
    signal(SIGINT, stop);

    get_global_policy(GLOBAL_POLICY_ZK);
    checkOutLimit = globalOutLimit;
    get_group_policy(GROUP_POLICY_ZK, &groupList);

    get_config_policy(myMac);
    get_config_policy(myInjectInterface);

    watch_status("/status");    
    
    watch_group_policy(GROUP_POLICY_ZK, "group_policy");
    watch_config(zkhandle, GLOBAL_POLICY_ZK, "globalpolicy_watch");

    watch_config(zkhandle, myMac, "mac_watch");
    watch_config(zkhandle, myInjectInterface, "injectInterface_watch");
    get_watch_injectroute_policy(myInjectRoute); 
    loop_watch_policy();
}

int main(int argc, const char *argv[])
{
    check_is_running("policywatcher");    
    watchGetThread();
    
    while(1)
    {
      sleep(5);	
    }
}
