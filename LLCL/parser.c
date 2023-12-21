#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "report.h"

#define endIfCurrentIsEmpty()\
if (*current == NULL) {\
	addStringToReportMsg("unexpected end of file");\
	compileError(FI, token->location);\
}

#define CURRENT_IS_NOT_SEMICOLON *current == NULL || (((Token *)((*current)->data))->type != TokenType_separator || SubString_string_cmp(&((Token *)((*current)->data))->subString, ";") != 0)

int getIfNodeShouldHaveSemicolon(ASTnode *node) {
	return node->nodeType != ASTnodeType_import &&
	node->nodeType != ASTnodeType_constrainedType &&
	node->nodeType != ASTnodeType_struct &&
	node->nodeType != ASTnodeType_implement &&
	node->nodeType != ASTnodeType_function &&
	node->nodeType != ASTnodeType_macro &&
	node->nodeType != ASTnodeType_while &&
	node->nodeType != ASTnodeType_if;
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

ASTnode_number parseInt(FileInformation *FI, linkedList_Node **current) {
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
		SubString_string_cmp(subString, "!=") == 0 ||
		SubString_string_cmp(subString, ">") == 0 ||
		SubString_string_cmp(subString, "<") == 0
	) {
		return 4;
	}
	
	else if (SubString_string_cmp(subString, "+") == 0 || SubString_string_cmp(subString, "-") == 0) {
		return 5;
	}
	
	else if (SubString_string_cmp(subString, "*") == 0 || SubString_string_cmp(subString, "/") == 0 || SubString_string_cmp(subString, "%") == 0) {
		return 6;
	}
	
	else if (SubString_string_cmp(subString, "as") == 0) {
		return 7;
	}
	
	else if (SubString_string_cmp(subString, ".") == 0) {
		return 8;
	}
	
	else if (SubString_string_cmp(subString, "::") == 0) {
		return 9;
	}
	
	else {
		printf("getOperatorPrecedence error\n");
		abort();
	}
}

linkedList_Node *parseType(FileInformation *FI, linkedList_Node **current);

linkedList_Node *parseOperators(FileInformation *FI, linkedList_Node **current, linkedList_Node *left, int precedence, int ignoreEquals) {
	if (*current == NULL) {
		return left;
	}
	
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
			linkedList_Node *right;
			if (SubString_string_cmp(&operator->subString, "as") == 0) {
				right = parseType(FI, current);
			} else if (SubString_string_cmp(&operator->subString, ".") == 0 || SubString_string_cmp(&operator->subString, "::") == 0) {
				right = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 1, 1), nextPrecedence, ignoreEquals);
			} else {
				right = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 1, 0), nextPrecedence, ignoreEquals);
			}
			
			ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_operator));
			
			data->nodeType = ASTnodeType_operator;
			data->location = operator->location;
			
			if (SubString_string_cmp(&operator->subString, "=") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_assignment;
			} else if (SubString_string_cmp(&operator->subString, "==") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_equivalent;
			} else if (SubString_string_cmp(&operator->subString, "!=") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_notEquivalent;
			} else if (SubString_string_cmp(&operator->subString, ">") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_greaterThan;
			} else if (SubString_string_cmp(&operator->subString, "<") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_lessThan;
			} else if (SubString_string_cmp(&operator->subString, "+") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_add;
			} else if (SubString_string_cmp(&operator->subString, "-") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_subtract;
			} else if (SubString_string_cmp(&operator->subString, "*") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_multiply;
			} else if (SubString_string_cmp(&operator->subString, "/") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_divide;
			} else if (SubString_string_cmp(&operator->subString, "%") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_modulo;
			} else if (SubString_string_cmp(&operator->subString, ".") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_memberAccess;
			} else if (SubString_string_cmp(&operator->subString, "::") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_scopeResolution;
			} else if (SubString_string_cmp(&operator->subString, "as") == 0) {
				((ASTnode_operator *)data->value)->operatorType = ASTnode_operatorType_cast;
			} else {
				abort();
			}
			
			((ASTnode_operator *)data->value)->left = left;
			((ASTnode_operator *)data->value)->right = right;
			
			return parseOperators(FI, current, AST, precedence, ignoreEquals);
		}
	}
	
	return left;
}

linkedList_Node *parseConstraintList(FileInformation *FI, linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			printf("unexpected end of file at constraint list\n");
			exit(1);
		}
		
		Token *token = (Token *)((*current)->data);
		if (token->type == TokenType_separator && SubString_string_cmp(&token->subString, "]") == 0) {
			*current = (*current)->next;
			return AST;
		}
		
		linkedList_Node *operatorAST = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 0, 0), 0, 0);
		linkedList_join(&AST, &operatorAST);
	}
}

linkedList_Node *parseType(FileInformation *FI, linkedList_Node **current) {
	linkedList_Node *AST = NULL;
	
	linkedList_Node *type = parseOperators(FI, current, parse(FI, current, ParserMode_expression, 1, 1), 0, 1);
	
	linkedList_Node *constraints = NULL;
	Token *token = (Token *)((*current)->data);
	if (token->type == TokenType_separator && SubString_string_cmp(&token->subString, "[") == 0) {
		*current = (*current)->next;
		constraints = parseConstraintList(FI, current);
	}
	
	ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_constrainedType));
	data->nodeType = ASTnodeType_constrainedType;
	data->location = ((Token *)((*current)->data))->location;
	
	((ASTnode_constrainedType *)data->value)->type = type;
	((ASTnode_constrainedType *)data->value)->constraints = constraints;
	
	return AST;
}

typedef struct {
	linkedList_Node *list1;
	linkedList_Node *list2;
} linkedList_Node_tuple;

linkedList_Node_tuple parseFunctionArguments(FileInformation *FI, linkedList_Node **current) {
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
					compileError(FI, colon->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *type = parseType(FI, current);
				
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
							compileError(FI, token->location);
						}
					}
					printf("expected a comma\n");
					compileError(FI, token->location);
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
					compileError(FI, token->location);
				}
				break;
			}
			
			default: {
				printf("unexpected token type: %u inside of function arguments\n", token->type);
				compileError(FI, token->location);
				break;
			}
		}
		
		*current = (*current)->next;
	}
}

void addQueryLocation(linkedList_Node **AST) {
	ASTnode *data = linkedList_addNode(AST, sizeof(ASTnode));
	
	data->nodeType = ASTnodeType_queryLocation;
	data->location = (SourceLocation){0};
	addedQueryLocation = 1;
}

linkedList_Node *parse(FileInformation *FI, linkedList_Node **current, ParserMode parserMode, int returnAtNonScopeResolutionOperator, int returnAtOpeningSeparator) {
	linkedList_Node *AST = NULL;
	
	while (1) {
		if (*current == NULL) {
			if (compilerMode == CompilerMode_query && queryMode == QueryMode_suggestions && !addedQueryLocation) {
				addQueryLocation(&AST);
			}
			return AST;
		}
		
		Token *token = (Token *)((*current)->data);
		
		if (compilerMode == CompilerMode_query && queryMode == QueryMode_suggestions && strcmp(FI->context.path, startFilePath) == 0) {
			if (!addedQueryLocation && token->location.line >= queryLine) {
				addQueryLocation(&AST);
			}
			
			if (token->location.line == queryLine) {
				*current = (*current)->next;
				continue;
			}
		}
		
		switch (token->type) {
			case TokenType_word: {
				// import statement
				if (SubString_string_cmp(&token->subString, "import") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *pathString = ((Token *)((*current)->data));
					
					if (pathString->type != TokenType_string) {
						addStringToReportIndicator("expected string as part of import keyword");
						compileError(FI, pathString->location);
					}
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_import));
					
					data->nodeType = ASTnodeType_import;
					data->location = token->location;
					
					((ASTnode_import *)data->value)->path = &pathString->subString;
					
					*current = (*current)->next;
				}
				
				// macro definition
				else if (SubString_string_cmp(&token->subString, "macro") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after macro keyword\n");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("struct expected '{'\n");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *codeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_macro));
					data->nodeType = ASTnodeType_macro;
					data->location = nameToken->location;
					
					((ASTnode_macro *)data->value)->name = &nameToken->subString;
					((ASTnode_macro *)data->value)->codeBlock = codeBlock;
				}
				
				// struct
				else if (SubString_string_cmp(&token->subString, "struct") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after struct keyword\n");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("struct expected '{'\n");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *block = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_struct));
					
					data->nodeType = ASTnodeType_struct;
					data->location = token->location;
					
					((ASTnode_struct *)data->value)->name = &nameToken->subString;
					((ASTnode_struct *)data->value)->block = block;
				}
				
				else if (SubString_string_cmp(&token->subString, "impl") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						addStringToReportMsg("expected word after impl keyword");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						addStringToReportMsg("impl expected '{'");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *block = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_implement));
					
					data->nodeType = ASTnodeType_implement;
					data->location = token->location;
					
					((ASTnode_implement *)data->value)->name = &nameToken->subString;
					((ASTnode_implement *)data->value)->block = block;
				}
				
				// function definition
				else if (SubString_string_cmp(&token->subString, "function") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					
					if (nameToken->type != TokenType_word) {
						printf("expected word after function keyword\n");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						printf("function definition expected an openingParentheses\n");
						compileError(FI, openingParentheses->location);
					}
					
					*current = (*current)->next;
					linkedList_Node_tuple argument_tuple = parseFunctionArguments(FI, current);
					
					linkedList_Node *argumentNames = argument_tuple.list1;
					linkedList_Node *argumentTypes = argument_tuple.list2;
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("function definition expected a colon\n");
						compileError(FI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *returnType = (ASTnode *)parseType(FI, current)->data;
					
					Token *codeStart = ((Token *)((*current)->data));
					
					if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, "{") == 0) {
						*current = (*current)->next;
						linkedList_Node *codeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 0;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = codeBlock;
					} else if (codeStart->type == TokenType_separator && SubString_string_cmp(&codeStart->subString, ";") == 0) {
						*current = (*current)->next;
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_function));
						
						data->nodeType = ASTnodeType_function;
						data->location = nameToken->location;
						
						((ASTnode_function *)data->value)->name = &nameToken->subString;
						((ASTnode_function *)data->value)->external = 1;
						((ASTnode_function *)data->value)->returnType = returnType;
						((ASTnode_function *)data->value)->argumentNames = argumentNames;
						((ASTnode_function *)data->value)->argumentTypes = argumentTypes;
						((ASTnode_function *)data->value)->codeBlock = NULL;
					} else {
						printf("function definition expected an openingBracket or a semicolon\n");
						compileError(FI, codeStart->location);
					}
				}
				
				// while loop
				else if (SubString_string_cmp(&token->subString, "while") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						printf("while loop expected an openingParentheses\n");
						compileError(FI, openingParentheses->location);
					}
					*current = (*current)->next;
					linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
					
					endIfCurrentIsEmpty()
					Token *closingParentheses = ((Token *)((*current)->data));
					if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
						printf("while loop expected ')'\n");
						compileError(FI, token->location);
					}
					
					*current = (*current)->next;
					
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("while loop expected '{'\n");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *codeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_while));
					
					data->nodeType = ASTnodeType_while;
					data->location = token->location;
					
					((ASTnode_while *)data->value)->expression = expression;
					((ASTnode_while *)data->value)->codeBlock = codeBlock;
				}
				
				// if statement
				else if (SubString_string_cmp(&token->subString, "if") == 0) {
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *openingParentheses = ((Token *)((*current)->data));
					
					if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
						printf("if statement expected an openingParentheses\n");
						compileError(FI, openingParentheses->location);
					}
					*current = (*current)->next;
					linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
					
					endIfCurrentIsEmpty()
					Token *closingParentheses = ((Token *)((*current)->data));
					if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
						printf("if statement expected ')'\n");
						compileError(FI, token->location);
					}
					*current = (*current)->next;
					
					endIfCurrentIsEmpty()
					Token *openingBracket = ((Token *)((*current)->data));
					if (openingBracket->type != TokenType_separator || SubString_string_cmp(&openingBracket->subString, "{") != 0) {
						printf("if statement expected '{'\n");
						compileError(FI, openingBracket->location);
					}
					
					*current = (*current)->next;
					linkedList_Node *trueCodeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_if));
					
					data->nodeType = ASTnodeType_if;
					data->location = token->location;
					
					((ASTnode_if *)data->value)->expression = expression;
					((ASTnode_if *)data->value)->trueCodeBlock = trueCodeBlock;
					
					if (*current != NULL && SubString_string_cmp(&(((Token *)((*current)->data)))->subString, "else") == 0) {
						*current = (*current)->next;
						*current = (*current)->next;
						linkedList_Node *falseCodeBlock = parse(FI, current, ParserMode_codeBlock, 0, 0);
						
						((ASTnode_if *)data->value)->falseCodeBlock = falseCodeBlock;
					} else {
						((ASTnode_if *)data->value)->falseCodeBlock = NULL;
					}
				}
				
				// return statement
				else if (SubString_string_cmp(&token->subString, "return") == 0) {
					*current = (*current)->next;
					if (CURRENT_IS_NOT_SEMICOLON) {
						linkedList_Node *expression = parse(FI, current, ParserMode_expression, 0, 0);
						
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
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *nameToken = ((Token *)((*current)->data));
					if (nameToken->type != TokenType_word) {
						printf("expected word after var keyword\n");
						compileError(FI, nameToken->location);
					}
					
					*current = (*current)->next;
					endIfCurrentIsEmpty()
					Token *colon = ((Token *)((*current)->data));
					if (colon->type != TokenType_separator || SubString_string_cmp(&colon->subString, ":") != 0) {
						printf("variable definition expected a colon\n");
						compileError(FI, colon->location);
					}
					
					*current = (*current)->next;
					ASTnode *type = (ASTnode *)parseType(FI, current)->data;
					
					linkedList_Node *expression = NULL;
					
					Token *equals = ((Token *)((*current)->data));
					if (equals->type == TokenType_operator && SubString_string_cmp(&equals->subString, "=") == 0) {
						*current = (*current)->next;
						expression = parse(FI, current, ParserMode_expression, 0, 0);
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
				
				// identifier
				else {
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_identifier));
					data->nodeType = ASTnodeType_identifier;
					data->location = token->location;
					((ASTnode_identifier *)data->value)->name = &token->subString;
					
					*current = (*current)->next;
				}
				break;
			}
				
			case TokenType_number: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_number));
				data->nodeType = ASTnodeType_number;
				data->location = token->location;
				((ASTnode_number *)data->value)->string = &token->subString;
				((ASTnode_number *)data->value)->value = parseInt(FI, current).value;
				
				*current = (*current)->next;
				break;
			}
				
			case TokenType_pound: {
				*current = (*current)->next;
				linkedList_Node *left = parse(FI, current, ParserMode_expression, 0, 1);
				
				endIfCurrentIsEmpty()
				Token *openingParentheses = ((Token *)((*current)->data));
				if (openingParentheses->type != TokenType_separator || SubString_string_cmp(&openingParentheses->subString, "(") != 0) {
					printf("runMacro expected an openingParentheses\n");
					compileError(FI, openingParentheses->location);
				}
				
				*current = (*current)->next;
				linkedList_Node *arguments = parse(FI, current, ParserMode_arguments, 0, 0);
				
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_runMacro));
				
				data->nodeType = ASTnodeType_runMacro;
				data->location = token->location;
				((ASTnode_runMacro *)data->value)->left = left;
				((ASTnode_runMacro *)data->value)->arguments = arguments;
				break;
			}
				
			case TokenType_ellipsis: {
				addStringToReportMsg("unexpected ellipsis");
				compileError(FI, token->location);
				break;
			}
			
			case TokenType_separator: {
				if (SubString_string_cmp(&token->subString, "(") == 0) {
					if (returnAtOpeningSeparator) {
						return AST;
					}
					
					int forFunction = 1;
					
					if (AST != NULL) {
						linkedList_Node *last = linkedList_getLast(AST);
						if (
							((ASTnode *)last->data)->nodeType == ASTnodeType_identifier ||
							((ASTnode *)last->data)->nodeType == ASTnodeType_operator
						) {
							forFunction = 1;
						} else {
							forFunction = 0;
						}
					} else {
						forFunction = 0;
					}
					
					if (forFunction) {
						linkedList_Node *left = linkedList_popLast(&AST);
						ASTnode *leftNode = (ASTnode *)left->data;
						
						*current = (*current)->next;
						linkedList_Node *arguments = parse(FI, current, ParserMode_arguments, 0, 0);
						
						ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_call));
						
						data->nodeType = ASTnodeType_call;
						data->location = leftNode->location;
						
						((ASTnode_call *)data->value)->left = left;
						((ASTnode_call *)data->value)->arguments = arguments;
					} else {
						*current = (*current)->next;
						linkedList_Node *newAST = parse(FI, current, ParserMode_expression, 0, 0);
						linkedList_join(&AST, &newAST);
						Token *closingParentheses = ((Token *)((*current)->data));
						if (closingParentheses->type != TokenType_separator || SubString_string_cmp(&closingParentheses->subString, ")") != 0) {
							printf("expected closingParentheses\n");
							compileError(FI, closingParentheses->location);
						}
						*current = (*current)->next;
					}
				} else if (SubString_string_cmp(&token->subString, "[") == 0) {
					if (returnAtOpeningSeparator) {
						return AST;
					}
					
					linkedList_Node *left = linkedList_popLast(&AST);
					ASTnode *leftNode = (ASTnode *)left->data;
					
					*current = (*current)->next;
					linkedList_Node *right = parse(FI, current, ParserMode_expression, 0, 0);
					Token *closingBracket = ((Token *)((*current)->data));
					if (closingBracket->type != TokenType_separator || SubString_string_cmp(&closingBracket->subString, "]") != 0) {
						printf("subscript expected a closingBracket\n");
						compileError(FI, closingBracket->location);
					}
					*current = (*current)->next;
					
					ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode) + sizeof(ASTnode_subscript));
					
					data->nodeType = ASTnodeType_subscript;
					data->location = leftNode->location;
					
					((ASTnode_subscript *)data->value)->left = left;
					((ASTnode_subscript *)data->value)->right = right;
				} else if (
					SubString_string_cmp(&token->subString, ")") == 0 ||
					SubString_string_cmp(&token->subString, "}") == 0 ||
					SubString_string_cmp(&token->subString, "]") == 0
				) {
					if (parserMode != ParserMode_expression) {
						*current = (*current)->next;
					}
					return AST;
				} else if (parserMode != ParserMode_expression) {
					printf("unexpected separator: '");
					SubString_print(&token->subString);
					printf("'\n");
					compileError(FI, token->location);
				}
				break;
			}
			
			case TokenType_operator: {
				if (returnAtNonScopeResolutionOperator && SubString_string_cmp(&token->subString, "::") != 0) {
					return AST;
				}
				linkedList_Node *left = linkedList_popLast(&AST);
				linkedList_Node *operatorAST = parseOperators(FI, current, left, 0, 0);
				linkedList_join(&AST, &operatorAST);
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
			
			case TokenType_selfReference: {
				ASTnode *data = linkedList_addNode(&AST, sizeof(ASTnode));
				
				data->nodeType = ASTnodeType_selfReference;
				data->location = token->location;
				
				*current = (*current)->next;
				break;
			}
			
			default: {
				printf("unknown token type: %u\n", token->type);
				compileError(FI, token->location);
				break;
			}
		}
		
		ASTnode *lastNode = (ASTnode *)linkedList_getLast(AST)->data;
		
		if (*current == NULL) {
			if (getIfNodeShouldHaveSemicolon(lastNode)) {
				addStringToReportMsg("expected ';' after statement, but the file ended");
				compileError(FI, lastNode->location);
			}
			if (compilerMode == CompilerMode_query && queryMode == QueryMode_suggestions && !addedQueryLocation) {
				addQueryLocation(&AST);
			}
			return AST;
		}
		
		if (!returnAtNonScopeResolutionOperator && ((Token *)((*current)->data))->type == TokenType_operator) {
			continue;
		}
		
		if (
			((Token *)((*current)->data))->type == TokenType_separator &&
			SubString_string_cmp(&((Token *)((*current)->data))->subString, ")") == 0
		) {
			continue;
		}
		
		if (
			!returnAtOpeningSeparator &&
			((Token *)((*current)->data))->type == TokenType_separator &&
			(
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "(") == 0 ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "[") == 0
			)
		) {
			continue;
		}
		
		if (parserMode == ParserMode_arguments) {
			if (
				((Token *)((*current)->data))->type != TokenType_separator ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, ",") != 0
			) {
				addStringToReportMsg("expected ',' inside of argument list");
				compileError(FI, lastNode->location);
			}
			*current = (*current)->next;
		}
		
		int nextTokenWillMoveLastNode = ((Token *)((*current)->data))->type == TokenType_operator ||
		(
			((Token *)((*current)->data))->type == TokenType_separator &&
			(
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "(") == 0 ||
				SubString_string_cmp(&((Token *)((*current)->data))->subString, "[") == 0
			)
		);
		
		// if the node that was just generated should have a semicolon
		if (parserMode == ParserMode_codeBlock && !nextTokenWillMoveLastNode && getIfNodeShouldHaveSemicolon(lastNode)) {
			if (CURRENT_IS_NOT_SEMICOLON) {
				addStringToReportMsg("expected ';' after statement");
				compileError(FI, lastNode->location);
			}
			*current = (*current)->next;
		}
		
		if (!returnAtNonScopeResolutionOperator && nextTokenWillMoveLastNode) {
			continue;
		}
		
		if (parserMode == ParserMode_expression) {
			return AST;
		}
	}
}
