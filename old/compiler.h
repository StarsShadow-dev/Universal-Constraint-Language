#ifndef compiler_h
#define compiler_h

#define JSMN_HEADER
#include "jsmn.h"
#include "types.h"
#include "globals.h"

char *readFile(const char *path);

char *getJsmnString(char *buffer, jsmntok_t *t, int start, int count, char *key);

void compileFile(FileInformation *FI);

#endif /* compiler_h */
