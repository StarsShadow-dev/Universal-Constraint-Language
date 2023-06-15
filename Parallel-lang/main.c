#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "main.h"
#include "types.h"
#include "globals.h"
#include "jsmn.h"
#include "lexer.h"
#include "parser.h"
#include "builder.h"

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
		printf("malloc failed\n");
		abort();
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
			
			char *result = malloc(stringLen + 1);
			if (result == NULL) {
				printf("malloc failed\n");
				abort();
			}
			memcpy(result, stringValue, stringLen);
			result[stringLen] = 0;
			return result;
		}
		
		i+=2;
	}
	return NULL;
}

//void free_AST(linkedList_Node **head) {
//
//}

typedef enum {
	CompilerMode_build,
	CompilerMode_run
} CompilerMode;

int main(int argc, char **argv) {
	CompilerMode compilerMode;
	
	if (argc != 2) {
		printf("unexpected amount of arguments, expected: 1");
		exit(1);
	}
	
	if (strcmp(argv[1], "build") == 0) {
		compilerMode = CompilerMode_build;
	}
	
	else if (strcmp(argv[1], "run") == 0) {
		compilerMode = CompilerMode_run;
	}
	
	else {
		printf("unexpected compiler mode: %s\n", argv[1]);
		exit(1);
	}
	
	jsmn_parser p;
	jsmntok_t t[128] = {}; // expect no more than 128 JSON tokens
	
	char *homePath = getenv("HOME");
	if (homePath == NULL) {
		printf("no home (getenv('HOME')) failed");
		exit(1);
	}
	char *globalConfigRelativePath = "/.Parallel_Lang/config.json";
	char *globalConfigPath = malloc(strlen(homePath) + strlen(globalConfigRelativePath) + 1);
	if (globalConfigPath == NULL) {
		printf("malloc failed\n");
		abort();
	}
	sprintf(globalConfigPath, "%s%s", homePath, globalConfigRelativePath);

	char *globalConfigJSON = readFile(globalConfigPath);
	printf("globalConfigJSON: %s\n", globalConfigJSON);

	jsmn_init(&p);
	int globalConfigJSONcount = jsmn_parse(&p, globalConfigJSON, strlen(globalConfigJSON), t, 128);

	char *LLC_path = getJsmnString(globalConfigJSON, t, globalConfigJSONcount, "LLC_path");
	if (LLC_path == 0 || LLC_path[0] == 0) {
		printf("no LLC_path in file at: %s\n", globalConfigPath);
		exit(1);
	}
	printf("LLC_path: %s\n", LLC_path);

	char *clang_path = getJsmnString(globalConfigJSON, t, globalConfigJSONcount, "clang_path");
	if (clang_path == 0 || clang_path[0] == 0) {
		printf("no clang_path in file at: %s\n", globalConfigPath);
		exit(1);
	}
	printf("clang_path: %s\n", clang_path);
	
	char *configJSON = readFile("config.json");
	printf("configJSON: %s\n", configJSON);
	
	jsmn_init(&p);
	int configJSONcount = jsmn_parse(&p, configJSON, strlen(configJSON), t, 128);
	
	char *name = getJsmnString(configJSON, t, configJSONcount, "name");
	if (name == 0 || name[0] == 0) {
		printf("no name in file at: ./config.json\n");
		exit(1);
	}
	printf("name: %s\n", name);
	
	char *entry_path = getJsmnString(configJSON, t, configJSONcount, "entry_path");
	if (entry_path == 0 || entry_path[0] == 0) {
		printf("no entry_path in file at: ./config.json\n");
		exit(1);
	}
	printf("entry_path: %s\n", entry_path);
	
	char *build_directory = getJsmnString(configJSON, t, configJSONcount, "build_directory");
	if (build_directory == 0 || build_directory[0] == 0) {
		printf("no build_directory in file at: ./config.json\n");
		exit(1);
	}
	printf("build_directory: %s\n", build_directory);
	
	source = readFile(entry_path);
	printf("source: %s\n", source);

	linkedList_Node *tokens = lex();
	printTokens(tokens);
	
	linkedList_Node *currentToken = tokens;
	
	linkedList_Node *AST = parse(&currentToken, 0);
	
	linkedList_Node *variables[maxBuilderLevel] = {0};
	addBuilderVariable_type(&variables[0], "Void", "void");
	
	addBuilderVariable_type(&variables[0], "Int8", "i8");
	addBuilderVariable_type(&variables[0], "Int32", "i32");
	addBuilderVariable_type(&variables[0], "Int64", "i64");
	// level is -1 so that it starts at 0 for the first iteration
	char *LLVMsource = buildLLVM((linkedList_Node **)&variables, -1, NULL, NULL, NULL, AST);
	
	printf("LLVMsource: %s\n", LLVMsource);
	
	String LLC_command = {100, 0, 0};
	String_initialize(&LLC_command);
	String_appendChars(&LLC_command, LLC_path);
	String_appendChars(&LLC_command, " -filetype=obj -o ");
	String_appendChars(&LLC_command, build_directory);
	String_appendChars(&LLC_command, "/objectFile.o");
	FILE *fp = popen(LLC_command.data, "w");
	fprintf(fp, "%s", LLVMsource);
	int LLC_status = pclose(fp);
	int LLC_exitCode = WEXITSTATUS(LLC_status);
	String_free(&LLC_command);
	
	if (LLC_exitCode != 0) {
		printf("LLVM error\n");
		exit(1);
	}
	
	char getcwdBuffer[1000];
	
	if (getcwd(getcwdBuffer, sizeof(getcwdBuffer)) == NULL) {
		printf("getcwd error\n");
		exit(1);
	}
	
	// now I understand why swift and JavaScript have built-in string formatting
	String clang_command = {100, 0, 0};
	String_initialize(&clang_command);
	String_appendChars(&clang_command, clang_path);
	String_appendChars(&clang_command, " ");
	String_appendChars(&clang_command, getcwdBuffer);
	String_appendChars(&clang_command, "/");
	String_appendChars(&clang_command, build_directory);
	String_appendChars(&clang_command, "/objectFile.o -o ");
	String_appendChars(&clang_command, getcwdBuffer);
	String_appendChars(&clang_command, "/");
	String_appendChars(&clang_command, build_directory);
	String_appendChars(&clang_command, "/");
	String_appendChars(&clang_command, name);
	
	printf("clang_command: %s\n", clang_command.data);
	
	int clang_status = system(clang_command.data);
	
	int clang_exitCode = WEXITSTATUS(clang_status);
	
	if (clang_exitCode != 0) {
		printf("clang error\n");
		exit(1);
	}
	
	String_free(&clang_command);
	
	if (compilerMode == CompilerMode_run) {
		String run_path = {100, 0, 0};
		String_initialize(&run_path);
		String_appendChars(&run_path, build_directory);
		String_appendChars(&run_path, "/");
		String_appendChars(&run_path, name);
		printf("running program at %s\n", run_path.data);
		system(run_path.data);
		String_free(&run_path);
	}
	
	// clean up
	free(globalConfigPath);
	free(globalConfigJSON);
	free(LLC_path);
	free(clang_path);
	
	free(configJSON);
	free(name);
	free(build_directory);
	free(entry_path);
	
	free(source);
	free(LLVMsource);
	
	linkedList_freeList(&tokens);
//	free_AST(&AST);
	
	return 0;
}
