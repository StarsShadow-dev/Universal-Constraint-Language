#ifndef globals_h
#define globals_h

#include "types.h"

#define CURRENT_VERSION "beta.6"

// 8 bytes on a 64 bit machine
#define pointer_byteSize 8

#define maxVariablesLevel 10

typedef struct {
	int includeDebugInformation;
	int verbose;
} CompilerOptions;

extern CompilerOptions compilerOptions;

extern char *LLC_path;
extern char *clang_path;
extern char *full_build_directory;

#endif /* globals_h */
