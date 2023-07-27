#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#define JSMN_HEADER
#include "jsmn.h"
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

char *readFile(const char *path) {
	FILE* file = fopen(path, "r");
	
	if (file == 0) {
		printf("Could not find file at: %s\n", path);
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

// -1 if it does not exist
int getJsmnTokenIndex(char *buffer, jsmntok_t *t, int start, int count, char *key, jsmntype_t type) {
	int i = start;
	
	while (i - start < count) {
		int correctKey = 1;
		
		if (key != NULL) {
			// now look for key/value pairs.
			
			jsmntok_t keyToken = t[i];
			if (keyToken.type != JSMN_STRING) {
				// the "key" is not a string.
				abort();
			}
			if (keyToken.size != 1) {
				// keys should have a single value.
				abort();
			}
			
			i++;
			
			if (strncmp(buffer + keyToken.start, key, strlen(key)) != 0) {
				correctKey = 0;
			}
		}
		
		jsmntok_t valueToken = t[i];
		switch (valueToken.type) {
			case JSMN_UNDEFINED:
				printf("error JSMN_UNDEFINED");
				abort();
			case JSMN_OBJECT:
				printf("error JSMN_OBJECT");
				abort();
			case JSMN_ARRAY:
				if (!correctKey || valueToken.type != type) {
					i += valueToken.size + 1;
					continue;
				}
				return i;
			case JSMN_STRING:
				if (!correctKey || valueToken.type != type) {
					i++;
					continue;
				}
				return i;
			case JSMN_PRIMITIVE:
				printf("error JSMN_PRIMITIVE");
				abort();
		}
		
		i++;
	}
	
	return -1;
}

char *getJsmnString(char *buffer, jsmntok_t *t, int start, int count, char *key) {
	int index = getJsmnTokenIndex(buffer, t, start, count, key, JSMN_STRING);
	if (index == -1) return NULL;
	jsmntok_t token = t[index];
	
	unsigned long stringLen = token.end - token.start;
	char *stringValue = buffer + token.start;
	char *result = malloc(stringLen + 1);
	if (result == NULL) {
		printf("malloc failed\n");
		abort();
	}
	
	memcpy(result, stringValue, stringLen);
	result[stringLen] = 0;
	return result;
}

linkedList_Node *getJsmnStringArray(char *buffer, jsmntok_t *t, int start, int count, char *key) {
	linkedList_Node *strings = NULL;
	
	int arrayIndex = getJsmnTokenIndex(buffer, t, start, count, key, JSMN_ARRAY);
	if (arrayIndex == -1) return NULL;
	jsmntok_t arrayToken = t[arrayIndex];
	
	
	int loopCount = 0;
	
	while (loopCount < arrayToken.size) {
		int index = getJsmnTokenIndex(buffer, t, arrayIndex + 1 + loopCount, arrayToken.size, NULL, JSMN_STRING);
		if (index == -1) return NULL;
		jsmntok_t token = t[index];
		
		int size = token.end - token.start;
		
		char *data = linkedList_addNode(&strings, size);
		
		memcpy(data, buffer + token.start, size);
		data[size] = 0;
		
		loopCount++;
	}
	
	return strings;
}

void compileFile(char *path, GlobalBuilderInformation *GBI, CharAccumulator *LLVMsource) {
	source = readFile(path);
	
//#ifdef COMPILER_DEBUG_MODE
//	printf("Source (%s): %s\n", path, source);
//#endif
	
	linkedList_Node *tokens = lex();
//#ifdef COMPILER_DEBUG_MODE
//	printTokens(tokens);
//#endif
	
	linkedList_Node *currentToken = tokens;
	
	linkedList_Node *AST = parse(&currentToken, ParserMode_normal);
	
	buildLLVM(GBI, NULL, LLVMsource, NULL, NULL, NULL, AST, 0, 0, 0);
}

void compileModule(char *path, CompilerMode compilerMode, CharAccumulator *LLVMsource, char *LLC_path, char *clang_path) {
	char *projectDirectoryPath = dirname(path);
	
	char *name = NULL;
	linkedList_Node *file_paths = NULL;
	CharAccumulator full_build_directory = {100, 0, 0};
	
	if (compilerMode != CompilerMode_compilerTesting) {
		char *configJSON = readFile(path);
		
		jsmn_parser p;
		jsmntok_t t[128] = {}; // expect no more than 128 JSON tokens
		jsmn_init(&p);
		int configJSONcount = jsmn_parse(&p, configJSON, strlen(configJSON), t, 128);
		
		name = getJsmnString(configJSON, t, 1, configJSONcount, "name");
		if (name == 0 || name[0] == 0) {
			printf("No name in file at: ./config.json\n");
			exit(1);
		}
		
		file_paths = getJsmnStringArray(configJSON, t, 1, configJSONcount, "file_paths");
		if (file_paths == NULL) {
			printf("No file_paths in file at: ./config.json\n");
			exit(1);
		}
		
		char *build_directory = getJsmnString(configJSON, t, 1, configJSONcount, "build_directory");
		if (build_directory == 0 || build_directory[0] == 0) {
			printf("No build_directory in file at: ./config.json\n");
			exit(1);
		}
		
		CharAccumulator_initialize(&full_build_directory);
		CharAccumulator_appendChars(&full_build_directory, projectDirectoryPath);
		CharAccumulator_appendChars(&full_build_directory, "/");
		CharAccumulator_appendChars(&full_build_directory, build_directory);
		
		struct stat stat_buffer = {0};
		if (stat(full_build_directory.data, &stat_buffer) == -1) {
			printf("The \"build_directory\" specified in the config file does not exist.\n");
			exit(1);
		} else {
			if (!S_ISDIR(stat_buffer.st_mode)) {
				printf("The \"build_directory\" specified in the config file exists but is not a directory.\n");
				exit(1);
			}
		}
		
		free(configJSON);
		
		free(build_directory);
	}
	
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
	
	addBuilderType(&GBI.variables[0], "Void", "void", 0, 4);
	addBuilderType(&GBI.variables[0], "Int8", "i8", 1, 4);
	addBuilderType(&GBI.variables[0], "Int32", "i32", 4, 4);
	addBuilderType(&GBI.variables[0], "Int64", "i64", 8, 4);
	// how much space should be made for an i1?
	// I will do one byte for now
	addBuilderType(&GBI.variables[0], "Bool", "i1", 1, 4);
	addBuilderType(&GBI.variables[0], "Pointer", "ptr", pointer_byteSize, pointer_byteSize);
	
	if (compilerMode != CompilerMode_compilerTesting) {
		linkedList_Node *currentFilePath = file_paths;
		while (currentFilePath != NULL) {
			CharAccumulator full_entry_path = {100, 0, 0};
			CharAccumulator_initialize(&full_entry_path);
			CharAccumulator_appendChars(&full_entry_path, projectDirectoryPath);
			CharAccumulator_appendChars(&full_entry_path, "/");
			CharAccumulator_appendChars(&full_entry_path, (char *)currentFilePath->data);
			compileFile(full_entry_path.data, &GBI, LLVMsource);
			CharAccumulator_free(&full_entry_path);
			
			currentFilePath = currentFilePath->next;
		}
	} else {
		compileFile(path, &GBI, LLVMsource);
	}
	
	CharAccumulator_appendChars(LLVMsource, LLVMconstantSource.data);
	CharAccumulator_free(&LLVMconstantSource);
	
	linkedList_freeList(&GBI.variables[0]);
	
	if (compilerMode != CompilerMode_compilerTesting) {
#ifdef COMPILER_DEBUG_MODE
		printf("LLVMsource: %s\n", LLVMsource->data);
#endif
		
		CharAccumulator outputFilePath = {100, 0, 0};
		CharAccumulator_initialize(&outputFilePath);
		CharAccumulator_appendChars(&outputFilePath, full_build_directory.data);
		CharAccumulator_appendChars(&outputFilePath, "/");
		CharAccumulator_appendChars(&outputFilePath, name);
		
		CharAccumulator LLC_command = {100, 0, 0};
		CharAccumulator_initialize(&LLC_command);
		CharAccumulator_appendChars(&LLC_command, LLC_path);
		CharAccumulator_appendChars(&LLC_command, " -filetype=obj -o ");
		CharAccumulator_appendChars(&LLC_command, outputFilePath.data);
		CharAccumulator_appendChars(&LLC_command, ".o");
		FILE *fp = popen(LLC_command.data, "w");
		fprintf(fp, "%s", LLVMsource->data);
		int LLC_status = pclose(fp);
		int LLC_exitCode = WEXITSTATUS(LLC_status);
		CharAccumulator_free(&LLC_command);
		
		if (LLC_exitCode != 0) {
			printf("llc error\n");
			exit(1);
		}
		
		if (compilerMode == CompilerMode_build_binary || compilerMode == CompilerMode_run) {
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
			CharAccumulator_appendChars(&clang_command, outputFilePath.data);
			CharAccumulator_appendChars(&clang_command, ".o -o ");
			CharAccumulator_appendChars(&clang_command, getcwdBuffer);
			CharAccumulator_appendChars(&clang_command, "/");
			CharAccumulator_appendChars(&clang_command, full_build_directory.data);
			CharAccumulator_appendChars(&clang_command, "/");
			CharAccumulator_appendChars(&clang_command, name);
			
			int clang_status = system(clang_command.data);
			
			int clang_exitCode = WEXITSTATUS(clang_status);
			
			if (clang_exitCode != 0) {
				printf("clang error\n");
				exit(1);
			}
			
			CharAccumulator_free(&clang_command);
			
			printf("Program saved to: %s\n", outputFilePath.data);
			
			if (compilerMode == CompilerMode_run) {
				printf("Running program at: %s\n", outputFilePath.data);
				int program_status = system(outputFilePath.data);
				
				int program_exitCode = WEXITSTATUS(program_status);
				
				printf("Program ended with exit code: %d\n", program_exitCode);
			}
		} else {
			printf("Object file saved to saved to %s.o\n", outputFilePath.data);
		}
		
		CharAccumulator_free(&outputFilePath);
		
		CharAccumulator_free(&full_build_directory);
		
		linkedList_freeList(&file_paths);
		
		free(name);
	} else {
		CharAccumulator LLC_command = {100, 0, 0};
		CharAccumulator_initialize(&LLC_command);
		CharAccumulator_appendChars(&LLC_command, LLC_path);
		CharAccumulator_appendChars(&LLC_command, " > /dev/null");
		FILE *fp = popen(LLC_command.data, "w");
		fprintf(fp, "%s", LLVMsource->data);
		int LLC_status = pclose(fp);
		int LLC_exitCode = WEXITSTATUS(LLC_status);
		CharAccumulator_free(&LLC_command);
		
		if (LLC_exitCode != 0) {
			abort();
		}
	}
	
	// clean up
	free(LLC_path);
	free(clang_path);
	
	free(source);
}