#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "globals.h"
#include "error.h"

#define wordStart (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_'
#define wordContinue wordStart || (character >= '0' && character <= '9')

#define numberStart (character >= '0' && character <= '9')
#define numberContinue (character >= '0' && character <= '9')

#define separator character == '(' || character == ')' || character == '{' || character == '}'  || character == '[' || character == ']' || character == ':' || character == ';' || character == ','

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
			line++;
			column = 0;
			
			index++;
			continue;
		}
		
		// ignore space and tab
		else if (character == ' ' || character == '\t') {}
		
		// start of a word
		else if (wordStart) {
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = source[index];
				
				if (character != 0 && wordContinue) {
					index++;
					column++;
					continue;
				}
				
				end = index;
				
				index--;
				column--;
				break;
			}
			
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_word;
			data->location = (SourceLocation){line, columnStart, columnStart + end - start};
			data->subString.start = source + start;
			data->subString.length = end - start;
		}
		
		else if (separator) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_separator;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = source + index;
			data->subString.length = 1;
		}
		
		else if (numberStart) {
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = source[index];
				
				if (character != 0 && numberContinue) {
					index++;
					column++;
					continue;
				}
				
				end = index;
				
				index--;
				column--;
				break;
			}
			
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_number;
			data->location = (SourceLocation){line, columnStart, columnStart + end - start};
			data->subString.start = source + start;
			data->subString.length = end - start;
		}
		
		else if (character == '"') {
			// eat the starting quotation mark
			index++;
			column++;
			
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = source[index];
				
				if (character != 0 && character != '"') {
					index++;
					column++;
					if (character == '\n') {
						line++;
						column = 0;
					}
					continue;
				}
				
				end = index;
				
				index--;
				column--;
				
				if (character == '"') {
					// eat the ending quotation mark
					index++;
					column++;
				}
				break;
			}
			
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_string;
			data->location = (SourceLocation){line, columnStart, columnStart + end - start};
			data->subString.start = source + start;
			data->subString.length = end - start;
		}
		
		// comment
		else if (character == '/' && source[index+1] == '/') {
			index++;
			column++;
			
			while (1) {
				char character = source[index];
				
				if (character != 0 && character != '\n') {
					index++;
					column++;
					continue;
				}
				
				index--;
				column--;
				break;
			}
		}
		
		// block comment
		else if (character == '/' && source[index+1] == '*') {
			index++;
			column++;
			
			while (1) {
				char character = source[index];
				
				if (character == 0) {
					index--;
					column--;
					break;
				}
				
				if (character == '*' && source[index+1] == '/') {
					index++;
					column++;
					break;
				}
				
				index++;
				column++;
			}
		}
		
		else {
			printf("unexpected character '%c'\n", character);
			compileError((SourceLocation){line, column, column + 1});
		}
		
		index++;
		column++;
	}
	
	return tokens;
}
