#ifndef parser_h
#define parser_h

#include "types.h"

typedef enum {
	ParserMode_normal,
	ParserMode_expression,
	ParserMode_arguments,
} ParserMode;

linkedList_Node *parse(linkedList_Node **token, ParserMode parserMode);

#endif /* parser_h */
