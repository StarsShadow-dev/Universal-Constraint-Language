#ifndef error_h
#define error_h

#include "types.h"

extern CharAccumulator errorMsg;
void addStringToErrorMsg(char *string);
void addSubStringToErrorMsg(SubString *subString);

extern CharAccumulator errorIndicator;
void addStringToErrorIndicator(char *string);
void addSubStringToErrorIndicator(SubString *subString);

void compileError(SourceLocation location) __dead2;

#endif /* error_h */
