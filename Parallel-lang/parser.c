#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "error.h"

#define endIfCurrentIsEmpty()\
if (current == NULL) {\
	printf("unexpected end of file\n");\
	compileError(token->location);\
}

#define CURRENT_IS_NOT_SEMICOLON (((Token *)((*current)->data))->type != TokenType_separator || SubString_string_cmp(&((Token *)((*current)->data))->subString, ";") != 0)

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
				
				if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
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
				
				*current = (*current)->next;
				token = ((Token *)((*current)->data));
				endIfCurrentIsEmpty()
				if (token->type != TokenType_separator || SubString_string_cmp(&token->subString, ",") != 0) {
					if (token->type == TokenType_separator) {
						if (SubString_string_cmp(&token->subString, ")") == 0) {
							return (linkedList_Node_tuple){argumentNames, argumentTypes};
						} else {
							printf("unexpected separator: '");
							SubString_print(&token->subString);
							printf("'\n");
							compileError(token->location);
						}
					}
					printf("expected a comma\n");
					compileError(token->location);
				}
				
				break;
			}
				
			case TokenType_separator: {
				if (SubString_string_cmp(&token->subString, ")") == 0) {
					return (linkedList_Node_tuple){argumentNames, argumentTypes};
				} else {
					printf("unexpected separator: '");
					SubString_print(&token->subString);
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

int getOperatorPrecedence(SubString *subString) {
	if (SubString_string_cmp(subString, "||") == 0) {
		return 1;
	} else if (SubString_string_cmp(subString, "&&") == 0) {
		return 2;
	} else if (SubString_string_cmp(subString, "==") == 0) {
		return 3;
	} else if (SubString_string_cmp(subString, "+") == 0 || SubString_string_cmp(subString, "-") == 0) {
		return 4;
	} else if (SubString_string_cmp(subString, "*") == 0 || SubString_string_cmp(subString, "/") == 0) {
		return 5;
	} else {
		abort();
	}
}

linkedList_Node *parseOperators(linkedList_Node **current) {
	Token *operator1 = ((Token *)((*current)->next->data));
	Token *operator2 = (Token *)((*current)->next->next->next->data);
	
	if (
		operator2->type == TokenType_operator &&
		SubString_string_cmp(&operator2->subString, "=") != 0 &&
		getOperatorPrecedence(&operator1->subString) < getOperatorPrecedence(&operator2->subString)
	) {
		printf("Operator precedence not finished yet.");
		abort();
//		return parseOperators(current);
	} else {
		linkedList_Node *operatorAST = NULL;
		
		linkedList_Node *left = parse(current, ParserMode_noOperatorChecking);
		*current = (*current)->next;
		linkedList_Node *right = parse(current, ParserMode_expression);
		
		ASTnode *data = linkedList_addNode(&operatorAST, sizeof(ASTnode) + sizeof(ASTnode_operator));
		
		data->type = ASTnodeType_operator;
		data->location = operator1->location;
		
		((ASTnode_operator *)data->value)->left = left;
		((ASTnode_operator *)data->value)->right = right;
		
		return operatorAST;
	}
}

// operatorPrecedence should be zero when it is not being used
linkedList_Node *parse(linkedList_Node **current, ParserMode parserMode) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		if (parserMode != ParserMode_noOperatorChecking) {
			Token *nextToken = (Token *)((*current)->next->data);
			if (nextToken->type == TokenType_operator && SubString_string_cmp(&nextToken->subString, "=") != 0) {
				linkedList_Node *operatorAST = parseOperators(current);
				linkedList_join(&AST, &operatorAST);
				if (parserMode == ParserMode_expression) {
					return AST;
				}
			}
		}
		
		Token *token = ((Token *)((*current)->data));
		
		switch (token->type) {
			case TokenType_word: {
				if (SubString_string_cmp(&token->subString, "function") == 0) {
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
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
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
					
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("function definition expected a colon\n");
						compileError(colon->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *returnType = parseType(current);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node *codeBlock = parse(current, ParserMode_normal);
						
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
						
						*current = (*current)->next;
					} else {
						printf("function definition expected an openingBracket or a quotation mark\n");
						compileError(codeStart->location);
					}
				} else if (SubString_string_cmp(&token->subString, "return") == 0) {
					*current = (*current)->next;
					if (CURRENT_IS_NOT_SEMICOLON) {
						linkedList_Node *expression = parse(current, ParserMode_expression);
						
						endIfCurrentIsEmpty()
						if (CURRENT_IS_NOT_SEMICOLON) {
							printf("expected ';' after return statement\n");
							compileError(token->location);
						}
						*current = (*current)->next;
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->type = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = expression;
					} else {
						*current = (*current)->next;
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->type = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = NULL;
					}
				} else if (SubString_string_cmp(&token->subString, "var") == 0) {
					if (parserMode != ParserMode_normal) {
						printf("variable definition in a weird spot\n");
						compileError(token->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					if (nameToken->type != TokenType_word) {
						printf("expected word after var keyword\n");
						compileError(nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("variable definition expected a colon\n");
						compileError(colon->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *type = parseType(current);
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *equals = ((Token *)((*current)->data));
					if (equals->type != TokenType_operator || SubString_string_cmp(&equals->subString, "=") != 0) {
						printf("expected equals as part of variable definition\n");
						compileError(equals->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *expression = parse(current, ParserMode_expression);
					
					if (CURRENT_IS_NOT_SEMICOLON) {
						printf("expected ';' after variable definition\n");
						compileError(token->location);
					}
					*current = (*current)->next;
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variableDefinition));
					
					data->type = ASTnodeType_variableDefinition;
					data->location = token->location;
					
					((ASTnode_variableDefinition *)data->value)->name = &nameToken->subString;
					((ASTnode_variableDefinition *)data->value)->type = type;
					((ASTnode_variableDefinition *)data->value)->expression = expression;
				} else if (SubString_string_cmp(&token->subString, "true") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
					
					data->type = ASTnodeType_true;
					data->location = token->location;
					
					*current = (*current)->next;
				} else if (SubString_string_cmp(&token->subString, "false") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
					
					data->type = ASTnodeType_false;
					data->location = token->location;
					
					*current = (*current)->next;
				} else {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nextToken = ((Token *)((*current)->data));
					
					if (nextToken->type == TokenType_separator && SubString_string_cmp(&nextToken->subString, "(") == 0) {
						if (SubString_string_cmp(&token->subString, "if") == 0) {
							*current = (*current)->next;
							linkedList_Node *expression = parse(current, ParserMode_expression);
							
							endIfCurrentIsEmpty()
							Token *closingParentheses = ((Token *)((*current)->data));
							if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
								printf("if statement expected ')'\n");
								compileError(token->location);
							}
							
							*current = (*current)->next;
							endIfCurrentIsEmpty()
							Token *openingBracket = ((Token *)((*current)->data));
							if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
								printf("if statement expected '{'\n");
								compileError(openingBracket->location);
							}
							
							*current = (*current)->next;
							linkedList_Node *codeBlock = parse(current, ParserMode_normal);
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_if));
							
							data->type = ASTnodeType_if;
							data->location = token->location;
							
							((ASTnode_if *)data->value)->expression = expression;
							((ASTnode_if *)data->value)->codeBlock = codeBlock;
						} else { // function call
							*current = (*current)->next;
							linkedList_Node *arguments = parse(current, ParserMode_arguments);
							
							endIfCurrentIsEmpty()
							if (parserMode == ParserMode_normal) {
								if (CURRENT_IS_NOT_SEMICOLON) {
									printf("expected ';' after function call\n");
									compileError(token->location);
								}
								*current = (*current)->next;
							}
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
							
							data->type = ASTnodeType_call;
							data->location = token->location;
							
							((ASTnode_call *)data->value)->name = &token->subString;
							((ASTnode_call *)data->value)->arguments = arguments;
						}
					} else if (nextToken->type == TokenType_operator && SubString_string_cmp(&nextToken->subString, "=") == 0) {
						if (parserMode != ParserMode_normal) {
							printf("variable assignment in a weird spot\n");
							compileError(token->location);
						}
						
						*current = (*current)->next;
						linkedList_Node *expression = parse(current, ParserMode_expression);
						
						if (parserMode == ParserMode_normal) {
							if (CURRENT_IS_NOT_SEMICOLON) {
								printf("expected ';' after variable assignment\n");
								compileError(token->location);
							}
							*current = (*current)->next;
						}
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variableAssignment));
						
						data->type = ASTnodeType_variableAssignment;
						data->location = token->location;
						
						((ASTnode_variableAssignment *)data->value)->name = &token->subString;
						((ASTnode_variableAssignment *)data->value)->expression = expression;
					} else {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variable));
						
						data->type = ASTnodeType_variable;
						data->location = token->location;
						((ASTnode_variable *)data->value)->name = &token->subString;
					}
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				
				data->type = ASTnodeType_number;
				data->location = token->location;
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(current).value;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_string: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_string));
				
				data->type = ASTnodeType_string;
				data->location = token->location;
				((ASTnode_string *)data->value)->value = &token->subString;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_separator: {
				if (SubString_string_cmp(&token->subString, ")") == 0 || SubString_string_cmp(&token->subString, "}") == 0) {
					*current = (*current)->next;
					return AST;
				} else {
					printf("unexpected separator: '");
					SubString_print(&token->subString);
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
		
		if (parserMode == ParserMode_expression || parserMode == ParserMode_noOperatorChecking) {
			return AST;
		}
		
		if (parserMode == ParserMode_arguments) {
			token = ((Token *)((*current)->data));
			if (token->type != TokenType_separator || SubString_string_cmp(&token->subString, ",") != 0) {
				if (token->type == TokenType_separator) {
					if (SubString_string_cmp(&token->subString, ")") == 0) {
						*current = (*current)->next;
						return AST;
					} else {
						printf("unexpected separator: '");
						SubString_print(&token->subString);
						printf("'\n");
						compileError(token->location);
					}
				}
				printf("expected a comma\n");
				compileError(token->location);
			}
			*current = (*current)->next;
		}
	}
}
