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
			printLine(&index);
		} else if (line == location.line + 1) {
			printLine(&index);
			exit(1);
		}
		
		index++;
	}
}
