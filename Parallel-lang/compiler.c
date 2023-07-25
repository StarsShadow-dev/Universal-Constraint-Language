#include <stdio.h>

#include "compiler.h"
#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include "builder.h"

/// print the tokens to standard out in a form resembling JSON
void printTokens(linkedList_Node *head) {
	if (head == NULL) {
		printf("Tokens: []\n");
		return;
	}
	
	printf("Tokens: [\n");
	
	linkedList_Node *current = head;
	while (1) {
		printf(
			"\t{type: %i, value: \"",
			((Token *)(current->data))->type
		);
		
		SubString_print(&((Token *)(current->data))->subString);
		
		printf("\", line: %i, start: %i, end: %i}",
			((Token *)(current->data))->location.line,
			((Token *)(current->data))->location.columnStart,
			((Token *)(current->data))->location.columnEnd
		);
		
		if (current->next == NULL) break;
		
		printf(",\n");
		current = current->next;
	}
	
	printf("\n]\n");
}

void compile(int compilerTesting, CharAccumulator *LLVMsource) {
	linkedList_Node *tokens = lex();
#ifdef COMPILER_DEBUG_MODE
	if (!compilerTesting) {
		printTokens(tokens);
	}
#endif
	
	linkedList_Node *currentToken = tokens;
	
	linkedList_Node *AST = parse(&currentToken, ParserMode_normal);
	
	// declare malloc
	CharAccumulator_appendChars(LLVMsource, "\n; for the new keyword\ndeclare ptr @malloc(i64 noundef) allocsize(0)");
	
	CharAccumulator LLVMconstantSource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMconstantSource);
	
	GlobalBuilderInformation GBI = {
		&LLVMconstantSource,
		0,
		
		// level is -1 so that it starts at 0 for the first iteration
		-1,
		{0}
	};
	
	addBuilderType(&GBI.variables[0], &(SubString){"Void", (int)strlen("Void")}, "void", 0, 4);
	addBuilderType(&GBI.variables[0], &(SubString){"Int8", (int)strlen("Int8")}, "i8", 1, 4);
	addBuilderType(&GBI.variables[0], &(SubString){"Int32", (int)strlen("Int32")}, "i32", 4, 4);
	addBuilderType(&GBI.variables[0], &(SubString){"Int64", (int)strlen("Int64")}, "i64", 8, 4);
	// how much space should be made for an i1?
	// I will do one byte for now
	addBuilderType(&GBI.variables[0], &(SubString){"Bool", (int)strlen("Bool")}, "i1", 1, 4);
	addBuilderType(&GBI.variables[0], &(SubString){"Pointer", (int)strlen("Pointer")}, "ptr", pointer_byteSize, pointer_byteSize);
	
	buildLLVM(&GBI, NULL, LLVMsource, NULL, NULL, NULL, AST, 0, 0, 0);
	
	CharAccumulator_appendChars(LLVMsource, LLVMconstantSource.data);
	CharAccumulator_free(&LLVMconstantSource);
	
	linkedList_freeList(&tokens);
}
