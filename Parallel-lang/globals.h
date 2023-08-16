#ifndef globals_h
#define globals_h

#include "types.h"

#define CURRENT_VERSION "beta.5"

// 8 bytes on a 64 bit machine
#define pointer_byteSize 8

#define maxVariablesLevel 10

extern char *source;

typedef struct {
	int includeDebugInformation;
	int verbose;
} CompilerOptions;

extern CompilerOptions compilerOptions;

#endif /* globals_h */
