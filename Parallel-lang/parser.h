#ifndef parser_h
#define parser_h

#include "types.h"

typedef enum {
	ParserMode_normal,
	ParserMode_expression,
	ParserMode_arguments,
	
	ParserMode_noOperatorChecking
} ParserMode;

linkedList_Node *parse(linkedList_Node **current, ParserMode parserMode);

#endif /* parser_h */
