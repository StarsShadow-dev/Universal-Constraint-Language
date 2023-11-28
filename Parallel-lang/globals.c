#include "globals.h"

CompilerMode compilerMode;

CharAccumulator objectFiles = {100, 0, 0};

CompilerOptions compilerOptions = {};

QueryMode queryMode;
char *queryPath = NULL;
int queryTextLength = 0;
int queryLine = 0;
int queryColumn = 0;
char *queryText = NULL;
int addedQueryLocation = 0;

char *LLC_path = NULL;
char *clang_path = NULL;
char *full_build_directory = NULL;

ModuleInformation *coreModulePointer;

linkedList_Node *alreadyCompiledModules = NULL;

int warningCount = 0;
