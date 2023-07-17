#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "globals.h"

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

void printLine(int *index) {
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

void printLineWithIndicator(int *index, int columnStart, int columnEnd, int indicatorOffset) {
	CharAccumulator indicator = {100, 0, 0};
	CharAccumulator_initialize(&indicator);
	
	for (int i = 0; i < indicatorOffset; i++) {
		CharAccumulator_appendChars(&indicator, "_");
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
				CharAccumulator_appendChars(&indicator, "____");
			}
		} else {
			putchar(character);
			
			i++;
			
			if (i > columnStart && i <= columnEnd) {
				CharAccumulator_appendChars(&indicator, "^");
			} else {
				CharAccumulator_appendChars(&indicator, "_");
			}
		}
		
		(*index) += 1;
	}
	
	printf("%s\n", indicator.data);
	
	CharAccumulator_free(&indicator);
}

void printSpaces(int count) {
	while (count > 0) {
		putchar(' ');
		count--;
	}
}

void compileError(SourceLocation location) {
	int maxLineSize = getSizeOfUint(location.line + 1);
	
	int index = 0;
	int line = 1;
	while (1) {
		char character = source[index];
		
		if (character == 0) {
			putchar('\n');
			exit(1);
		}
		
		if (line == location.line - 1) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLine(&index);
			line++;
		} else if (line == location.line) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLineWithIndicator(&index, location.columnStart, location.columnEnd, maxLineSize + 2);
			line++;
		} else if (line == location.line + 1) {
			printf("%d", line);
			int lineSize = getSizeOfUint(line);
			if (lineSize < maxLineSize) {
				printSpaces(maxLineSize - lineSize);
			}
			printf(" |");
			printLine(&index);
			exit(1);
		} else if (character == '\n') {
			line++;
		}
		
		index++;
	}
}
