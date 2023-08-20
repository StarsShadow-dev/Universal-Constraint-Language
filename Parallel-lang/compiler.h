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

typedef struct {
	char *path;
	char *currentSource;
	CharAccumulator *topLevelConstantSource;
	CharAccumulator *LLVMmetadataSource;
	
	int metadataCount;
	
	int stringCount;
	
	int level;
	linkedList_Node *context[maxVariablesLevel];
	
	int debugInformationCompileUnitID;
	int debugInformationFileScopeID;
} ModuleInformation;

char *readFile(const char *path);

char *getJsmnString(char *buffer, jsmntok_t *t, int start, int count, char *key);

void compileModule(CompilerMode compilerMode, char *path);

#endif /* compiler_h */
