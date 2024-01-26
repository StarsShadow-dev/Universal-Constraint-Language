#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "globals.h"
#include "report.h"

#define wordStart (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_'
#define wordContinue wordStart || (character >= '0' && character <= '9')

#define numberStart (character >= '0' && character <= '9')
#define numberContinue (character >= '0' && character <= '9')

#define selfReference character == '$'

#define ellipsis character == '.' && FI->context.currentSource[index+1] == '.' && FI->context.currentSource[index+2] == '.'

// periods are considered operators because they perform an operation
#define operator_1char character == '>' || character == '<' || character == '=' || character == '+' || character == '-' || character == '*' || character == '/' || character == '%' || character == '.'
#define operator_2chars \
character == '=' && FI->context.currentSource[index+1] == '=' ||\
character == '!' && FI->context.currentSource[index+1] == '=' ||\
character == ':' && FI->context.currentSource[index+1] == ':' ||\
character == '&' && FI->context.currentSource[index+1] == '&' ||\
character == '|' && FI->context.currentSource[index+1] == '|' ||\
character == 'a' && FI->context.currentSource[index+1] == 's'

#define separator character == '(' || character == ')' || character == '{' || character == '}'  || character == '[' || character == ']' || character == ':' || character == ';' || character == ','

linkedList_Node *lex(FileInformation *FI) {
	linkedList_Node *tokens = 0;
	
	int index = 0;
	int line = 1;
	int column = 0;
	
	int lineBreaksInARow = 0;
	
	if (FI->context.currentSource == NULL) {
		return NULL;
	}
	
	while (1) {
		char character = FI->context.currentSource[index];
		
		// if character is a NULL byte, then the string is over
		if (character == 0) {
			break;
		}
		
		if (character == '\n') {
			line++;
			column = 0;
			
			index++;
			
			lineBreaksInARow++;
			if (lineBreaksInARow == 5) {
				// warning test
				addStringToReportMsg("4 empty lines in a row");
				compileWarning(FI, (SourceLocation){line, 0, 0}, WarningType_format);
			}
			
			continue;
		}
		
		// ignore space and tab
		else if (character == ' ' || character == '\t') {
			index++;
			column++;
			continue;
		}
		
		lineBreaksInARow = 0;
		
		// comment
		if (character == '/' && FI->context.currentSource[index+1] == '/') {
			index++;
			column++;
			
			while (1) {
				char character = FI->context.currentSource[index];
				
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
		else if (character == '/' && FI->context.currentSource[index+1] == '*') {
			index++;
			column++;
			
			while (1) {
				char character = FI->context.currentSource[index];
				
				if (character == 0) {
					index--;
					column--;
					break;
				}
				
				if (character == '\n') {
					line++;
				} else if (character == '*' && FI->context.currentSource[index+1] == '/') {
					index++;
					column++;
					break;
				}
				
				index++;
				column++;
			}
		}
		
		else if (operator_2chars) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_operator;
			data->location = (SourceLocation){line, column, column + 2};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 2;
			
			index++;
			column++;
		}
		
		else if (operator_1char) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_operator;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 1;
		}
		
		else if (wordStart) {
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = FI->context.currentSource[index];
				
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
			data->subString.start = FI->context.currentSource + start;
			data->subString.length = end - start;
		}
		
		else if (numberStart) {
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = FI->context.currentSource[index];
				
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
			data->subString.start = FI->context.currentSource + start;
			data->subString.length = end - start;
		}
		
		else if (selfReference) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_selfReference;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 1;
		}
		
		else if (character == '#') {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_pound;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 1;
		}
		
		else if (character == '@') {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_floatingType;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 1;
		}
		
		else if (ellipsis) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_ellipsis;
			data->location = (SourceLocation){line, column, column + 3};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 3;
			
			index += 2;
			column += 2;
		}
		
		else if (separator) {
			Token *data = linkedList_addNode(&tokens, sizeof(Token));
			
			data->type = TokenType_separator;
			data->location = (SourceLocation){line, column, column + 1};
			data->subString.start = FI->context.currentSource + index;
			data->subString.length = 1;
		}
		
		else if (character == '"') {
			// eat the starting quotation mark
			index++;
			column++;
			
			int start = index;
			int columnStart = column;
			int end = 0;
			
			while (1) {
				char character = FI->context.currentSource[index];
				
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
			data->subString.start = FI->context.currentSource + start;
			data->subString.length = end - start;
		}
		
		else if (character == '\r') {}
		
		else {
			printf("unexpected character '%c' (%d)\n", character, character);
			compileError(FI, (SourceLocation){line, column, column + 1});
		}
		
		index++;
		column++;
	}
	
	return tokens;
}
