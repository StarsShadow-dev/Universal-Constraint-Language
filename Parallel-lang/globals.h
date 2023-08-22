#ifndef globals_h
#define globals_h

#include "types.h"
#include "compiler.h"

#define CURRENT_VERSION "beta.6"

// 8 bytes (on a 64 bit machine)
#define pointer_byteSize 8

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

#endif /* globals_h */
