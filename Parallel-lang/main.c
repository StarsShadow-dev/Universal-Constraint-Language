#include <stdio.h>
#include <string.h>

#include "main.h"
#include "types.h"
#include "globals.h"
#include "lexer.h"

/// print the tokens to standard out in a form resembling JSON
void printTokens(linkedList_Node *head) {
	printf("tokens: [\n");
	
	linkedList_Node *current = head;
	while (1) {
		printf(
			"\t{type: %i, value: \"%s\", line: %i, start: %i, end: %i}",
			((token *)(current->data))->type,
			((token *)(current->data))->value,
			((token *)(current->data))->location.line,
			((token *)(current->data))->location.columnStart,
			((token *)(current->data))->location.columnEnd
		);
		
		if (current->next == NULL) break;
		
		printf(",\n");
		current = current->next;
	}
	
	printf("\n]\n");
}

char *readFile(const char *path) {
	FILE* file = fopen(path, "r");
	
	if (file == 0) {
		printf("could not find file %s\n", path);
		exit(1);
	}

	// get the size of the file
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);
	
	char *buffer = malloc(fileSize + 1);
	
	if (buffer == NULL) {
		printf("malloc failed to get space for a file buffer\n");
		exit(1);
	}
	
	if (fread(buffer, sizeof(char), fileSize, file) != fileSize) {
		printf("fread failed\n");
		exit(1);
	}
	buffer[fileSize] = 0;
	
	fclose(file);
	
	return buffer;
}

int main(int argc, char **argv) {
//	source = "function main()\n{}";
//
//	linkedList_Node *tokens = lex();
//	printTokens(tokens);
	
	char *buffer = readFile("config.json");
	printf("buffer: %s", buffer);
	free(buffer);
	
	return 0;
}
