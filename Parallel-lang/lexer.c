#include <stdio.h>
#include <string.h>

#include "globals.h"
#include "lexer.h"
#include "error.h"

#define wordStart (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_'
#define wordContinue wordStart || (character >= '0' && character <= '9')

void lex(void) {
	int i = 0;
	
	while (1) {
		char character = source[i];
		
		// if character is a NULL byte, then the string is over
		if (character == 0) {
			return;
		}
		
		printf("character: %c\n", character);
		
		// ignore space and tab
		if (character == ' ' || character == '\t') {
			
		}
		
		// start of a word
		else if (wordStart) {
			int start = i;
			int end = 0;
			
			while (1) {
				char character = source[i];
				
				if (character == 0) {
					end = i;
					
					i -= 1;
					break;
				}
				
				if (wordContinue) {
					i += 1;
					continue;
				}
				
				end = i;
				break;
			}
			
			// plus one because of the NULL bite
			int valueSize = end - start + 1;
			
			token *data = linkedList_addNode(&tokens, sizeof(token) + valueSize);
			
			data->type = TokenType_word;
			
			data->location = (SourceLocation){0, start, end};
			
			stpncpy(data->value, &source[start], valueSize - 1);
			data->value[valueSize] = 0;
		}
		
		else {
			printf("unexpected character '%c'\n", character);
			compileError((SourceLocation){});
		}
		
		i += 1;
	}
}
