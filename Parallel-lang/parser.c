#include <stdio.h>

#include "parser.h"
#include "error.h"

linkedList_Node *parse(linkedList_Node *tokens) {
	linkedList_Node *current = tokens;
	
	while (1) {
		if (current == NULL) {
			break;
		}
		
		Token *token = ((Token *)(current->data));
		
		printf("token: %u %s\n", token->type, token->value);
		
		switch (token->type) {
			case TokenType_word: {
				break;
			}
				
			case TokenType_string: {
				break;
			}
				
			case TokenType_separator: {
				break;
			}
				
			default: {
				printf("unknown token type: %u\n", token->type);
				compileError(token->location);
				break;
			}
		}
		
		current = current->next;
	}
	
	return 0;
}
