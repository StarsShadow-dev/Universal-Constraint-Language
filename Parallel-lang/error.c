#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "globals.h"

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

void printLineWithIndicator(int *index, int columnStart, int columnEnd) {
	String indicator = {100, 0, 0};
	String_initialize(&indicator);
	
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
			
			if (i > columnStart && i < columnEnd) {
				String_appendChars(&indicator, "↑↑↑↑");
			} else {
				String_appendChars(&indicator, "⎽⎽⎽⎽");
			}
			
			i++;
		} else {
			putchar(character);
			
			if (i > columnStart && i <= columnEnd) {
				String_appendChars(&indicator, "↑");
			} else {
				String_appendChars(&indicator, "⎽");
			}
			
			i++;
		}
		
		(*index) += 1;
	}
	
	printf("%s\n", indicator.data);
	
	String_free(&indicator);
}

void compileError(SourceLocation location) {
	int index = 0;
	int line = 1;
	while (1) {
		char character = source[index];
		
		if (character == 0) {
			putchar('\n');
			exit(1);
		} else if (character == '\n') {
			line++;
		}
		
		if (line == location.line - 1) {
			line++;
			printLine(&index);
		} else if (line == location.line) {
			line++;
			printLineWithIndicator(&index, location.columnStart, location.columnEnd);
		} else if (line == location.line + 1) {
			printLine(&index);
			exit(1);
		}
		
		index++;
	}
}
