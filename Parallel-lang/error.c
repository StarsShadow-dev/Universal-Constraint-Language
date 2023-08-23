#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "globals.h"

CharAccumulator errorMsg = {100, 0, 0};

void addStringToErrorMsg(char *string) {
	CharAccumulator_appendChars(&errorMsg, string);
}

void addSubStringToErrorMsg(SubString *subString) {
	CharAccumulator_appendSubString(&errorMsg, subString);
}

CharAccumulator errorIndicator = {100, 0, 0};

void addStringToErrorIndicator(char *string) {
	CharAccumulator_appendChars(&errorIndicator, string);
}

void addSubStringToErrorIndicator(SubString *subString) {
	CharAccumulator_appendSubString(&errorIndicator, subString);
}

// there might be a simpler way of doing this
// But this works!
int getSizeOfUint(unsigned int number) {
	int size = 1;
	
	while (number > 9) {
		number /= 10;
		size++;
	}
	
	return size;
}

void printLine(char *source, int *index) {
	while (1) {
		char character = source[*index];
		
		if (character == 0) {
			putchar('\n');
			exit(1);
		} else if (character == '\n') {
			putchar('\n');
			return;
		}
		
		putchar(character);
		
		(*index) += 1;
	}
}

void printLineWithIndicator(char *source, int *index, int columnStart, int columnEnd, int indicatorOffset) {
	CharAccumulator indicator = {100, 0, 0};
	CharAccumulator_initialize(&indicator);
	
	for (int i = 0; i < indicatorOffset; i++) {
		CharAccumulator_appendChars(&indicator, " ");
	}
	
	int i = 0;
	while (1) {
		char character = source[*index];
		
		if (character == 0 || character == '\n') {
			putchar('\n');
			break;
		} else if (character == '\t') {
			// Make all tabs the size of 4 spaces,
			// this is so that the positions of characters can be determined for monospace fonts,
			// even if the size of tabs is changed.
			printf("    ");
			
			i++;
			
			if (i > columnStart && i < columnEnd) {
				CharAccumulator_appendChars(&indicator, "^^^^");
			} else {
				CharAccumulator_appendChars(&indicator, "    ");
			}
		} else {
			putchar(character);
			
			i++;
			
			if (i > columnStart && i <= columnEnd) {
				CharAccumulator_appendChars(&indicator, "^");
			} else if (i > columnEnd) {
				
			} else {
				CharAccumulator_appendChars(&indicator, " ");
			}
		}
		
		(*index) += 1;
	}
	
	if (errorIndicator.size > 0) {
		printf("%s %s\n", indicator.data, errorIndicator.data);
		CharAccumulator_initialize(&errorIndicator);
	} else {
		printf("%s\n", indicator.data);
	}
	
	CharAccumulator_free(&indicator);
}

void printSpaces(int count) {
	while (count > 0) {
		putchar(' ');
		count--;
	}
}

void compileError(ModuleInformation *MI, SourceLocation location) {
	if (errorMsg.size > 0) {
		// \x1B[31m \x1B[0m
		if (MI->name == NULL || MI->currentFilePath == NULL) {
			printf("error: %s\n", errorMsg.data);
		} else {
			printf("\nerror: %s\n at %s:%s:%d\n", errorMsg.data, MI->name, MI->currentFilePath, location.line);
		}
		CharAccumulator_initialize(&errorMsg);
	}
	
	int maxLineSize = getSizeOfUint(location.line + 1);
	
	int index = 0;
	int line = 1;
	while (1) {
		char character = MI->currentSource[index];
		
		if (character == 0) {
			putchar('\n');
			break;
		}
		
		if (line == location.line - 1) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLine(MI->currentSource, &index);
			line++;
		} else if (line == location.line) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLineWithIndicator(MI->currentSource, &index, location.columnStart, location.columnEnd, maxLineSize + 2);
			line++;
		} else if (line == location.line + 1) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLine(MI->currentSource, &index);
			break;
		} else if (character == '\n') {
			line++;
		}
		
		index++;
	}
	
	exit(1);
}
