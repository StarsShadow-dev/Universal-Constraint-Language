#ifndef parser_h
#define parser_h

#include "types.h"
#include "compiler.h"

typedef enum {
	ParserMode_codeBlock,
	ParserMode_expression,
	ParserMode_arguments
} ParserMode;

linkedList_Node *parse(ModuleInformation *MI, linkedList_Node **current, ParserMode parserMode, int returnAtNonScopeResolutionOperator, int returnAtOpeningParentheses);

#endif /* parser_h */
