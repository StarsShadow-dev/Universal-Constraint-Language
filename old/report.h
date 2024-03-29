#ifndef report_h
#define report_h

#include "types.h"
#include "builder.h"

//
// shared
//

extern CharAccumulator reportMsg;
void addStringToReportMsg(char *string);
void addSubStringToReportMsg(SubString *subString);
void addIntToReportMsg(int64_t number);

extern CharAccumulator reportIndicator;
void addStringToReportIndicator(char *string);
void addSubStringToReportIndicator(SubString *subString);
void addIntToReportIndicator(int64_t number);

//
// warnings
//

// remember to update getWarningTypeAsString when this is changed
typedef enum {
	WarningType_format,
	WarningType_unused,
	WarningType_unneeded,
	WarningType_redundant,
	WarningType_unsafe,
	WarningType_deprecated,
	WarningType_other
} WarningType;

void compileWarning(FileInformation *FI, SourceLocation location, WarningType warningType);

//
// errors
//

void compileError(FileInformation *FI, SourceLocation location) __dead2;

#endif /* report_h */
