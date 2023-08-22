#include "globals.h"

CharAccumulator objectFiles = {100, 0, 0};

CompilerOptions compilerOptions = {};

char *LLC_path = NULL;
char *clang_path = NULL;
char *full_build_directory = NULL;

ModuleInformation *coreModulePointer;
