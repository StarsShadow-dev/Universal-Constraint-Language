#ifndef printer_h
#define printer_h

void getTypeDescription(FileInformation *FI, CharAccumulator *charAccumulator, BuilderType *type, int withFactInformation);
void getVariableDescription(FileInformation *FI, CharAccumulator *charAccumulator, ContextBinding *variableBinding);

void printError(SourceLocation location, char *msg, char *indicator);

void printKeyword(int type, char *name, char *documentation);
void printBinding(FileInformation *FI, ContextBinding *binding);

void printKeywords(FileInformation *FI);
void printBindings(FileInformation *FI);

#endif /* printer_h */
