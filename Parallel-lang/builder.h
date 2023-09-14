#ifndef builder_h
#define builder_h

#include "types.h"
#include "globals.h"
#include "compiler.h"

void addContextBinding_simpleType(linkedList_Node **context, char *name, char *LLVMtype, int byteSize, int byteAlign);

void addContextBinding_macro(ModuleInformation *MI, char *name);

int buildLLVM(
	ModuleInformation *MI,
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
