#include "policyWatcher.h"
#include "parse.h"

char macIp[IP_LENGTH] = "";
char injectIp[IP_LENGTH] = "";
char injectMask[IP_LENGTH] = "";
char interfaceIp[IP_LENGTH] = "";
int globalOutLimit = 0;
//char clusterInfo[512][512] = {""};

void parseMac(char *pMsg)
{
    int i, cnt;
    char macString[LENGTH] = "";
    char macCmd[LENGTH] = "";
    cJSON *pJson = cJSON_Parse(pMsg);
    if(!pJson)
    {
        printf("Can't parse mac policy!\n");
        return;
    }
    cnt = cJSON_GetArraySize(pJson);
    for (i = 0; i < cnt; i++)
    {
        cJSON *pArrayItem = cJSON_GetArrayItem(pJson, i);
        cJSON *ip = cJSON_GetObjectItem(pArrayItem, "ip");
        memset(macString, 0, LENGTH);
        strcat(macString, ip->valuestring);
        strcat(macString, " ");
        strcpy(macIp, ip->valuestring);

        cJSON *mac = cJSON_GetObjectItem(pArrayItem, "mac");
        strcat(macString, mac->valuestring);
        strcat(macString, "\n");
        sprintf(macCmd, "/usr/local/bin/gethopmac -a %s %s", ip->valuestring, mac->valuestring);
        system(macCmd);
        memset(macCmd, 0, sizeof(macCmd));

        int fd = open(IPMAC_CONFIG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
        if((write(fd, macString, strlen(macString))) == 0)
        {
            printf("Error writing to the file.\n");
            cJSON_Delete(pJson);
            return;
        }
        close(fd);
    }
    cJSON_Delete(pJson);
}

void parseInject(const char *pMsg)
{
    int i, cnt;
    char injectString[LENGTH] = "";
    char injectCmd[LENGTH] = "";
    char nexthopStr[DATA_LENGTH] = "";
    char daemonStr[LENGTH] = "";

    cJSON * pJson = cJSON_Parse(pMsg);
    if(NULL == pJson)
    {
        return ;
    }

    cJSON *ip = cJSON_GetObjectItem(pJson, "ip");
    strcpy(injectIp, ip->valuestring);

    strcat(injectString, ip->valuestring);
    strcat(injectString, " ");

    cJSON *mask = cJSON_GetObjectItem(pJson, "mask");
    strcpy(injectMask, mask->valuestring);

    strcat(injectString, mask->valuestring);
    strcat(injectString, " ");

    cJSON *nexthop = cJSON_GetObjectItem(pJson, "nexthop");
    if(nexthop != NULL)
    {
        cnt = cJSON_GetArraySize(nexthop);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(nexthop, i);
            strcat(nexthopStr, pArrayItem->valuestring);
            strcat(nexthopStr, " ");
        }
    }
    strcat(injectString, nexthopStr);
    injectString[strlen(injectString) -1] = '\n';
    cJSON *daemon = cJSON_GetObjectItem(pJson, "daemon");
    if(daemon != NULL)
    {
        cnt = cJSON_GetArraySize(daemon);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(daemon, i);
            strcat(daemonStr, pArrayItem->valuestring);
            strcat(daemonStr, " ");
        }
    }
    sprintf(injectCmd, "/usr/local/bin/setinject -a %s %s %s", ip->valuestring, mask->valuestring, nexthopStr);
    system(injectCmd);

    int fd = open(INJECT_CONFIG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);
    if((write(fd, injectString, strlen(injectString))) == 0)
    {
        printf("Error writing to the inject conf file.\n");
        cJSON_Delete(pJson);
        return;
    }
    close(fd);
    cJSON_Delete(pJson);
}

void parseInterface(char *pMsg)
{
    int i, j, n, cnt, childNum;
    char interfaceCmd[LENGTH] = "";
    char interfaceString[LENGTH] = "";
    char list[LENGTH]="";
    int vlanFlag = 0;

    cJSON * pJson = cJSON_Parse(pMsg);
    if(NULL == pJson)
    {
        return ;
    }
    childNum = cJSON_GetArraySize(pJson);
    for(i = 0; i < childNum; i++)
    {
        cJSON *pArrayItem = cJSON_GetArrayItem(pJson, i);
        cJSON *ip = cJSON_GetObjectItem(pArrayItem, "ip");
        strcpy(interfaceIp, ip->valuestring);
        strcat(interfaceString, ip->valuestring);
        strcat(interfaceString, " ");

        cJSON *mask = cJSON_GetObjectItem(pArrayItem, "mask");
        strcat(interfaceString, mask->valuestring);
        strcat(interfaceString, " ");

        cJSON *vlanid = cJSON_GetObjectItem(pArrayItem, "vlanid");
        if(strlen(vlanid->valuestring) == 0)
        {
            vlanFlag = 0;
            strcat(interfaceString, "0 ");
        }
        else
        {
            vlanFlag = 1;
            strcat(interfaceString, vlanid->valuestring);
            strcat(interfaceString, " ");
        }
        cJSON *portlist =cJSON_GetObjectItem(pArrayItem, "portlist");
        cnt = cJSON_GetArraySize(portlist);
        for(j=0;j<cnt;j++)
        {
            cJSON *portArray = cJSON_GetArrayItem(portlist, j);
            strcat(list, portArray->valuestring);
            strcat(list, " ");
        }
        strcat(interfaceString, list);
        strcat(interfaceString, "\n");
        for(n=0;list[n];n++)
            if(list[n]=='/') list[n]=' ';

        if(vlanFlag == 0)
        {
            sprintf(interfaceCmd, "/usr/local/bin/interface -a %s %s vlan 0 port %s", ip->valuestring, mask->valuestring, list);
            system(interfaceCmd);
            printf("interface command is %s\n",interfaceCmd);
        }
        else
        {
            sprintf(interfaceCmd, "/usr/local/bin/interface -a %s %s vlan %s port %s", ip->valuestring, mask->valuestring, vlanid->valuestring, list);
            system(interfaceCmd);
            printf("interface command is %s\n",interfaceCmd);
        }
        //system(interfaceCmd);
        printf("+++++++++%s++++++++++++%s++++++\n", INTERFACE_CONFIG_FILE, interfaceString);
        writeFile(INTERFACE_CONFIG_FILE, interfaceString);
        /*int fd = open(INTERFACE_CONFIG_FILE, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        if ((write(fd, interfaceString, strlen(interfaceString))) == 0)
        {
            printf("Error writing to the interface conf file.\n");
            return;
        }
        close(fd);*/
        memset(interfaceString, 0, LENGTH);
        memset(interfaceCmd, 0, LENGTH);
        memset(list, 0, LENGTH);
    }
    cJSON_Delete(pJson);
}

void parseGlobalPolicy(char *pMsg)
{
    int i ,cnt;
    int methods = 0, methods2 = 0;
    char globalString[5000] = "";
    char icmp[LENGTH]="";

    cJSON * pJson = cJSON_Parse(pMsg);
    if(NULL == pJson)
    {
        printf("global policy: %s\n", pMsg);
        printf("parse global policy error\n");
        return ;
    }
    strcat(globalString, "/usr/local/bin/setfilter 5 1\n");
    
    cJSON *pSubLowTime = cJSON_GetObjectItem(pJson, "low_trust_time");
    if(pSubLowTime != NULL)
    {
        char lowTime[LENGTH] = "";
        sprintf(lowTime, "/usr/local/bin/setflag -q 1 0 %s\n", pSubLowTime->valuestring);
        strcat(globalString, lowTime);     
    }

    cJSON * pSubUdp = cJSON_GetObjectItem(pJson, "udp");
    if(pSubUdp != NULL)
    {
        int threshold;
        char udp[LENGTH]="";

        cJSON * udpThreshold = cJSON_GetObjectItem(pSubUdp, "threshold");
        threshold = atoi(udpThreshold->valuestring) * ZOOM;
        sprintf(udp, "/usr/local/bin/globalpolicy -w udp1=%d\n", threshold);
        strcat(globalString, udp);
        
        cJSON *udpLen = cJSON_GetObjectItem(pSubUdp, "udp_len");
        if(udpLen != NULL)
        {
            char packet[LENGTH] = "";
            sprintf(packet, "/usr/local/bin/setudp -u %s\n", udpLen->valuestring);             
            strcat(globalString, packet);
        }
        cJSON *udpEnable = cJSON_GetObjectItem(pSubUdp, "enable");
        cJSON *udpLimit = cJSON_GetObjectItem(pSubUdp, "limit");
        if(udpLimit != NULL)
        {
            char udpLimitString[LENGTH] = "";
            cJSON * udpLimitDipRate = cJSON_GetObjectItem(udpLimit, "dip_rate");     
            cJSON * udpLimitEnable = cJSON_GetObjectItem(udpLimit, "enable");
            cJSON * udpLimitDip = cJSON_GetObjectItem(udpLimit, "dip");
            cJSON * udpLimitSip = cJSON_GetObjectItem(udpLimit, "sip");
            cJSON * udpLimitSipRate = cJSON_GetObjectItem(udpLimit, "sip_rate");
           
            sprintf( udpLimitString, "/usr/local/bin/setflag -j %s %s %s %s\n", udpLimitSip->valuestring, udpLimitSipRate->valuestring, udpLimitDip->valuestring, udpLimitDipRate->valuestring);
            strcat(globalString, udpLimitString); 
            /*char udpLimitString[LENGTH]="";
            char value[LENGTH]="";
            cJSON * udpLimitEnable = cJSON_GetObjectItem(udpLimit, "enable");
            cJSON *udpLimitValue = cJSON_GetObjectItem(udpLimit, "value");

            cJSON *udpLimitSip = cJSON_GetObjectItem(udpLimit, "sip");
            sprintf(udpLimitString, "/usr/local/bin/setflag -j %s %s 0 1\n", udpLimitSip->valuestring, udpLimitValue->valuestring);
            strcat(globalString, udpLimitString);
            cJSON *udpLimitDport = cJSON_GetObjectItem(udpLimit, "dport");
            cJSON *udpLimitSport = cJSON_GetObjectItem(udpLimit, "sport");
            cJSON *udpLimitDip = cJSON_GetObjectItem(udpLimit, "dip");*/
        }

        cJSON *udpMethods = cJSON_GetObjectItem(pSubUdp, "methods");
        if(udpMethods != NULL)
        {
            cnt = cJSON_GetArraySize(udpMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(udpMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                        methods = 1;
                if(!strcmp(pArrayItem->valuestring, "2"))
                    methods2 = 2;
            }
            if(!strcmp(udpEnable->valuestring, "1"))
                strcat(globalString, "/usr/local/bin/setfilter 3 1\n");
            else
                strcat(globalString, "/usr/local/bin/setfilter 3 64\n");

            if(methods2 == 2 && !strcmp(udpEnable->valuestring, "1"))
                strcat(globalString, "/usr/local/bin/setflag -a 97\n");
            else
                strcat(globalString, "/usr/local/bin/setflag -a 96\n");
        }
    }
    cJSON * pSubHttp = cJSON_GetObjectItem(pJson, "http");
    if( pSubHttp != NULL)
    {
        char http[LENGTH]="";
        cJSON * httpThreshold = cJSON_GetObjectItem(pSubHttp, "threshold");
        if(NULL != httpThreshold)
        {
            sprintf(http, "/usr/local/bin/setcc -s 2 %s\n", httpThreshold->valuestring);
            strcat(globalString, http);
            memset(http, 0, LENGTH);

            int threshold;
            threshold = atoi(httpThreshold->valuestring)*ZOOM;
            sprintf(http, "/usr/local/bin/setcc -s 2 %d\n", threshold);
            strcat(globalString, http);
        }
        cJSON *httpEnable = cJSON_GetObjectItem(pSubHttp, "enable");
        cJSON *httpMethods = cJSON_GetObjectItem(pSubHttp, "methods");
        if(httpMethods != NULL)
        {
            char methods[LENGTH]="";
            cnt = cJSON_GetArraySize(httpMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(httpMethods, i);
                if(i == 0 && !strcmp(httpEnable->valuestring, "1"))
                {
                    sprintf(methods, "/usr/local/bin/setcc -s 1 %s\n", pArrayItem->valuestring);
                    strcat(globalString, methods);
                }
                else
                {
                    sprintf(methods, "/usr/local/bin/setcc -s 1 0\n");
                    strcat(globalString, methods);
                }
            }
        }
        cJSON *httpPort = cJSON_GetObjectItem(pSubHttp, "port");
        char port[LENGTH]="";
        sprintf(port, "/usr/local/bin/setcc -s 6 %s\n", httpPort->valuestring);
        strcat(globalString, port);
    }

    cJSON * pSubHttps = cJSON_GetObjectItem(pJson, "https");
    if(pSubHttps != NULL)
    {
        char threshold[LENGTH]="", enable [LENGTH]="", https[LENGTH]="";
        cJSON * pSubSubHttps = cJSON_GetObjectItem(pSubHttps, "threshold");
        if(NULL != pSubSubHttps)
        {
            strcpy(threshold, pSubSubHttps->valuestring);
        }
        pSubSubHttps = cJSON_GetObjectItem(pSubHttps, "enable");
        strcpy(enable, pSubSubHttps->valuestring);
        pSubSubHttps = cJSON_GetObjectItem(pSubHttps, "port");
        sprintf(https, "/usr/local/bin/setapp -https -a %s %s %s 0\n", enable, threshold, pSubSubHttps->valuestring);
        strcat(globalString, https);

        cJSON *httpsMethods = cJSON_GetObjectItem(pSubHttps, "methods");
        if(httpsMethods != NULL)
        {
            cnt = cJSON_GetArraySize(httpsMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(httpsMethods, i);
            }
        }
    }

    cJSON * pSubAck = cJSON_GetObjectItem(pJson, "ack");
    if( pSubAck != NULL)
    {
        cJSON * ackThreshold = cJSON_GetObjectItem(pSubAck, "threshold");
        if(NULL != ackThreshold)
        {
            int threshold;
            char ack[LENGTH]="";
            threshold = atoi(ackThreshold->valuestring)*ZOOM;

            sprintf(ack, "/usr/local/bin/globalpolicy -w ack1=%d\n", threshold);
            strcat(globalString, ack);
        }
        cJSON *ackEnable = cJSON_GetObjectItem(pSubAck, "enable");

        cJSON *ackMethods = cJSON_GetObjectItem(pSubAck, "methods");
        if(ackMethods != NULL)
        {
            cnt = cJSON_GetArraySize(ackMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(ackMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                        methods = 1;
                if(!strcmp(pArrayItem->valuestring, "2"))
                        methods2 = 2;
            }
        }
        if(!strcmp(ackEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setfilter 2 1\n");
        else
            strcat(globalString, "/usr/local/bin/setfilter 2 64\n");
        if(methods == 2 && !strcmp(ackEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setflag -E 1\n");
        else
            strcat(globalString, "/usr/local/bin/setflag -E 0\n");
    }

    cJSON *pSubTrust = cJSON_GetObjectItem(pJson, "flagset_trustip");
    if(pSubTrust != NULL)
    {
        char trust[LENGTH] = "";
        cJSON * trustEnable = cJSON_GetObjectItem(pSubTrust, "enable");
        cJSON * trustSynpps = cJSON_GetObjectItem(pSubTrust, "syn_pps");
        cJSON * trustOtherpps = cJSON_GetObjectItem(pSubTrust, "other_pps");
        cJSON * trustPolicy = cJSON_GetObjectItem(pSubTrust, "policy");
        cJSON * trustEmptyconn = cJSON_GetObjectItem(pSubTrust, "empty_conn");
        cJSON * trustCheck = cJSON_GetObjectItem(pSubTrust, "check_interval");
        sprintf(trust, "/usr/local/bin/iptrust -a %s %s\n/usr/local/bin/iptrust -b %s\n/usr/local/bin/iptrust -o %s\n/usr/local/bin/iptrust -t %s %s\n", trustSynpps->valuestring, trustOtherpps->valuestring, trustEnable->valuestring, trustPolicy->valuestring, trustEmptyconn->valuestring, trustCheck->valuestring);
        strcat(globalString, trust);
    }

    cJSON * pSubSyn = cJSON_GetObjectItem(pJson, "syn");
    if(pSubSyn != NULL)
    {
        cJSON * synThreshold = cJSON_GetObjectItem(pSubSyn, "threshold");
        if(NULL != synThreshold)
        {
            int threshold;
            char syn[LENGTH]="";
            threshold = atoi(synThreshold->valuestring)*ZOOM;
            sprintf(syn, "/usr/local/bin/globalpolicy -w syn1=%d\n", threshold);
            strcat(globalString, syn);
        }
        cJSON *synEnable = cJSON_GetObjectItem(pSubSyn, "enable");
        cJSON *synLimit = cJSON_GetObjectItem(pSubSyn, "limit");
        if(synLimit != NULL)
        {
            char synEnable[LENGTH]="", synValue[LENGTH]="";
            int value, dportflag, dipflag;
            cJSON * synLimitEnable = cJSON_GetObjectItem(synLimit, "enable");
            sprintf(synEnable, "/usr/local/bin/setflag -r %s\n", synLimitEnable->valuestring);
            strcat(globalString, synEnable);

            cJSON *synLimitValue = cJSON_GetObjectItem(synLimit, "value");
            value = atoi(synLimitValue->valuestring)*ZOOM;
            sprintf(synValue, "/usr/local/bin/setflag -k %d\n", value);
            strcat(globalString, synValue);
            cJSON *synLimitSip = cJSON_GetObjectItem(synLimit, "sip");
            cJSON *synLimitDport = cJSON_GetObjectItem(synLimit, "dport");
            if(!strcmp(synLimitDport->valuestring, "1"))
                dportflag=1;

            cJSON *synLimitSport = cJSON_GetObjectItem(synLimit, "sport");
            cJSON *synLimitDip = cJSON_GetObjectItem(synLimit, "dip");
            if(!strcmp(synLimitDip->valuestring, "1"))
                dipflag=1;

            if(dportflag == 1 && dipflag ==1)
                strcat(globalString, "/usr/local/bin/setflag -d 0\n");
            else if(dipflag == 1)
                strcat(globalString, "/usr/local/bin/setflag -d 1\n");
        }
        cJSON *synMethods = cJSON_GetObjectItem(pSubSyn, "methods");
        if(synMethods != NULL)
        {
            cnt = cJSON_GetArraySize(synMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(synMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                    methods = 1;
                if(!strcmp(pArrayItem->valuestring, "11"))
                    methods2 = 11;
            }
        }
        if(!strcmp(synEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setfilter 1 1\n");
        else
            strcat(globalString, "/usr/local/bin/setfilter 1 64\n");

        if(methods2 == 11 && !strcmp(synEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setflag -G 1\n");
        else
            strcat(globalString, "/usr/local/bin/setflag -G 0\n");
    }

    cJSON * pSubOutLimit = cJSON_GetObjectItem(pJson, "outlimit");
    globalOutLimit = atoi(pSubOutLimit->valuestring);

    cJSON * pSubDns = cJSON_GetObjectItem(pJson, "dns");
    cJSON * pSubSubDns = cJSON_GetObjectItem(pSubDns, "threshold");
    if(NULL != pSubSubDns)
    {
        int threshold;char dns[LENGTH]="";
        threshold = atoi(pSubSubDns->valuestring)*ZOOM;
        sprintf(dns, "/usr/local/bin/globalpolicy -w dns_echo_limit=%d\n", threshold);
        strcat(globalString, dns);
    }
    pSubSubDns = cJSON_GetObjectItem(pSubDns, "enable");
    cJSON *dnsMethods = cJSON_GetObjectItem(pSubDns, "methods");
    if(dnsMethods != NULL)
    {
        char methods[LENGTH]="";
        cnt = cJSON_GetArraySize(dnsMethods);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(dnsMethods, i);
            if(i == 0)
            {
                sprintf(methods, "/usr/local/bin/setflag -u %s\n",pArrayItem->valuestring);
                strcat(globalString, methods);
            }
        }
    }
    pSubSubDns = cJSON_GetObjectItem(pSubDns, "port");

    cJSON * pSubLhttp = cJSON_GetObjectItem(pJson, "lhttp");
    cJSON * lhttpThreshold = cJSON_GetObjectItem(pSubLhttp, "threshold");
    int threshold;char lhttp[LENGTH]="";

    threshold = atoi(lhttpThreshold->valuestring)*ZOOM;
    sprintf(lhttp, "/usr/local/bin/setcc -s 13 %d\n", threshold);
    strcat(globalString, lhttp);
    memset(lhttp, 0, LENGTH);

    cJSON *lhttpEnable = cJSON_GetObjectItem(pSubLhttp, "enable");
    cJSON *lhttpPort = cJSON_GetObjectItem(pSubLhttp, "port");
    sprintf(lhttp, "/usr/local/bin/setcc -s 12 %s\n", lhttpPort->valuestring);
    strcat(globalString, lhttp);

    cJSON *portMethods = cJSON_GetObjectItem(pSubLhttp, "methods");
    if(portMethods != NULL)
    {
        cnt = cJSON_GetArraySize(portMethods);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(portMethods, i);
            if(!strcmp(pArrayItem->valuestring, "1"))
                methods = 1;
        }
        if(!strcmp(lhttpEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setcc -s 14 1\n");
        else
            strcat(globalString, "/usr/local/bin/setcc -s 14 64\n");
    }

    cJSON * pSubIcmp = cJSON_GetObjectItem(pJson, "icmp");
    cJSON * icmpThreshold = cJSON_GetObjectItem(pSubIcmp, "threshold");
    sprintf(icmp, "/usr/local/bin/globalpolicy -w icmp1=%d\n", atoi(icmpThreshold->valuestring) * ZOOM);
    strcat(globalString, icmp);

    cJSON *icmpEnable = cJSON_GetObjectItem(pSubIcmp, "enable");
    cJSON *icmpMethods = cJSON_GetObjectItem(pSubIcmp, "methods");
    if(icmpMethods != NULL)
    {
        cnt = cJSON_GetArraySize(icmpMethods);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(icmpMethods, i);
            if(!strcmp(pArrayItem->valuestring, "1"))
                methods = 1;
        }
        if(!strcmp(icmpEnable->valuestring, "1"))
            strcat(globalString, "/usr/local/bin/setfilter 4 1\n");
        else
            strcat(globalString, "/usr/local/bin/setfilter 4 64\n");
    }
    
    cJSON *pSubAdvancedTime = cJSON_GetObjectItem(pJson, "advanced_trust_time");
    if(pSubAdvancedTime != NULL)
    {
        char advancedTime[LENGTH] = "";
        sprintf(advancedTime, "/usr/local/bin/setflag -q 1 5 %s\n", pSubAdvancedTime->valuestring);
        strcat(globalString, advancedTime);
    }
    /*cJSON * pSubOutLimit = cJSON_GetObjectItem(pJson, "outlimit");
    globalOutLimit = atoi(pSubOutLimit->valuestring);

    cJSON *pSubTrust = cJSON_GetObjectItem(pJson, "flagset_trustip");
    if(pSubTrust != NULL)
    {
        char trust[LENGTH] = "";
        cJSON * trustEnable = cJSON_GetObjectItem(pSubTrust, "enable");
        cJSON * trustSynpps = cJSON_GetObjectItem(pSubTrust, "syn_pps");
        cJSON * trustOtherpps = cJSON_GetObjectItem(pSubTrust, "other_pps");
        cJSON * trustPolicy = cJSON_GetObjectItem(pSubTrust, "policy");
        cJSON * trustEmptyconn = cJSON_GetObjectItem(pSubTrust, "empty_conn");
        cJSON * trustCheck = cJSON_GetObjectItem(pSubTrust, "check_interval");
        sprintf(trust, "/usr/local/bin/iptrust -a %s %s\n/usr/local/bin/iptrust -b %s\n/usr/local/bin/iptrust -o %s\n/usr/local/bin/iptrust -t %s %s\n", trustSynpps->valuestring, trustOtherpps->valuestring, trustEnable->valuestring, trustPolicy->valuestring, trustEmptyconn->valuestring, trustCheck->valuestring);
        strcat(globalString, trust);
    }*/

    int fd = open(GLOBAL_POLICY, O_RDWR | O_CREAT, S_IRWXU);
    if((write(fd, globalString, strlen(globalString))) == 0)
    {
        printf("Error writing to the file.\n");
        return;
    }
    close(fd);
    cJSON_Delete(pJson);
}

void parsePolicy(const char* pMsg)
{
    int i ,cnt, fd;
    int tcpAckMethods = 0;
    int tcpSynMethods = 0;
    int tcpSynLimit = 0;

    char slowatkString[LENGTH] = "";char postString[LENGTH] = "";
    char flagSetTcp[LENGTH] = "";
    char writeString[LENGTH] = "";char configName[LENGTH] = "";
    char groupParser[LENGTH] = "";char groupString[LENGTH] = "";
    char udpString[LENGTH] = "";char httpString[LENGTH] = "";
    char httpsString[LENGTH] = "";char ackString[LENGTH] = "";
    char synString[LENGTH] = "";char dnsString[LENGTH] = "";
    char outLimit[LENGTH] = "";char group_ip[LENGTH] = "";
    char groupIpCmd[LENGTH] = "";char nameString[LENGTH] = "";
    char udpConfig[DATA_LENGTH] = "";char ccConfig[DATA_LENGTH] = "";

    char baseString[LENGTH] = "-ddos CONN 1 64\n-ddos MAX_FLOW 1 0 1000 64\n-syncookieurl 1\n-sip 1 1 0 1 65535 5060\n-re_enable 1\n";

    sprintf(outLimit, " %d", globalOutLimit);
    cJSON * pJson = cJSON_Parse(pMsg);
    if(NULL == pJson)
    {
        traceEvent("Parse group policy ", "format", "ERROR");
        return ;
    }

    cJSON *groupIp = cJSON_GetObjectItem(pJson, "groupIp");
    if(groupIp != NULL)
    {
        cnt = cJSON_GetArraySize(groupIp);
        for (i = 0; i < cnt; i++)
        {
            cJSON *pArrayItem = cJSON_GetArrayItem(groupIp, i);
            if(INADDR_NONE != inet_addr(pArrayItem->valuestring))
            {
                char tmpBuf[LENGTH] = "";
                sprintf(tmpBuf, "%s-%s", pArrayItem->valuestring, pArrayItem->valuestring);
                memset(pArrayItem->valuestring, 0, strlen(pArrayItem->valuestring));
                strcpy(pArrayItem->valuestring, tmpBuf);
            }
            strcat(groupIpCmd, pArrayItem->valuestring);
            strcat(groupIpCmd, " ");
        }
        sprintf(group_ip,"-group_ip %s\n", groupIpCmd);
    }

    cJSON *groupId = cJSON_GetObjectItem(pJson, "groupId");
    if(groupId != NULL)
    {
        sprintf(nameString, "-name %s\n", groupId->valuestring);
    }

    sprintf(configName, "%s/%s", GROUP_POLICY_FILE_DIR, groupId->valuestring);
    fd = open(configName, /*O_RDWR | O_CREAT*/O_WRONLY | O_CREAT, S_IRWXU);
    if((write(fd, baseString, strlen(baseString))) == 0)
    {
        traceEvent("write to file faild ", configName, "ERROR");
        return ;
    }
    if((write(fd, nameString, strlen(nameString))) == 0)
    {
        traceEvent("write to file faild ", configName, "ERROR");
        return;
    }
    if((write(fd, group_ip, strlen(group_ip))) == 0)
    {
        traceEvent("write to file faild ", configName, "ERROR");
        return;
    }

    cJSON *policy = cJSON_GetObjectItem(pJson, "policy");
    if(policy == NULL)
    {
        cJSON_Delete(pJson);
        traceEvent("Parse group policy ", "format", "ERROR");
        return;
    }
    cJSON * pSubUdp = cJSON_GetObjectItem(policy, "udp");
    if( pSubUdp != NULL)
    {
        int methods = 0, methods2 = 0;
        /*char limit1[5] = "";
        char limit2[5] = "";
        char limit3[5] = "";
        char limit4[5] = "";*/

        cJSON *udpLimitDipRate;
        cJSON *udpLimitEnable;
        cJSON *udpLimitDip;
        cJSON *udpLimitSip;
        cJSON *udpLimitSipRate;

        char flagSetUdp[LENGTH] = "";
        cJSON * udpThreshold = cJSON_GetObjectItem(pSubUdp, "threshold");
        cJSON * udpUdpLen = cJSON_GetObjectItem(pSubUdp, "udp_len");
        cJSON *udpEnable = cJSON_GetObjectItem(pSubUdp, "enable");
        if(udpEnable != NULL)
            sprintf(udpString, "-ddos UDP 1 ");
        if(udpThreshold != NULL)
            strcat(udpString, udpThreshold->valuestring);

        cJSON *udpLimit = cJSON_GetObjectItem(pSubUdp, "limit");
        if(udpLimit != NULL)
        {
            /*cJSON *udpLimitEnable = cJSON_GetObjectItem(udpLimit, "enable");
            cJSON *udpLimitValue = cJSON_GetObjectItem(udpLimit, "value");
            cJSON *udpLimitSip = cJSON_GetObjectItem(udpLimit, "sip");
            cJSON *udpLimitDport = cJSON_GetObjectItem(udpLimit, "dport");
            cJSON *udpLimitSport = cJSON_GetObjectItem(udpLimit, "sport");
            cJSON *udpLimitDip = cJSON_GetObjectItem(udpLimit, "dip");
            */         

            udpLimitDipRate = cJSON_GetObjectItem(udpLimit, "dip_rate");
            udpLimitEnable = cJSON_GetObjectItem(udpLimit, "enable");
            udpLimitDip = cJSON_GetObjectItem(udpLimit, "dip");
            udpLimitSip = cJSON_GetObjectItem(udpLimit, "sip");
            udpLimitSipRate = cJSON_GetObjectItem(udpLimit, "sip_rate");
            
            /*if(!strcmp(udpLimitSip->valuestring, "1") && !strcmp(udpLimitSport->valuestring, "1"))
                sprintf(limit1, "1");
            else
                sprintf(limit1, "0");

            if(!strcmp(udpLimitSip->valuestring, "1") && strcmp(udpLimitSport->valuestring, "1"))
                sprintf(limit2, "1");
            else
                sprintf(limit2, "0");

            if(!strcmp(udpLimitDip->valuestring, "1") && !strcmp(udpLimitDport->valuestring, "1"))
                sprintf(limit3, "1");
            else
                sprintf(limit3, "0");

            if(!strcmp(udpLimitDip->valuestring, "1") && strcmp(udpLimitDport->valuestring, "1"))
                sprintf(limit4, "1");
            else
                sprintf(limit4, "0");
            */
        }
        cJSON *udpMethods = cJSON_GetObjectItem(pSubUdp, "methods");
        if(udpMethods != NULL)
        {
            cnt = cJSON_GetArraySize(udpMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(udpMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                    methods = 1;
                if(!strcmp(pArrayItem->valuestring, "2"))
                    methods2 = 1;
            }
            if(methods == 1 && !strcmp(udpEnable->valuestring, "1"))
            {
                strcat(udpString, " 1\n");
                sprintf(flagSetUdp, "-flagset_udp 1 1 0 80\n");//, udpPacket->valuestring);
            }
            else
            {
                strcat(udpString, " 64\n");
                sprintf(flagSetUdp, "-flagset_udp 1 0 0 80\n");// udpPacket->valuestring);
            }
            if(methods2 == 1 && !strcmp(udpEnable->valuestring, "1"))
                sprintf(udpConfig, "-udp 1 1 0 %s 1 %s %s %s %s\n", udpUdpLen->valuestring, udpLimitSip->valuestring, udpLimitSipRate->valuestring, udpLimitDip->valuestring, udpLimitDipRate->valuestring);
                //sprintf(udpConfig, "-udp 1 1 0 65535 1 %s 65535 %s 3000000 %s 65535 %s 3000000\n", limit1, limit2, limit3, limit4);
            else
                sprintf(udpConfig, "-udp 1 0 0 %s 1 %s %s %s %s\n", udpUdpLen->valuestring, udpLimitSip->valuestring, udpLimitSipRate->valuestring, udpLimitDip->valuestring, udpLimitDipRate->valuestring);
                //sprintf(udpConfig, "-udp 1 0 0 65535 1 %s 65535 %s 3000000 %s 65535 %s 3000000\n", limit1, limit2, limit3, limit4);

            if((write(fd, udpConfig, strlen(udpConfig))) == 0)
            {
                traceEvent("write to file faild ", configName, "ERROR");
                return;
            }
            if((write(fd, udpString, strlen(udpString))) == 0)
            {
                traceEvent("write to file faild ", configName, "ERROR");
                return;
            }
            if((write(fd, flagSetUdp, strlen(flagSetUdp))) == 0)
            {
                traceEvent("write to file faild ", configName, "ERROR");
                return;
            }
        }
    }
    cJSON * pSubHttp = cJSON_GetObjectItem(policy, "http");
    if( pSubHttp != NULL)
    {
        char ccMetholds[5] = "";
        cJSON *httpThreshold = cJSON_GetObjectItem(pSubHttp, "threshold");

        cJSON *httpEnable = cJSON_GetObjectItem(pSubHttp, "enable");
        cJSON *httpMethods = cJSON_GetObjectItem(pSubHttp, "methods");
        if(httpMethods != NULL)
        {
            cnt = cJSON_GetArraySize(httpMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(httpMethods, i);
                if(i==0)
                    strcpy(ccMetholds, pArrayItem->valuestring);
            }
        }
        cJSON *httpPort = cJSON_GetObjectItem(pSubHttp, "port");
        sprintf(ccConfig, "-cc 1 %s %s %s %s 0 qq\n", httpThreshold->valuestring, httpPort->valuestring, httpEnable->valuestring, ccMetholds);
        if((write(fd, ccConfig, strlen(ccConfig))) == 0)
        {
            traceEvent("write to file faild ", configName, "ERROR");
            return;
        }
    }
    cJSON * pSubHttps = cJSON_GetObjectItem(policy, "https");
    if( pSubHttps != NULL)
    {
        char methods[LENGTH] = "";
        char port[LENGTH] = "";

        cJSON * httpsThreshold = cJSON_GetObjectItem(pSubHttps, "threshold");

        cJSON *httpsEnable = cJSON_GetObjectItem(pSubHttps, "enable");
        if(httpsEnable != NULL)
            sprintf(httpsString, "-https 1 %s ", httpsEnable->valuestring);
        if(httpsThreshold != NULL)
            strcat(httpsString, httpsThreshold->valuestring);
        cJSON *httpsPort = cJSON_GetObjectItem(pSubHttps, "port");
        if(httpsPort != NULL)
            sprintf(port, " %s 0\n", httpsPort->valuestring);
        strcat(httpsString, port);

        if((write(fd, httpsString, strlen(httpsString))) == 0)
        {
            traceEvent("write to file faild ", configName, "ERROR");
            return;
        }
        cJSON *httpsMethods = cJSON_GetObjectItem(pSubHttps, "methods");
        if(httpsMethods != NULL)
        {
            cnt = cJSON_GetArraySize(httpsMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(httpsMethods, i);
            }
        }
    }
    cJSON * pSubAck = cJSON_GetObjectItem(policy, "ack");
    if( pSubAck != NULL)
    {
        int methods = 0;
        cJSON * ackThreshold = cJSON_GetObjectItem(pSubAck, "threshold");
        if(ackThreshold != NULL)
        {
            sprintf(ackString, "-ddos ACK 1 %s", ackThreshold->valuestring);
        }
        cJSON *ackEnable = cJSON_GetObjectItem(pSubAck, "enable");
        cJSON *ackMethods = cJSON_GetObjectItem(pSubAck, "methods");
        if(ackMethods != NULL)
        {
            cnt = cJSON_GetArraySize(ackMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(ackMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                    methods = 1;
                if(!strcmp(pArrayItem->valuestring, "2"))
                    tcpAckMethods = 1;
            }
            if(methods == 1 && !strcmp(ackEnable->valuestring, "1"))
                strcat(ackString, " 1\n");
            else
                strcat(ackString, " 64\n");
        }
        if((write(fd, ackString, strlen(ackString))) == 0)
        {
            traceEvent("write to file faild ", configName, "ERROR");
            return;
        }
    }

    cJSON *pSubTrust = cJSON_GetObjectItem(policy, "flagset_trustip");
    if(pSubTrust != NULL)
    {
        char flagset_trust[LENGTH] = "";
        cJSON * trustEnable = cJSON_GetObjectItem(pSubTrust, "enable");
        cJSON * trustSynpps = cJSON_GetObjectItem(pSubTrust, "syn_pps");
        cJSON * trustOtherpps = cJSON_GetObjectItem(pSubTrust, "other_pps");
        cJSON * trustPolicy = cJSON_GetObjectItem(pSubTrust, "policy");
        cJSON * trustEmptyconn = cJSON_GetObjectItem(pSubTrust, "empty_conn");
        cJSON * trustCheck = cJSON_GetObjectItem(pSubTrust, "check_interval");
        sprintf(flagset_trust, "-flagset_trust 1 %s %s %s %s %s %s\n", trustEnable->valuestring, trustPolicy->valuestring, trustSynpps->valuestring, trustOtherpps->valuestring, trustEmptyconn->valuestring, trustCheck->valuestring);
        if((write(fd, flagset_trust, strlen(flagset_trust))) == 0)
        {
            printf("Error writing to the file group policy config\n");
            return;
        } 
    }

    cJSON * pSubSyn = cJSON_GetObjectItem(policy, "syn");
    if(pSubSyn != NULL)
    {
        int methods = 0;
        cJSON *synLimitEnable;cJSON *synLimitValue;
        cJSON *synLimitSip;cJSON *synLimitDport;
        cJSON *synLimitSport;cJSON *synLimitDip;

        cJSON *synThreshold = cJSON_GetObjectItem(pSubSyn, "threshold");
        cJSON *synEnable = cJSON_GetObjectItem(pSubSyn, "enable");
        if(synEnable != NULL)
            sprintf(synString, "-ddos SYN 1 ");

        if(synThreshold != NULL)
            strcat(synString, synThreshold->valuestring);

        cJSON *synLimit = cJSON_GetObjectItem(pSubSyn, "limit");
        if(synLimit != NULL)
        {
            synLimitEnable = cJSON_GetObjectItem(synLimit, "enable");
            synLimitValue = cJSON_GetObjectItem(synLimit, "value");
            synLimitSip = cJSON_GetObjectItem(synLimit, "sip");
            synLimitDport = cJSON_GetObjectItem(synLimit, "dport");
            synLimitSport = cJSON_GetObjectItem(synLimit, "sport");
            synLimitDip = cJSON_GetObjectItem(synLimit, "dip");

            if(!strcmp(synLimitDip->valuestring, "1") && strcmp(synLimitDport->valuestring, "1"))
                tcpSynLimit = 0;
            if(!strcmp(synLimitDip->valuestring, "1"))
                tcpSynLimit = 1;
        }
        cJSON *synMethods = cJSON_GetObjectItem(pSubSyn, "methods");
        if(synMethods != NULL)
        {
            cnt = cJSON_GetArraySize(synMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(synMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                    methods = 1;
                if(!strcmp(pArrayItem->valuestring, "11"))
                    tcpSynMethods = 1;
            }
            if(methods == 1 && !strcmp(synEnable->valuestring, "1"))
            {
                strcat(synString, outLimit);
                strcat(synString, " 1 1\n");
            }
            else
            {
                strcat(synString, outLimit);
                strcat(synString, " 64 1\n");
            }
        }
        if(tcpSynMethods ==1 && !strcmp(synEnable->valuestring, "1"))
            sprintf(flagSetTcp, "-flagset_tcp 1 %s %s 0 %d %d 8 24 10000\n", synLimitEnable->valuestring, synLimitValue->valuestring, tcpSynLimit, tcpAckMethods);
        else
            sprintf(flagSetTcp, "-flagset_tcp 1 %s %s 1 %d %d 8 24 10000\n", synLimitEnable->valuestring, synLimitValue->valuestring, tcpSynLimit, tcpAckMethods);
        if((write(fd, flagSetTcp, strlen(flagSetTcp))) == 0)
        {
            printf("Error writing to the file.\n");
            return;
        }
        if((write(fd, synString, strlen(synString))) == 0)
        {
            printf("Error writing to the file.\n");
            return;
        }
    }
    cJSON * pSubDns = cJSON_GetObjectItem(policy, "dns");
    cJSON * pSubSubDns = cJSON_GetObjectItem(pSubDns, "threshold");
    if(pSubSubDns != NULL)
    {
        cJSON *dnsEnable = cJSON_GetObjectItem(pSubDns, "enable");
        sprintf(dnsString, "-dns 1 %s 0 1 1%s\n", dnsEnable->valuestring, outLimit);

        if((write(fd, dnsString, strlen(dnsString))) == 0)
        {
            printf("Error writing to the file.\n");
            return;
        }
        cJSON *dnsMethods = cJSON_GetObjectItem(pSubDns, "methods");
        if(dnsMethods != NULL)
        {
            cnt = cJSON_GetArraySize(dnsMethods);
            for(i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(dnsMethods, i);
            }
        }
        cJSON *dnsPort = cJSON_GetObjectItem(pSubDns, "port");
    }
    cJSON * pSubLhttp = cJSON_GetObjectItem(policy, "lhttp");
    if(pSubLhttp != NULL)
    {
        cJSON * lhttpThreshold = cJSON_GetObjectItem(pSubLhttp, "threshold");
        cJSON *lhttpEnable = cJSON_GetObjectItem(pSubLhttp, "enable");
        sprintf(slowatkString, "-slowatk %s 500 %s\n", lhttpEnable->valuestring, lhttpThreshold->valuestring);
        if((write(fd, slowatkString, strlen(slowatkString))) == 0)
        {
            printf("Error writing to the file.\n");
            return;
        }

        cJSON *lhttpPort = cJSON_GetObjectItem(pSubLhttp, "port");
        sprintf(postString, "-post 1 %s %s 1 2\n", lhttpThreshold->valuestring, lhttpPort->valuestring);
        if((write(fd, postString, strlen(postString))) == 0)
        {
            printf("Error writing to the file.\n");
            return;
        }

        cJSON *portMethods = cJSON_GetObjectItem(pSubLhttp, "methods");
        if(portMethods != NULL)
        {
            cnt = cJSON_GetArraySize(portMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(portMethods, i);
            }
        }
    }
    int methosd = 0;
    cJSON * pSubIcmp = cJSON_GetObjectItem(policy, "icmp");
    if(pSubIcmp != NULL)
    {
        cJSON *icmpThreshold = cJSON_GetObjectItem(pSubIcmp, "threshold");
        cJSON *icmpEnable = cJSON_GetObjectItem(pSubIcmp, "enable");
        cJSON *icmpMethods = cJSON_GetObjectItem(pSubIcmp, "methods");
        if(icmpMethods != NULL)
        {
            char icmpString[LENGTH] = "";
            cnt = cJSON_GetArraySize(icmpMethods);
            for (i = 0; i < cnt; i++)
            {
                cJSON *pArrayItem = cJSON_GetArrayItem(icmpMethods, i);
                if(!strcmp(pArrayItem->valuestring, "1"))
                    methosd = 1;
            }
            if(methosd == 1 && !strcmp(icmpEnable->valuestring, "1"))
                sprintf(icmpString, "-ddos ICMP 1 %s 1\n", icmpThreshold->valuestring);
            else
                sprintf(icmpString, "-ddos ICMP 1 %s 64\n", icmpThreshold->valuestring);

            if((write(fd, icmpString, strlen(icmpString))) == 0)
            {
                printf("Error writing to the file group policy config\n");
                return;
            }
        }
    }

    close(fd);
    sprintf(groupParser, "/usr/local/bin/groupparser -A %s", configName);
    system(groupParser);
    cJSON_Delete(pJson);
}

