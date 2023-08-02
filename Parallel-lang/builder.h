#ifndef builder_h
#define builder_h

#include "types.h"
#include "globals.h"

typedef struct {
	CharAccumulator *topLevelConstantSource;
	
	int stringCount;
	
	int level;
	linkedList_Node *context[maxVariablesLevel];
} GlobalBuilderInformation;

void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign);

void buildLLVM(
	GlobalBuilderInformation *GBI,
	ContextBinding_function *outerFunction,
	CharAccumulator *outerSource,
	CharAccumulator *innerSource,
	
	/// BuilderType
	linkedList_Node *expectedTypes,
	/// BuilderType
	linkedList_Node **types,
	/// ASTnode
	linkedList_Node *current,
	
	int loadVariables,
	int withTypes,
	int withCommas
);

#endif /* builder_h */
