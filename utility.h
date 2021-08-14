#ifndef _UTILITY_H_
#define _UTILITY_H_

extern char *replace(char * src,char oldChar,char newChar);
extern void stringtrim(char *str);
extern void cleanFile(char *path);
extern void writeFile(char *path, char* content);
extern void traceEvent(char *content, const char *name, char *type);
#endif
