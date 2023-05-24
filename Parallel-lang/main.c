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

int main(int argc, char **argv) {
	source = "function main()\n{}";
	
	linkedList_Node *tokens = lex();
	printTokens(tokens);
	
	return 0;
}
