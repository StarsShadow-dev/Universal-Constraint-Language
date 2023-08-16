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
	
//	Token *nextToken = ((Token *)((*current)->next->data));
//	if (nextToken->type == TokenType_operator && SubString_string_cmp(&nextToken->subString, "<") == 0) {
//		
//	}
	
	ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_type));
	data->nodeType = ASTnodeType_type;
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
	if (SubString_string_cmp(subString, "=") == 0) {
		return 1;
	}
	
	else if (SubString_string_cmp(subString, "||") == 0) {
		return 2;
	}
	
	else if (SubString_string_cmp(subString, "&&") == 0) {
		return 3;
	}
	
	else if (
		SubString_string_cmp(subString, "==") == 0 ||
		SubString_string_cmp(subString, ">") == 0 ||
		SubString_string_cmp(subString, "<") == 0
	) {
		return 4;
	}
	
	else if (SubString_string_cmp(subString, "+") == 0 || SubString_string_cmp(subString, "-") == 0) {
		return 5;
	}
	
	else if (SubString_string_cmp(subString, "*") == 0 || SubString_string_cmp(subString, "/") == 0) {
		return 6;
	}
	
	else if (SubString_string_cmp(subString, ".") == 0) {
		return 7;
	}
	
	else {
		abort();
	}
}

linkedList_Node *parseOperators(linkedList_Node **current, linkedList_Node *left, int precedence) {
	Token *operator = ((Token *)((*current)->data));
	
	if (operator->type == TokenType_operator) {
		int nextPrecedence = getOperatorPrecedence(&operator->subString);
		
		if (nextPrecedence > precedence) {
			linkedList_Node *AST = NULL;
			
			*current = (*current)->next;
			linkedList_Node *right = parseOperators(current, parse(current, ParserMode_noOperatorChecking), nextPrecedence);
			
			ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_operator));
			
			data->nodeType = ASTnodeType_operator;
			data->location = operator->location;
			
			if (SubString_string_cmp(&operator->subString, "=") == 0) {
				Token *token = (Token *)((*current)->data);
				
				if (CURRENT_IS_NOT_SEMICOLON) {
					printf("expected ';' after variable assignment\n");
					compileError(token->location);
				}
				*current = (*current)->next;
				
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_assignment;
			} else if (SubString_string_cmp(&operator->subString, "==") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_equivalent;
			} else if (SubString_string_cmp(&operator->subString, ">") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_greaterThan;
			} else if (SubString_string_cmp(&operator->subString, "<") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_lessThan;
			} else if (SubString_string_cmp(&operator->subString, "+") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_add;
			} else if (SubString_string_cmp(&operator->subString, "-") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_subtract;
			} else if (SubString_string_cmp(&operator->subString, ".") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_memberAccess;
			} else {
				abort();
			}
			
			((ASTnode_operator *)data->value)->left = left;
			((ASTnode_operator *)data->value)->right = right;
			
			return parseOperators(current, AST, precedence);
		}
	}
	
	return left;
}

linkedList_Node *parse(linkedList_Node **current, ParserMode parserMode) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		if (parserMode != ParserMode_noOperatorChecking && (*current)->next != NULL) {
			Token *nextToken = (Token *)((*current)->next->data);
			
			if (nextToken->type == TokenType_operator) {
				linkedList_Node *operatorAST = parseOperators(current, parse(current, ParserMode_noOperatorChecking), 0);
				Token *token = (Token *)((*current)->data);
				if (token->type == TokenType_separator && SubString_string_cmp(&token->subString, "(") == 0) {
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
					
					data->nodeType = ASTnodeType_call;
					data->location = token->location;
					
					((ASTnode_call *)data->value)->left = operatorAST;
					((ASTnode_call *)data->value)->arguments = arguments;
				} else {
					linkedList_join(&AST, &operatorAST);
				}
				if (parserMode == ParserMode_expression) {
					return AST;
				}
				// there might be an operator right after this operator
				// so go back to the top of the while loop
				continue;
			}
		}
		
		Token *token = (Token *)((*current)->data);
		
		switch (token->type) {
			case TokenType_word: {
				// function definition
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
					ASTnode *returnType = (ASTnode *)parseType(current)->data;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node *codeBlock = parse(current, ParserMode_normal);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 0;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = codeBlock;
					} else if (codeStart->type == TokenType_string) {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 1;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						
						ASTnode *codeBlockData = linkedList_addNode(&(((ASTnode_function *)data->value)->codeBlock), sizeof(ASTnode) + sizeof(ASTnode_string));
						codeBlockData->nodeType = ASTnodeType_string;
						codeBlockData->location = codeStart->location;
						((ASTnode_string *)codeBlockData->value)->value = &codeStart->subString;
						
						*current = (*current)->next;
					} else {
						printf("function definition expected an openingBracket or a quotation mark\n");
						compileError(codeStart->location);
					}
				}
				
				// struct
				else if (SubString_string_cmp(&token->subString, "struct") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after struct keyword\n");
						compileError(nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("struct expected '{'\n");
						compileError(openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *block = parse(current, ParserMode_normal);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_struct));
					
					data->nodeType = ASTnodeType_struct;
					data->location = token->location;
					
					((ASTnode_struct *)data->value)->name = &nameToken->subString;
					((ASTnode_struct *)data->value)->block = block;
				}
				
				// return statement
				else if (SubString_string_cmp(&token->subString, "return") == 0) {
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
						
						data->nodeType = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = expression;
					} else {
						*current = (*current)->next;
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->nodeType = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = NULL;
					}
				}
				
				// variable definition
				else if (SubString_string_cmp(&token->subString, "var") == 0) {
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
					ASTnode *type = (ASTnode *)parseType(current)->data;
					
					linkedList_Node *expression = NULL;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *equals = ((Token *)((*current)->data));
					if (equals->type == TokenType_operator && SubString_string_cmp(&equals->subString, "=") == 0) {
						*current = (*current)->next;
						expression = parse(current, ParserMode_expression);
					}
					
					if (CURRENT_IS_NOT_SEMICOLON) {
						printf("expected ';' after variable definition\n");
						compileError(token->location);
					}
					*current = (*current)->next;
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variableDefinition));
					
					data->nodeType = ASTnodeType_variableDefinition;
					data->location = token->location;
					
					((ASTnode_variableDefinition *)data->value)->name = &nameToken->subString;
					((ASTnode_variableDefinition *)data->value)->type = type;
					((ASTnode_variableDefinition *)data->value)->expression = expression;
				}
				
				// true constant
				else if (SubString_string_cmp(&token->subString, "true") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
					
					data->nodeType = ASTnodeType_true;
					data->location = token->location;
					
					*current = (*current)->next;
				}
				
				// false constant
				else if (SubString_string_cmp(&token->subString, "false") == 0) {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
					
					data->nodeType = ASTnodeType_false;
					data->location = token->location;
					
					*current = (*current)->next;
				}
				
				else {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nextToken = ((Token *)((*current)->data));
					
					if (parserMode != ParserMode_noOperatorChecking && nextToken->type == TokenType_separator && SubString_string_cmp(&nextToken->subString, "(") == 0) {
						// if statement
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
							linkedList_Node *trueCodeBlock = parse(current, ParserMode_normal);
							
							Token *elseToken = ((Token *)((*current)->data));
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_if));
							
							data->nodeType = ASTnodeType_if;
							data->location = token->location;
							
							((ASTnode_if *)data->value)->expression = expression;
							((ASTnode_if *)data->value)->trueCodeBlock = trueCodeBlock;
							
							if (SubString_string_cmp(&elseToken->subString, "else") == 0) {
								*current = (*current)->next;
								*current = (*current)->next;
								linkedList_Node *falseCodeBlock = parse(current, ParserMode_normal);
								
								((ASTnode_if *)data->value)->falseCodeBlock = falseCodeBlock;
							} else {
								((ASTnode_if *)data->value)->falseCodeBlock = NULL;
							}
						}
						
						// while loop
						else if (SubString_string_cmp(&token->subString, "while") == 0) {
							*current = (*current)->next;
							linkedList_Node *expression = parse(current, ParserMode_expression);
							
							endIfCurrentIsEmpty()
							Token *closingParentheses = ((Token *)((*current)->data));
							if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
								printf("while loop expected ')'\n");
								compileError(token->location);
							}
							
							*current = (*current)->next;
							endIfCurrentIsEmpty()
							Token *openingBracket = ((Token *)((*current)->data));
							if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
								printf("while loop expected '{'\n");
								compileError(openingBracket->location);
							}
							
							*current = (*current)->next;
							linkedList_Node *codeBlock = parse(current, ParserMode_normal);
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_while));
							
							data->nodeType = ASTnodeType_while;
							data->location = token->location;
							
							((ASTnode_while *)data->value)->expression = expression;
							((ASTnode_while *)data->value)->codeBlock = codeBlock;
						}
						
						// function call
						else {
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
							
							linkedList_Node *left = NULL;
							
							ASTnode *leftData = linkedList_addNode(&left, sizeof(ASTnode) + sizeof(ASTnode_variable));
							
							leftData->nodeType = ASTnodeType_variable;
							leftData->location = token->location;
							((ASTnode_variable *)leftData->value)->name = &token->subString;
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
							
							data->nodeType = ASTnodeType_call;
							data->location = token->location;
							
							((ASTnode_call *)data->value)->left = left;
							((ASTnode_call *)data->value)->arguments = arguments;
						}
					}
					
					// variable
					else {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_variable));
						
						data->nodeType = ASTnodeType_variable;
						data->location = token->location;
						((ASTnode_variable *)data->value)->name = &token->subString;
					}
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				
				data->nodeType = ASTnodeType_number;
				data->location = token->location;
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(current).value;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_string: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_string));
				
				data->nodeType = ASTnodeType_string;
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
