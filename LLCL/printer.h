#ifndef printer_h
#define printer_h

void printKeyword(int type, char *name, char *documentation);
void printBinding(ContextBinding *binding);

void printKeywords(FileInformation *FI);
void printBindings(FileInformation *FI);

#endif /* printer_h */
