#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "error.h"

#define endIfCurrentIsEmpty()\
if (current == NULL) {\
	printf("unexpected end of file\n");\
	compileError(token->location);\
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

linkedList_Node *parseType(linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	Token *token = ((Token *)((*current)->data));
	endIfCurrentIsEmpty()
	if (token->type != TokenType_word) {
		printf("not a word in a type expression\n");
		compileError(token->location);
	}
	
	ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_type));
	data->type = ASTnodeType_type;
	data->location = token->location;
	((ASTnode_type *)data->value)->name = &token->subString;
	
	return AST;
}

ASTnode_number parseInt(linkedList_Node **current) {
	ASTnode_number node = {};
	
	Token *token = ((Token *)((*current)->data));
	endIfCurrentIsEmpty()
	
	if (token->type != TokenType_number) abort();
	
	int64_t accumulator = 0;
	
	long stringLength = token->subString.length;
	
	for (int index = 0; index < stringLength; index++) {
		char character = token->subString.start[index];
		int number = character - '0';
		
		accumulator += intPow(10, stringLength-index-1) * number;
	}
	
	node.value = accumulator;
	
	return node;
}

typedef struct {
	linkedList_Node *list1;
	linkedList_Node *list2;
} linkedList_Node_tuple;

linkedList_Node_tuple parseFunctionArguments(linkedList_Node **current) {
	linkedList_Node *argumentNames = NULL;
	linkedList_Node *argumentTypes = NULL;
	
	while (1) {
		if (*current == NULL) {
			printf("unexpected end of file at function arguments\n");
			exit(1);
		}
		
		Token *token = ((Token *)((*current)->data));
		
		switch (token->type) {
			case TokenType_word: {
				*current = (*current)->next;
				endIfCurrentIsEmpty()
				Token *colon = ((Token *)((*current)->data));
				
				if (colon->type != TokenType_separator || SubString_cmp(colon->subString, ":") != 0) {
					printf("function argument expected a colon\n");
					compileError(colon->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *type = parseType(current);
				
				SubString *nameSubString = linkedList_addNode(&argumentNames, sizeof(SubString));
				nameSubString->start = token->subString.start;
				nameSubString->length = token->subString.length;
				
				// join type to argumentTypes
				linkedList_join(&argumentTypes, &type);
				
				break;
			}
				
			case TokenType_separator: {
				if (SubString_cmp(token->subString, ")") == 0) {
					return (linkedList_Node_tuple){argumentNames, argumentTypes};
				} else {
					printf("unexpected separator: '");
					SubString_print(token->subString);
					printf("'\n");
					compileError(token->location);
				}
				break;
			}
				
			default: {
				printf("unexpected token type: %u inside of function arguments\n", token->type);
				compileError(token->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
}

linkedList_Node *parse(linkedList_Node **current, int endAfterOneToken) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		Token *token = ((Token *)((*current)->data));
		
		switch (token->type) {
			case TokenType_word: {
				if (SubString_cmp(token->subString, "function") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after function keyword\n");
						compileError(nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_cmp(openingParentheses->subString, "(") != 0) {
						printf("function definition expected an openingParentheses\n");
						compileError(openingParentheses->location);
					}
					
					*current = (*current)->next;
					linkedList_Node_tuple argument_tuple = parseFunctionArguments(current);
					
					linkedList_Node *argumentNames = argument_tuple.list1;
					linkedList_Node *argumentTypes = argument_tuple.list2;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					
					if (colon->type != TokenType_separator || SubString_cmp(colon->subString, ":") != 0) {
						printf("function definition expected a colon\n");
						compileError(colon->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *returnType = parseType(current);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_cmp(codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node * codeBlock = parse(current, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->type = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 0;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = codeBlock;
					} else if (codeStart->type == TokenType_string) {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->type = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 1;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						
						ASTnode *codeBlockData = linkedList_addNode(&(((ASTnode_function *)data->value)->codeBlock), sizeof(ASTnode) + sizeof(ASTnode_string));
						codeBlockData->type = ASTnodeType_string;
						codeBlockData->location = codeStart->location;
						((ASTnode_string *)codeBlockData->value)->value = &codeStart->subString;
					} else {
						printf("function definition expected an openingBracket or a quotation mark\n");
						compileError(codeStart->location);
					}
				} else if (SubString_cmp(token->subString, "return") == 0) {
					*current = (*current)->next;
					linkedList_Node *expression = parse(current, 1);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					if (((Token *)((*current)->data))->type != TokenType_separator || SubString_cmp(((Token *)((*current)->data))->subString, ";") != 0) {
						printf("expected ';' after return statement\n");
						compileError(token->location);
					}
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
					
					data->type = ASTnodeType_return;
					data->location = token->location;
					
					((ASTnode_return *)data->value)->expression = expression;
				} else {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_cmp(openingParentheses->subString, "(") != 0) {
						printf("unexpected word: ");
						SubString_print(token->subString);
						printf("\n");
						compileError(token->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *arguments = parse(current, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
					
					data->type = ASTnodeType_call;
					data->location = token->location;
					
					((ASTnode_call *)data->value)->name = &token->subString;
					((ASTnode_call *)data->value)->arguments = arguments;
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				
				data->type = ASTnodeType_number;
				
				data->location = token->location;
				
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(current).value;
				break;
			}
				
//			case TokenType_string: {
//				break;
//			}
				
			case TokenType_separator: {
				if (SubString_cmp(token->subString, ")") == 0) {
					return AST;
				} else if (SubString_cmp(token->subString, "}") == 0) {
					return AST;
				} else {
					printf("unexpected separator: '");
					SubString_print(token->subString);
					printf("'\n");
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
		
		if (endAfterOneToken) {
			return AST;
		}
		
		*current = (*current)->next;
	}
}
