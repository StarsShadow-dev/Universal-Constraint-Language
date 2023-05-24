#include <stdio.h>
#include <string.h>

#include "main.h"
#include "types.h"
#include "globals.h"
#include "lexer.h"

/// print the tokens to standard out in a form resembling JSON
void printTokens(void) {
	printf("tokens: [");
	
	linkedList_Node *current = tokens;
	while (1) {
		printf("{type: %i, value: \"%s\"}", ((token *)(current->data))->type, ((token *)(current->data))->value);
		
		if (current->next == NULL) break;
		
		printf(", ");
		current = current->next;
	}
	
	printf("]\n");
}

int main(int argc, char **argv) {
	allocateGlobals();
	
	source = "function main() {}";
	
	lex();
	
	printTokens();
	
	deallocateGlobals();
	return 0;
}
