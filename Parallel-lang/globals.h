#ifndef globals_h
#define globals_h

#include "types.h"

extern char *source;

extern linkedList_Node *tokens;

void allocateGlobals(void);

void deallocateGlobals(void);

#endif /* globals_h */
