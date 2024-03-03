#ifndef printer_h
#define printer_h

void getScopeObjectDescription(FileInformation *FI, CharAccumulator *charAccumulator, ScopeObject *scopeObject, int withFactInformation);

void printError(SourceLocation location, char *msg, char *indicator);

void printKeyword(int type, char *name, char *documentation);
void printScopeObject_alias(FileInformation *FI, ScopeObject *scopeObject);

void printKeywords(FileInformation *FI);
void printScopeObjects(FileInformation *FI);

#endif /* printer_h */
