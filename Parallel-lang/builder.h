#ifndef builder_h
#define builder_h

#include "types.h"

#define maxBuilderLevel 10

typedef struct {
	CharAccumulator *topLevelSource;
	
	int stringCount;
} GlobalBuilderInformation;

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype);

char *buildLLVM(GlobalBuilderInformation *globalBuilderInformation, linkedList_Node **variables, int level, CharAccumulator *outerSource, SubString *outerName, linkedList_Node *expectedType, linkedList_Node *current, int withTypes, int withCommas);

#endif /* builder_h */
