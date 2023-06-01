#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "error.h"

#define endIfCurrentIsEmpty()\
if (current == NULL) {\
	printf("unexpected end of file\n");\
	compileError(token->location);\
}

ASTnode_type parseType(linkedList_Node **current) {
	ASTnode_type type = {};
	
	Token *token = ((Token *)((*current)->data));
	endIfCurrentIsEmpty()
	
	if (token->type != TokenType_word) {
		printf("not a word in a type expression\n");
		compileError(token->location);
	}
	
	type.name = token->value;
	
	return type;
}

int64_t intPow(int64_t base, int64_t exponent) {
	if (exponent == 0) {
		return 1;
	}
	int64_t result = base;
	for (int i = 1; i < exponent; i++) {
		result *= base;
	}
	return result;
}

ASTnode_number parseInt(linkedList_Node **current) {
	ASTnode_number node = {};
	
	Token *token = ((Token *)((*current)->data));
	endIfCurrentIsEmpty()
	
	if (token->type != TokenType_number) abort();
	
	int64_t accumulator = 0;
	
	long stringLength = strlen(token->value);
	
	for (int index = 0; index < stringLength; index++) {
		char character = token->value[index];
		int number = character - '0';
		
		accumulator += intPow(10, stringLength-index-1) * number;
	}
	
	node.value = accumulator;
	
	return node;
}

linkedList_Node *parse(linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		Token *token = ((Token *)((*current)->data));
		
		printf("token: %u %s\n", token->type, token->value);
		
		switch (token->type) {
			case TokenType_word: {
				if (strcmp(token->value, "function") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					printf("nameToken: %u %s\n", nameToken->type, nameToken->value);
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after function keyword\n");
						compileError(nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					printf("openingParentheses: %u %s\n", openingParentheses->type, openingParentheses->value);
					
					if (openingParentheses->type != TokenType_separator || strcmp(openingParentheses->value, "(") != 0) {
						printf("function definition expected an openingParentheses\n");
						compileError(openingParentheses->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *arguments = parse(current);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					
					printf("colon: %u %s\n", colon->type, colon->value);
					
					if (colon->type != TokenType_separator || strcmp(colon->value, ":") != 0) {
						printf("function definition expected a colon\n");
						compileError(colon->location);
					}
					
					*current = (*current)->next;
					ASTnode_type returnType = parseType(current);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					
					printf("openingBracket: %u %s\n", openingBracket->type, openingBracket->value);
					
					if (openingBracket->type != TokenType_separator || strcmp(openingBracket->value, "{") != 0) {
						printf("function definition expected an openingBracket\n");
						compileError(openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node * codeBlock = parse(current);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
					
					data->type = ASTnodeType_function;
					
					data->location = nameToken->location;
					
					((ASTnode_function *)data->value)->name = nameToken->value;
					((ASTnode_function *)data->value)->returnType = returnType;
					((ASTnode_function *)data->value)->arguments = arguments;
					((ASTnode_function *)data->value)->codeBlock = codeBlock;
				} else {
					printf("unexpected word: %s\n", token->value);
					compileError(token->location);
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				
				data->type = ASTnodeType_number;
				
				data->location = token->location;
				
				((ASTnode_number *)data->value)->value = parseInt(current).value;
				break;
			}
				
//			case TokenType_string: {
//				break;
//			}
				
			case TokenType_separator: {
				if (strcmp(token->value, ")") == 0) {
					return AST;
				} else if (strcmp(token->value, "}") == 0) {
					return AST;
				} else {
					printf("unexpected separator: %s\n", token->value);
					compileError(token->location);
				}
				break;
			}
				
			default: {
				printf("unknown token type: %u\n", token->type);
				compileError(token->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
}
