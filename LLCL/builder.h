#ifndef builder_h
#define builder_h

#include "types.h"
#include "globals.h"
#include "compiler.h"

int buildLLVM(
	FileInformation *FI,
	ScopeObject_function *outerFunction,
	CharAccumulator *outerSource,
	CharAccumulator *innerSource,
	
	/// BuilderType
	linkedList_Node *expectedTypes,
	/// BuilderType
	linkedList_Node **types,
	/// ASTnode
	linkedList_Node *current,
	
	Bool loadVariables,
	Bool withTypes,
	Bool withCommas
);

#endif /* builder_h */
