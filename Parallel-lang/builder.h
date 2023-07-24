#ifndef builder_h
#define builder_h

#include "types.h"
#include "globals.h"

typedef struct {
	CharAccumulator *topLevelConstantSource;
	
	int stringCount;
	
	int level;
	linkedList_Node *variables[maxVariablesLevel];
} GlobalBuilderInformation;

void addBuilderType(linkedList_Node **variables, SubString *key, char *LLVMtype, int byteSize, int byteAlign);

void buildLLVM(
	GlobalBuilderInformation *GBI,
	Variable_function *outerFunction,
	CharAccumulator *outerSource,
	CharAccumulator *innerSource,
	
	/// Variable
	linkedList_Node *expectedTypes,
	/// Variable?, ASTnode?
	linkedList_Node **types,
	/// ASTnode
	linkedList_Node *current,
	
	int loadVariables,
	int withTypes,
	int withCommas
);

#endif /* builder_h */
