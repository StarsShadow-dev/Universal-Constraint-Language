#ifndef builder_h
#define builder_h

#include "types.h"

typedef struct {
	CharAccumulator *topLevelConstantSource;
	
	int stringCount;
	
	int level;
	linkedList_Node *variables[maxVariablesLevel];
} GlobalBuilderInformation;

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype);

void buildLLVM(GlobalBuilderInformation *GBI, SubString *outerName, CharAccumulator *outerSource, CharAccumulator *innerSource, linkedList_Node *expectedTypes, linkedList_Node **types, linkedList_Node *current, int withTypes, int withCommas);

#endif /* builder_h */
