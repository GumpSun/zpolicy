#ifndef _PARSE_H_
#define _PARSE_H_

#define IP_LENGTH        16

extern char macIp[IP_LENGTH];
extern char injectIp[IP_LENGTH];
extern char injectMask[IP_LENGTH];
extern char interfaceIp[IP_LENGTH];
extern int globalOutLimit;
//extern char clusterInfo[512][512];

extern void parseMac(char *pMsg);
extern void parseInject(const char *pMsg);
extern void parseInterface(char *pMsg);
extern void parseGlobalPolicy(char *pMsg);
#endif

