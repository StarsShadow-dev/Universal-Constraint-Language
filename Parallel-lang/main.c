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

#include <libgen.h>

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

typedef enum {
	CompilerMode_build,
	CompilerMode_run,
	CompilerMode_compilerTesting
} CompilerMode;

int main(int argc, char **argv) {
	CompilerMode compilerMode;
	
	if (argc < 2) {
		printf("no arguments\n");
		exit(1);
	}
	
	if (strcmp(argv[1], "build") == 0) {
		compilerMode = CompilerMode_build;
	}
	
	else if (strcmp(argv[1], "run") == 0) {
		compilerMode = CompilerMode_run;
	}
	
	else if (strcmp(argv[1], "compilerTesting") == 0) {
		compilerMode = CompilerMode_compilerTesting;
	}
	
	else {
		printf("unexpected compiler mode: %s\n", argv[1]);
		exit(1);
	}
	
	if (argc != 3) {
		printf("unexpected amount of arguments, expected: 2, but got: %d\n", argc - 1);
		exit(1);
	}
	
	char *projectDirectoryPath = dirname(argv[2]);
	
	jsmn_parser p;
	jsmntok_t t[128] = {}; // expect no more than 128 JSON tokens
	
	char *homePath = getenv("HOME");
	if (homePath == NULL) {
		printf("no home (getenv(\"HOME\")) failed");
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

	jsmn_init(&p);
	int globalConfigJSONcount = jsmn_parse(&p, globalConfigJSON, strlen(globalConfigJSON), t, 128);

	char *LLC_path = getJsmnString(globalConfigJSON, t, globalConfigJSONcount, "LLC_path");
	if (LLC_path == 0 || LLC_path[0] == 0) {
		printf("no LLC_path in file at: %s\n", globalConfigPath);
		exit(1);
	}

	char *clang_path = getJsmnString(globalConfigJSON, t, globalConfigJSONcount, "clang_path");
	if (clang_path == 0 || clang_path[0] == 0) {
		printf("no clang_path in file at: %s\n", globalConfigPath);
		exit(1);
	}
	
	char *name = NULL;
	char *entry_path = NULL;
	
	CharAccumulator actual_build_directory = {100, 0, 0};
	
	if (compilerMode == CompilerMode_compilerTesting) {
		source = readFile(argv[2]);
	} else {
		char *configJSON = readFile(argv[2]);
		
		jsmn_init(&p);
		int configJSONcount = jsmn_parse(&p, configJSON, strlen(configJSON), t, 128);
		
		name = getJsmnString(configJSON, t, configJSONcount, "name");
		if (name == 0 || name[0] == 0) {
			printf("no name in file at: ./config.json\n");
			exit(1);
		}
		
		entry_path = getJsmnString(configJSON, t, configJSONcount, "entry_path");
		if (entry_path == 0 || entry_path[0] == 0) {
			printf("no entry_path in file at: ./config.json\n");
			exit(1);
		}
		
		char *build_directory = getJsmnString(configJSON, t, configJSONcount, "build_directory");
		if (build_directory == 0 || build_directory[0] == 0) {
			printf("no build_directory in file at: ./config.json\n");
			exit(1);
		}
		
		printf("compiler version: %s\n", CURRENT_VERSION);
		
		CharAccumulator actual_entry_path = {100, 0, 0};
		CharAccumulator_initialize(&actual_entry_path);
		CharAccumulator_appendChars(&actual_entry_path, projectDirectoryPath);
		CharAccumulator_appendChars(&actual_entry_path, "/");
		CharAccumulator_appendChars(&actual_entry_path, entry_path);
		
		CharAccumulator_initialize(&actual_build_directory);
		CharAccumulator_appendChars(&actual_build_directory, projectDirectoryPath);
		CharAccumulator_appendChars(&actual_build_directory, "/");
		CharAccumulator_appendChars(&actual_build_directory, build_directory);
		
		source = readFile(actual_entry_path.data);
//#ifdef COMPILER_DEBUG_MODE
		printf("source: %s\n", source);
//#endif
		
		free(configJSON);
		free(build_directory);
		
		CharAccumulator_free(&actual_entry_path);
	}

	linkedList_Node *tokens = lex();
#ifdef COMPILER_DEBUG_MODE
	if (compilerMode != CompilerMode_compilerTesting) {
		printTokens(tokens);
	}
#endif
	
	linkedList_Node *currentToken = tokens;
	
	linkedList_Node *AST = parse(&currentToken, ParserMode_normal);
	
	CharAccumulator LLVMsource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMsource);
	
	GlobalBuilderInformation globalBuilderInformation = {
		&LLVMsource,
		0
	};
	
	linkedList_Node *variables[maxBuilderLevel] = {0};
	addBuilderType(&variables[0], &(SubString){"Void", (int)strlen("Void")}, "void");
	addBuilderType(&variables[0], &(SubString){"Int8", (int)strlen("Int8")}, "i8");
	addBuilderType(&variables[0], &(SubString){"Int32", (int)strlen("Int32")}, "i32");
	addBuilderType(&variables[0], &(SubString){"Int64", (int)strlen("Int64")}, "i64");
	addBuilderType(&variables[0], &(SubString){"Bool", (int)strlen("Bool")}, "i1");
	addBuilderType(&variables[0], &(SubString){"Pointer", (int)strlen("Pointer")}, "ptr");
	// level is -1 so that it starts at 0 for the first iteration
	CharAccumulator_appendChars(&LLVMsource, buildLLVM(&globalBuilderInformation, (linkedList_Node **)&variables, -1, NULL, NULL, NULL, AST, 0));
	
	if (compilerMode != CompilerMode_compilerTesting) {
		
#ifdef COMPILER_DEBUG_MODE
		printf("LLVMsource: %s\n", LLVMsource.data);
#endif
		
		CharAccumulator LLC_command = {100, 0, 0};
		CharAccumulator_initialize(&LLC_command);
		CharAccumulator_appendChars(&LLC_command, LLC_path);
		CharAccumulator_appendChars(&LLC_command, " -filetype=obj -o ");
		CharAccumulator_appendChars(&LLC_command, actual_build_directory.data);
		CharAccumulator_appendChars(&LLC_command, "/objectFile.o");
		FILE *fp = popen(LLC_command.data, "w");
		fprintf(fp, "%s", LLVMsource.data);
		int LLC_status = pclose(fp);
		int LLC_exitCode = WEXITSTATUS(LLC_status);
		CharAccumulator_free(&LLC_command);
		
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
		CharAccumulator clang_command = {100, 0, 0};
		CharAccumulator_initialize(&clang_command);
		CharAccumulator_appendChars(&clang_command, clang_path);
		CharAccumulator_appendChars(&clang_command, " ");
		CharAccumulator_appendChars(&clang_command, getcwdBuffer);
		CharAccumulator_appendChars(&clang_command, "/");
		CharAccumulator_appendChars(&clang_command, actual_build_directory.data);
		CharAccumulator_appendChars(&clang_command, "/objectFile.o -o ");
		CharAccumulator_appendChars(&clang_command, getcwdBuffer);
		CharAccumulator_appendChars(&clang_command, "/");
		CharAccumulator_appendChars(&clang_command, actual_build_directory.data);
		CharAccumulator_appendChars(&clang_command, "/");
		CharAccumulator_appendChars(&clang_command, name);
		
		int clang_status = system(clang_command.data);
		
		int clang_exitCode = WEXITSTATUS(clang_status);
		
		if (clang_exitCode != 0) {
			printf("clang error\n");
			exit(1);
		}
		
		CharAccumulator_free(&clang_command);
		
		CharAccumulator run_path = {100, 0, 0};
		CharAccumulator_initialize(&run_path);
		CharAccumulator_appendChars(&run_path, actual_build_directory.data);
		CharAccumulator_appendChars(&run_path, "/");
		CharAccumulator_appendChars(&run_path, name);
		
		printf("program saved to %s\n", run_path.data);
		
		if (compilerMode == CompilerMode_run) {
			printf("running program at %s\n", run_path.data);
			int program_status = system(run_path.data);
			
			int program_exitCode = WEXITSTATUS(program_status);
			
			printf("program ended with exit code: %d\n", program_exitCode);
		}
		
		CharAccumulator_free(&run_path);
		
		CharAccumulator_free(&actual_build_directory);
		
		free(name);
		free(entry_path);
	}
	
	// clean up
	free(globalConfigPath);
	free(globalConfigJSON);
	free(LLC_path);
	free(clang_path);
	
	free(source);
	
	CharAccumulator_free(&LLVMsource);
	
	linkedList_freeList(&tokens);
//	free_AST(&AST);
	
	if (compilerMode == CompilerMode_compilerTesting) {
		printf("compiled without any errors\n");
	}
	
	return 0;
}
