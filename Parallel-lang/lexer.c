#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "globals.h"
#include "error.h"

#define wordStart (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_'
#define wordContinue wordStart || (character >= '0' && character <= '9')

#define numberStart (character >= '0' && character <= '9')
#define numberContinue (character >= '0' && character <= '9')

// periods are considered operators because they perform an operation
#define operator_1char character == '>' || character == '<' || character == '=' || character == '+' || character == '-' || character == '.'
#define operator_2chars character == '=' && MI->currentSource[index+1] == '=' || character == ':' && MI->currentSource[index+1] == ':'

#define separator character == '(' || character == ')' || character == '{' || character == '}'  || character == '[' || character == ']' || character == ':' || character == ';' || character == ','

linkedList_Node *lex(ModuleInformation *MI) {
	linkedList_Node *tokens = 0;
	
	int index = 0;
	int line = 1;
	int column = 0;
	
	if (MI->currentSource == NULL) {
		return NULL;
	}
	
	while (1) {
		char character = MI->currentSource[index];
		
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
				char character = MI->currentSource[index];
				
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
			data->subString.start = MI->currentSource + start;
			data->subString.length = end - start;
		}
		
		else if (operator_2chars) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_operator;
			data->location = (SourceLocation){line, column, column + 2};
			data->subString.start = MI->currentSource + index;
			data->subString.length = 2;
			
			index++;
			column++;
		}
		
		else if (operator_1char) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_operator;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = MI->currentSource + index;
			data->subString.length = 1;
		}
		
		else if (separator) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_separator;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = MI->currentSource + index;
			data->subString.length = 1;
		}
		
		else if (numberStart) {
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = MI->currentSource[index];
				
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
			data->subString.start = MI->currentSource + start;
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
				char character = MI->currentSource[index];
				
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
			data->location = (SourceLocation){line, columnStart - 1, columnStart + end - start + 1};
			data->subString.start = MI->currentSource + start;
			data->subString.length = end - start;
		}
		
		// comment
		else if (character == '/' && MI->currentSource[index+1] == '/') {
			index++;
			column++;
			
			while (1) {
				char character = MI->currentSource[index];
				
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
		else if (character == '/' && MI->currentSource[index+1] == '*') {
			index++;
			column++;
			
			while (1) {
				char character = MI->currentSource[index];
				
				if (character == 0) {
					index--;
					column--;
					break;
				}
				
				if (character == '\n') {
					line++;
				} else if (character == '*' && MI->currentSource[index+1] == '/') {
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
			compileError(MI, (SourceLocation){line, column, column + 1});
		}
		
		index++;
		column++;
	}
	
	return tokens;
}
