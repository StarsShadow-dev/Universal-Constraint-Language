#ifndef compiler_h
#define compiler_h

#define JSMN_HEADER
#include "jsmn.h"
#include "types.h"
#include "globals.h"

typedef enum {
	CompilerMode_build_objectFile,
	CompilerMode_build_binary,
	CompilerMode_run,
	CompilerMode_compilerTesting
} CompilerMode;

char *readFile(const char *path);

char *getJsmnString(char *buffer, jsmntok_t *t, int start, int count, char *key);

void compileModule(ModuleInformation *MI, CompilerMode compilerMode, char *path);

#endif /* compiler_h */
