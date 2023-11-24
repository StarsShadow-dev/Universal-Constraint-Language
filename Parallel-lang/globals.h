#ifndef globals_h
#define globals_h

#include "types.h"

#define CURRENT_VERSION "beta.7"

// 8 bytes (on a 64 bit machine)
#define pointer_byteSize 8

typedef enum {
	CompilerMode_build_objectFile,
	CompilerMode_build_binary,
	CompilerMode_run,
	CompilerMode_compilerTesting,
	CompilerMode_check,
	CompilerMode_query
} CompilerMode;

extern char *queryPath;
extern int queryTextLength;
extern int queryLine;
extern int queryColumn;
extern char *queryText;

extern CompilerMode compilerMode;

extern CharAccumulator objectFiles;

typedef struct {
	int includeDebugInformation;
	int verbose;
} CompilerOptions;

extern CompilerOptions compilerOptions;

extern char *LLC_path;
extern char *clang_path;
extern char *full_build_directory;

extern ModuleInformation *coreModulePointer;

extern linkedList_Node *alreadyCompiledModules;

extern int warningCount;

#endif /* globals_h */
