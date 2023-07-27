#ifndef compiler_h
#define compiler_h

#include "types.h"

typedef enum {
	CompilerMode_build_objectFile,
	CompilerMode_build_binary,
	CompilerMode_run,
	CompilerMode_compilerTesting
} CompilerMode;

char *readFile(const char *path);

char *getJsmnString(char *buffer, jsmntok_t *t, int start, int count, char * key);

void compileModule(char *path, char *target_triple, CompilerMode compilerMode, CharAccumulator *LLVMsource, char *LLC_path, char *clang_path);

#endif /* compiler_h */
