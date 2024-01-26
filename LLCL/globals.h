#ifndef globals_h
#define globals_h

#include "types.h"

#define CURRENT_VERSION "beta.7"

// 8 bytes (on a 64 bit machine)
#define pointer_byteSize 8

extern struct timespec startTime;

typedef enum {
	CompilerMode_build_objectFile,
	CompilerMode_build_binary,
	CompilerMode_run,
	CompilerMode_check,
	CompilerMode_query
} CompilerMode;

typedef enum {
	QueryMode_hover,
	QueryMode_suggestions
} QueryMode;

extern QueryMode queryMode;
extern int queryTextLength;
extern int queryLine;
extern int queryColumn;
extern char *queryText;

extern CompilerMode compilerMode;

extern CharAccumulator objectFiles;

typedef struct {
	int includeDebugInformation;
	int verbose;
	int printIR;
	int timed;
	int compilerTesting;
} CompilerOptions;

extern CompilerOptions compilerOptions;

extern char *LLC_path;
extern char *clang_path;
extern char *buildDirectory;
extern char *startFilePath;

extern FileInformation *coreFilePointer;

extern linkedList_Node *alreadyCompiledFiles;

extern int warningCount;

#endif /* globals_h */
