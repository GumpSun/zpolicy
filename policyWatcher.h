#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "zookeeper.h"
#include "zookeeper.jute.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
//#include <semaphore.h>

#include "cJSON.h"
#include "utility.h"
#include "parse.h"

#define FAIL                            0
#define OK                              1
#define EXISTS                          2
#define ZOOM                            4

#define GROUP_POLICY_TYPE		1
#define GROUP_THRESHOLD_TYPE		2

#define IP_LENGTH                       16
#define LENGTH                          512
#define DATA_LENGTH                     1024
#define LONG_DATA_LENGTH                2048

#define ETH_NAME                        "eth1" 
#define DEV_NODE                        "/dev"
#define CONFIG_NODE                     "/config"
#define STATUS_NODE                     "/status"
#define GLOBAL_NODE                     "/global"
#define GROUP_NODE                      "/group"
#define GLOBAL_POLICY_NODE              "/global/policy"
#define GROUP_POLICY_NODE               "/group/policy"

#define CONFIG_MAC_NODE                 "/config/mac"
#define CONFIG_INJECTROUTE_NODE         "/config/injectroute"
#define CONFIG_INJECTINTERFACE_NODE     "/config/injectinterface"

#define HOST_NAME_FILE                  "/proc/sys/kernel/hostname"

#define POLICY_DIR                      "/etc/conf"
#define GROUP_FILE_DIR			"/etc/conf/group"
#define GROUP_POLICY_FILE_DIR		"/etc/conf/group/policy"
#define GROUP_THRESHOLD_FILE_DIR	"/etc/conf/group/threshold"

#define GLOBAL_POLICY_FILE_DIR		"/etc/conf/global"
#define GLOBAL_POLICY                   "/etc/conf/global/policy"
#define GLOBAL_THRESHOLD                "/etc/conf/global/threshold"

#define GROUP_POLICY_ZK			"/group/policy"
#define GROUP_THRESHOLD_ZK              "/group/threshold"

#define GLOBAL_POLICY_ZK		"/global/policy"
#define GLOBAL_THRESHOLD_ZK             "/global/threshold"

#define INJECT_CONFIG_FILE              "/etc/conf/cluster/injectroute.conf"     
#define INTERFACE_CONFIG_FILE           "/etc/conf/cluster/interface.conf"
#define IPMAC_CONFIG_FILE               "/etc/conf/cluster/portmac.conf"

#define ZOOKEEPER_ID_FILE               "/usr/local/zookeeper/data/myid"
#define ZOOKEEPER_RESTART               "/usr/local/zookeeper/bin/zkServer.sh restart"
#define ZOOKEEPER_STATUS                "/usr/local/zookeeper/bin/zkServer.sh status"
#define ZOOKEEPER_CFG                   "/usr/local/zookeeper/conf/zoo.cfg"
#define ZOOKEEPER_SAMPLE_CFG            "/usr/local/zookeeper/conf/zoo_sample.cfg"

struct mylist{
    int32_t count;
    char **data;
    int64_t mzxid[40960];
};

extern zhandle_t* zhEngine;
extern struct String_vector myGroThreshold;

extern void isLeader();
extern void* watchGetThread();
extern int init_check_zknode(zhandle_t *zkhandle);
extern void getChildren(char *str);
extern void get_config_policy(char *str);
extern void wexists(zhandle_t *zkhandle, char *path, char *ctx);

extern void zkadd_watcher_g(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
extern void zk_gp_watcher(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
extern void zkpolicy_watcher_g(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
extern void watch_injectroute(zhandle_t* zh, int type, int state, const char* path, void* watcherCtx);
extern void inject_child_node_event(int rc, const struct String_vector *strings, const struct Stat *stat, const void *data);

extern void get_group_policy(char *path, struct mylist* str);
extern void get_group_threshold(char *path);
extern char* get_threshold(char* host, int timeout, char *path);
extern void get_global(char *str);
extern char* get_global_threshol(char *str, void (*wfun)(zhandle_t*, int, int, const char*, void*), void (*dfun)(int, const char*, int, const struct Stat*, const void*));

extern void parseInject(const char *pMsg);
extern void parseMac(char *pMsg);
//extern void parseInterface(const char *pMsg);
extern void parseGlobal(char *pMsg);
extern void parsePolicy(const char* pMsg);
extern void parseGlobalPolicy(char *pMsg);
extern void parseGroupThreshold(char* pMsg, char *path);
extern void parseGlobalThreshold(char *pMsg);

extern void change_group_policy(char *path);
