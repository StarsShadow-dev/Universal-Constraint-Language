#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "error.h"

#define endIfCurrentIsEmpty()\
if (*current == NULL) {\
	printf("unexpected end of file\n");\
	compileError(MI, token->location);\
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

ASTnode_number parseInt(ModuleInformation *MI, linkedList_Node **current) {
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
	
	else if (SubString_string_cmp(subString, "::") == 0) {
		return 8;
	}
	
	else {
		printf("getOperatorPrecedence error");
		abort();
	}
}

linkedList_Node *parseOperators(ModuleInformation *MI, linkedList_Node **current, linkedList_Node *left, int precedence, int ignoreEquals) {
	Token *operator = ((Token *)((*current)->data));
	
	if (operator->type == TokenType_operator) {
		int nextPrecedence = getOperatorPrecedence(&operator->subString);
		
		if (nextPrecedence > precedence) {
			linkedList_Node *AST = NULL;
			
			// to fix variable definitions
			if (
				ignoreEquals &&
				operator->type == TokenType_operator &&
				SubString_string_cmp(&operator->subString, "=") == 0
			) {
				return left;
			}
			
			*current = (*current)->next;
			linkedList_Node *right = parseOperators(MI, current, parse(MI, current, ParserMode_noOperatorChecking), nextPrecedence, ignoreEquals);
			
			ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_operator));
			
			data->nodeType = ASTnodeType_operator;
			data->location = operator->location;
			
			if (SubString_string_cmp(&operator->subString, "=") == 0) {
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
			} else if (SubString_string_cmp(&operator->subString, "::") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_scopeResolution;
			} else {
				abort();
			}
			
			((ASTnode_operator *)data->value)->left = left;
			((ASTnode_operator *)data->value)->right = right;
			
			return parseOperators(MI, current, AST, precedence, ignoreEquals);
		}
	}
	
	return left;
}

linkedList_Node *parseType(ModuleInformation *MI, linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
//	Token *nextToken = ((Token *)((*current)->next->data));
//	if (nextToken->type == TokenType_operator && SubString_string_cmp(&nextToken->subString, "<") == 0) {
//
//	}
	
	ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_constrainedType));
	data->nodeType = ASTnodeType_constrainedType;
	data->location = ((Token *)((*current)->data))->location;
	
	((ASTnode_constrainedType *)data->value)->type = parseOperators(MI, current, parse(MI, current, ParserMode_noOperatorChecking), 0, 1);
	
	return AST;
}

typedef struct {
	linkedList_Node *list1;
	linkedList_Node *list2;
} linkedList_Node_tuple;

linkedList_Node_tuple parseFunctionArguments(ModuleInformation *MI, linkedList_Node **current) {
	linkedList_Node *argumentNames = NULL;
	linkedList_Node *argumentTypes = NULL;
	
	while (1) {
		if (*current == NULL) {
			printf("unexpected end of file at function arguments\n");
			exit(1);
		}
		
		Token *token = (Token *)((*current)->data);
		
		switch (token->type) {
			case TokenType_word: {
				*current = (*current)->next;
				endIfCurrentIsEmpty()
				Token *colon = ((Token *)((*current)->data));
				
				if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
					printf("function argument expected a colon\n");
					compileError(MI, colon->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *type = parseType(MI, current);
				
				SubString *nameSubString = linkedList_addNode(&argumentNames, sizeof(SubString));
				nameSubString->start = token->subString.start;
				nameSubString->length = token->subString.length;
				
				// join type to argumentTypes
				linkedList_join(&argumentTypes, &type);
				
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
							compileError(MI, token->location);
						}
					}
					printf("expected a comma\n");
					compileError(MI, token->location);
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
					compileError(MI, token->location);
				}
				break;
			}
			
			default: {
				printf("unexpected token type: %u inside of function arguments\n", token->type);
				compileError(MI, token->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
}

linkedList_Node *parse(ModuleInformation *MI, linkedList_Node **current, ParserMode parserMode) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			return AST;
		}
		
		if (parserMode != ParserMode_noOperatorChecking && (*current)->next != NULL) {
			Token *nextToken = (Token *)((*current)->next->data);
			
			if (nextToken->type == TokenType_operator) {
				linkedList_Node *operatorAST = parseOperators(MI, current, parse(MI, current, ParserMode_noOperatorChecking), 0, 0);
				Token *token = (Token *)((*current)->data);
				if (token->type == TokenType_separator && SubString_string_cmp(&token->subString, "(") == 0) {
					*current = (*current)->next;
					linkedList_Node *arguments = parse(MI, current, ParserMode_arguments);
					
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
				
				if (parserMode == ParserMode_normal) {
					endIfCurrentIsEmpty()
					if (CURRENT_IS_NOT_SEMICOLON) {
						printf("expected ';' after statement\n");
						compileError(MI, token->location);
					}
					*current = (*current)->next;
				}
				// there might be an operator right after this operator
				// so go back to the top of the while loop
				continue;
			}
		}
		
		Token *token = (Token *)((*current)->data);
		
		switch (token->type) {
			case TokenType_word: {
				// import statement
				if (SubString_string_cmp(&token->subString, "import") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *pathString = ((Token *)((*current)->data));
					
					if (pathString->type != TokenType_string) {
						addStringToErrorIndicator("expected string as part of import keyword");
						compileError(MI, pathString->location);
					}
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_import));
					
					data->nodeType = ASTnodeType_import;
					data->location = token->location;
					
					((ASTnode_import *)data->value)->path = &pathString->subString;
					
					*current = (*current)->next;
					continue;
				}
				
				// function definition
				else if (SubString_string_cmp(&token->subString, "function") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after function keyword\n");
						compileError(MI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						printf("function definition expected an openingParentheses\n");
						compileError(MI, openingParentheses->location);
					}
					
					*current = (*current)->next;
					linkedList_Node_tuple argument_tuple = parseFunctionArguments(MI, current);
					
					linkedList_Node *argumentNames = argument_tuple.list1;
					linkedList_Node *argumentTypes = argument_tuple.list2;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("function definition expected a colon\n");
						compileError(MI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *returnType = (ASTnode *)parseType(MI, current)->data;
					
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node *codeBlock = parse(MI, current, ParserMode_normal);
						
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
						compileError(MI, codeStart->location);
					}
					continue;
				}
				
				// macro definition
				else if (SubString_string_cmp(&token->subString, "macro") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after macro keyword\n");
						compileError(MI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("struct expected '{'\n");
						compileError(MI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *codeBlock = parse(MI, current, ParserMode_normal);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_macro));
					data->nodeType = ASTnodeType_macro;
					data->location = nameToken->location;
					
					((ASTnode_macro *)data->value)->name = &nameToken->subString;
					((ASTnode_macro *)data->value)->codeBlock = codeBlock;
					continue;
				}
				
				// struct
				else if (SubString_string_cmp(&token->subString, "struct") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after struct keyword\n");
						compileError(MI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("struct expected '{'\n");
						compileError(MI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *block = parse(MI, current, ParserMode_normal);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_struct));
					
					data->nodeType = ASTnodeType_struct;
					data->location = token->location;
					
					((ASTnode_struct *)data->value)->name = &nameToken->subString;
					((ASTnode_struct *)data->value)->block = block;
					continue;
				}
				
				// return statement
				else if (SubString_string_cmp(&token->subString, "return") == 0) {
					*current = (*current)->next;
					if (CURRENT_IS_NOT_SEMICOLON) {
						linkedList_Node *expression = parse(MI, current, ParserMode_expression);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_return));
						
						data->nodeType = ASTnodeType_return;
						data->location = token->location;
						
						((ASTnode_return *)data->value)->expression = expression;
					} else {
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
						compileError(MI, token->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					if (nameToken->type != TokenType_word) {
						printf("expected word after var keyword\n");
						compileError(MI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("variable definition expected a colon\n");
						compileError(MI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *type = (ASTnode *)parseType(MI, current)->data;
					
					linkedList_Node *expression = NULL;
					
					Token *equals = ((Token *)((*current)->data));
					if (equals->type == TokenType_operator && SubString_string_cmp(&equals->subString, "=") == 0) {
						*current = (*current)->next;
						expression = parse(MI, current, ParserMode_expression);
					}
					
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
							linkedList_Node *expression = parse(MI, current, ParserMode_expression);
							
							endIfCurrentIsEmpty()
							Token *closingParentheses = ((Token *)((*current)->data));
							if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
								printf("if statement expected ')'\n");
								compileError(MI, token->location);
							}
							
							*current = (*current)->next;
							endIfCurrentIsEmpty()
							Token *openingBracket = ((Token *)((*current)->data));
							if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
								printf("if statement expected '{'\n");
								compileError(MI, openingBracket->location);
							}
							
							*current = (*current)->next;
							linkedList_Node *trueCodeBlock = parse(MI, current, ParserMode_normal);
							
							Token *elseToken = ((Token *)((*current)->data));
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_if));
							
							data->nodeType = ASTnodeType_if;
							data->location = token->location;
							
							((ASTnode_if *)data->value)->expression = expression;
							((ASTnode_if *)data->value)->trueCodeBlock = trueCodeBlock;
							
							if (SubString_string_cmp(&elseToken->subString, "else") == 0) {
								*current = (*current)->next;
								*current = (*current)->next;
								linkedList_Node *falseCodeBlock = parse(MI, current, ParserMode_normal);
								
								((ASTnode_if *)data->value)->falseCodeBlock = falseCodeBlock;
							} else {
								((ASTnode_if *)data->value)->falseCodeBlock = NULL;
							}
							continue;
						}
						
						// while loop
						else if (SubString_string_cmp(&token->subString, "while") == 0) {
							*current = (*current)->next;
							linkedList_Node *expression = parse(MI, current, ParserMode_expression);
							
							endIfCurrentIsEmpty()
							Token *closingParentheses = ((Token *)((*current)->data));
							if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
								printf("while loop expected ')'\n");
								compileError(MI, token->location);
							}
							
							*current = (*current)->next;
							endIfCurrentIsEmpty()
							Token *openingBracket = ((Token *)((*current)->data));
							if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
								printf("while loop expected '{'\n");
								compileError(MI, openingBracket->location);
							}
							
							*current = (*current)->next;
							linkedList_Node *codeBlock = parse(MI, current, ParserMode_normal);
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_while));
							
							data->nodeType = ASTnodeType_while;
							data->location = token->location;
							
							((ASTnode_while *)data->value)->expression = expression;
							((ASTnode_while *)data->value)->codeBlock = codeBlock;
							continue;
						}
						
						// function call
						else {
							*current = (*current)->next;
							linkedList_Node *arguments = parse(MI, current, ParserMode_arguments);
							
							linkedList_Node *left = NULL;
							
							ASTnode *leftData = linkedList_addNode(&left, sizeof(ASTnode) + sizeof(ASTnode_identifier));
							
							leftData->nodeType = ASTnodeType_identifier;
							leftData->location = token->location;
							((ASTnode_identifier *)leftData->value)->name = &token->subString;
							
							ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
							
							data->nodeType = ASTnodeType_call;
							data->location = token->location;
							
							((ASTnode_call *)data->value)->left = left;
							((ASTnode_call *)data->value)->arguments = arguments;
						}
					}
					
					// variable
					else {
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_identifier));
						
						data->nodeType = ASTnodeType_identifier;
						data->location = token->location;
						((ASTnode_identifier *)data->value)->name = &token->subString;
					}
				}
				break;
			}
				
			case TokenType_pound: {
				*current = (*current)->next;
				linkedList_Node *left = parseOperators(MI, current, parse(MI, current, ParserMode_noOperatorChecking), 0, 1);
				
				endIfCurrentIsEmpty()
				Token *openingParentheses = ((Token *)((*current)->data));
				if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
					printf("runMacro expected an openingParentheses\n");
					compileError(MI, openingParentheses->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *arguments = parse(MI, current, ParserMode_arguments);
				
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_runMacro));
				
				data->nodeType = ASTnodeType_runMacro;
				data->location = token->location;
				((ASTnode_runMacro *)data->value)->left = left;
				((ASTnode_runMacro *)data->value)->arguments = arguments;
				break;
			}
				
			case TokenType_ellipsis: {
				addStringToErrorMsg("unexpected ellipsis");
				compileError(MI, token->location);
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
					compileError(MI, token->location);
				}
				break;
			}
				
			case TokenType_operator: {
				addStringToErrorMsg("unexpected operator");
				compileError(MI, token->location);
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				
				data->nodeType = ASTnodeType_number;
				data->location = token->location;
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(MI, current).value;
				
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
				
			default: {
				printf("unknown token type: %u\n", token->type);
				compileError(MI, token->location);
				break;
			}
		}
		
		if (parserMode == ParserMode_normal) {
			endIfCurrentIsEmpty()
			if (CURRENT_IS_NOT_SEMICOLON) {
				printf("expected ';' after statement\n");
				compileError(MI, token->location);
			}
			*current = (*current)->next;
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
						compileError(MI, token->location);
					}
				}
				printf("expected a comma\n");
				compileError(MI, token->location);
			}
			*current = (*current)->next;
		}
	}
}
