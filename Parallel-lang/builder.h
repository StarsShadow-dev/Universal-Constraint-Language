#ifndef builder_h
#define builder_h

#include "types.h"

#define maxBuilderLevel 10

void addBuilderVariable(linkedList_Node **variables, char *key, char *LLVMname);

char *buildLLVM(linkedList_Node **variables, int level, String *outerSource, linkedList_Node *current);

#endif /* builder_h */
