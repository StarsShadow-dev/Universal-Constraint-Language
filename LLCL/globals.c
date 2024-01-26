#include "globals.h"

struct timespec startTime = {0};

CompilerMode compilerMode;

CharAccumulator objectFiles = {100, 0, 0};

CompilerOptions compilerOptions = {0};

QueryMode queryMode;
int queryTextLength = 0;
int queryLine = 0;
int queryColumn = 0;
char *queryText = NULL;

char *LLC_path = NULL;
char *clang_path = NULL;
char *buildDirectory = NULL;
char *startFilePath = NULL;

FileInformation *coreFilePointer;

linkedList_Node *alreadyCompiledFiles = NULL;

int warningCount = 0;
