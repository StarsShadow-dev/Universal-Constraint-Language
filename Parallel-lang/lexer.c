#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "lexer.h"
#include "error.h"

#define wordStart (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_'
#define wordContinue wordStart || (character >= '0' && character <= '9')

#define separator character == '(' || character == ')' || character == '{' || character == '}' || character == ':'

linkedList_Node *lex(void) {
	linkedList_Node *tokens = 0;
	
	int index = 0;
	int line = 1;
	int column = 0;
	
	while (1) {
		char character = source[index];
		
		// if character is a NULL byte, then the string is over
		if (character == 0) {
			break;
		}
		
//		printf("character: %c\n", character);
		
		if (character == '\n') {
			line += 1;
			column = 0;
			
			index += 1;
			continue;
		}
		
		// ignore space and tab
		else if (character == ' ' || character == '\t') {}
		
		// start of a word
		else if (wordStart) {
			int start = index;
			int end = 0;
			
			while (1) {
				char character = source[index];
				
				if (character == 0) {
					end = index;
					
					index -= 1;
					break;
				}
				
				if (wordContinue) {
					index += 1;
					continue;
				}
				
				end = index;
				
				index -= 1;
				break;
			}
			
			// plus one because of the NULL bite
			int valueSize = end - start + 1;
			
			token *data = linkedList_addNode(&tokens, sizeof(token) + valueSize);
			
			data->type = TokenType_word;
			
			data->location = (SourceLocation){line, start, end};
			
			stpncpy(data->value, &source[start], valueSize - 1);
			data->value[valueSize] = 0;
		}
		
		else if (separator) {
			token *data = linkedList_addNode(&tokens, sizeof(token) + 2);
			
			data->type = TokenType_separator;
			
			data->location = (SourceLocation){line, index, index};
			
			stpncpy(data->value, &source[index], 1);
			data->value[1] = 0;
		}
		
		else {
			printf("unexpected character '%c'\n", character);
			compileError((SourceLocation){});
		}
		
		index += 1;
	}
	
	return tokens;
}
