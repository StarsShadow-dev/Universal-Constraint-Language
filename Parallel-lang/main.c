#include <stdio.h>
#include <string.h>

#include "main.h"
#include "types.h"
#include "globals.h"
#include "lexer.h"

void printTokens(void) {
	linkedList_Node *current = tokens;
	while (1) {
		printf("token(type: %i value: %s)", ((token *)(current->data))->type, ((token *)(current->data))->value);
		
		if (current->next == NULL) break;
		
		// space
		putchar(32);
		current = current->next;
	}
	
	// line break
	putchar(10);
}

int main(int argc, char **argv) {
	allocateGlobals();
	
	source = "function main";
	
	lex();
	
	printTokens();
	
	deallocateGlobals();
	return 0;
}
