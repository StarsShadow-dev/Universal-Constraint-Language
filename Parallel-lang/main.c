#include <stdio.h>
#include <string.h>

#include "main.h"
#include "types.h"
#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include "jsmn.h"

/// print the tokens to standard out in a form resembling JSON
void printTokens(linkedList_Node *head) {
	if (head == 0) {
		printf("tokens: []\n");
		return;
	}
	
	printf("tokens: [\n");
	
	linkedList_Node *current = head;
	while (1) {
		printf(
			"\t{type: %i, value: \"%s\", line: %i, start: %i, end: %i}",
			((Token *)(current->data))->type,
			((Token *)(current->data))->value,
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

char *readFile(const char *path) {
	FILE* file = fopen(path, "r");
	
	if (file == 0) {
		printf("could not find file at: %s\n", path);
		exit(1);
	}

	// get the size of the file
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);
	
	char *buffer = malloc(fileSize + 1);
	
	if (buffer == NULL) {
		printf("malloc failed to get space for a file buffer\n");
		exit(1);
	}
	
	if (fread(buffer, sizeof(char), fileSize, file) != fileSize) {
		printf("fread failed\n");
		exit(1);
	}
	buffer[fileSize] = 0;
	
	fclose(file);
	
	return buffer;
}

/// this function only works if the JSON has only strings
char *getJsmnString(char *buffer, jsmntok_t *t, int count, char * key) {
	if (count < 0) { return NULL; }
	
	if (t[0].type != JSMN_OBJECT) { return NULL; }
	
	unsigned long keyLen = strlen(key);
	
	int i = 1;
	
	while (i < count) {
		// now look for key/value pairs.
		
		jsmntok_t keyToken = t[i];
		if (keyToken.type != JSMN_STRING) {
			// the "key" is not a string.
			return NULL;
		}
		if (keyToken.size != 1) {
			// keys should have a single value.
			return NULL;
		}

		jsmntok_t valueToken = t[i+1];
		if (valueToken.type != JSMN_STRING) {
			return NULL;
		}

		char *keyValue = buffer + keyToken.start;
		if (memcmp(keyValue, key, keyLen) == 0) {
			unsigned long stringLen = valueToken.end - valueToken.start;
			char *stringValue = buffer + valueToken.start;
			
			char *result = malloc(stringLen);
			memcpy(result, stringValue, stringLen);
			return result;
		}
		
		i+=2;
	}
	return NULL;
}

int main(int argc, char **argv) {
	source = "function main()\n{}";

	linkedList_Node *tokens = lex();
	printTokens(tokens);
	
	linkedList_Node *AST = parse(tokens);
	
	// clean up
	linkedList_freeList(&tokens);
	linkedList_freeList(&AST);
	
//	char *homePath = getenv("HOME");
//	char *globalConfigRelativePath = "/.Parallel_Lang/config.json";
//	char *globalConfigPath = malloc(strlen(homePath) + strlen(globalConfigRelativePath));
//	sprintf(globalConfigPath, "%s%s", homePath, globalConfigRelativePath);
//
//	char *globalConfigJSON = readFile(globalConfigPath);
//	printf("globalConfigJSON: %s\n", globalConfigJSON);
//
//	jsmn_parser p;
//	jsmntok_t t[128] = {}; // expect no more than 128 JSON tokens
//	jsmn_init(&p);
//	int count = jsmn_parse(&p, globalConfigJSON, strlen(globalConfigJSON), t, 128);
//
//	char *LLC_path = getJsmnString(globalConfigJSON, t, count, "LLC_path");
//	if (LLC_path == 0 || LLC_path[0] == 0) {
//		printf("no LLC_path in file at: %s\n", globalConfigPath);
//		exit(1);
//	}
//	printf("LLC_path: %s\n", LLC_path);
//
//	char *clang_path = getJsmnString(globalConfigJSON, t, count, "clang_path");
//	if (clang_path == 0 || clang_path[0] == 0) {
//		printf("no clang_path in file at: %s\n", globalConfigPath);
//		exit(1);
//	}
//	printf("clang_path: %s\n", clang_path);
//
//	free(globalConfigPath);
//	free(globalConfigJSON);
//
//	free(LLC_path);
//	free(clang_path);
	
	return 0;
}
