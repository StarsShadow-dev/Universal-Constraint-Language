#ifndef builder_h
#define builder_h

#include "types.h"

typedef struct {
	CharAccumulator *topLevelSource;
	CharAccumulator *topLevelConstantSource;
	
	int stringCount;
} GlobalBuilderInformation;

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype);

void buildLLVM(GlobalBuilderInformation *GBI, linkedList_Node **variables, int level, SubString *outerName, CharAccumulator *innerSource, linkedList_Node **expectedTypes, linkedList_Node **types, linkedList_Node *current, int withTypes, int withCommas);

#endif /* builder_h */
