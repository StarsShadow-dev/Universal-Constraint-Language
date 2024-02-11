#ifndef printer_h
#define printer_h

void getTypeDescription(FileInformation *FI, CharAccumulator *charAccumulator, BuilderType *type, int withFactInformation);

void printError(SourceLocation location, char *msg, char *indicator);

void printKeyword(int type, char *name, char *documentation);
void printScopeObject(FileInformation *FI, ScopeObject *scopeObject);

void printKeywords(FileInformation *FI);
void printScopeObjects(FileInformation *FI);

#endif /* printer_h */
