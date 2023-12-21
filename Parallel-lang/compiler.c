#include <stdio.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>

#include "compiler.h"
#include "globals.h"
#include "lexer.h"
#include "parser.h"
#include "builder.h"
#include "utilities.h"
#include "report.h"

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
	
	char *buffer = safeMalloc(fileSize + 1);
	
	if (fread(buffer, sizeof(char), fileSize, file) != fileSize) {
		printf("fread failed, path = %s\n", path);
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
				return -1;
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
	char *result = safeMalloc(stringLen + 1);
	
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
		
		char *data = linkedList_addNode(&strings, size + 1);
		
		memcpy(data, buffer + token.start, size);
		data[size] = 0;
		
		loopCount++;
	}
	
	return strings;
}

void compileFile(FileInformation *FI) {
//	struct timespec fileStartTime = getTimespec();
	
	// add the coreFile to this file's importedFiles list
	FileInformation **coreFilePointerData = linkedList_addNode(&FI->context.importedFiles, sizeof(void *));
	*coreFilePointerData = coreFilePointer;
	
	CharAccumulator LLVMsource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMsource);
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->metadataCount);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, " = !DIFile(filename: \"");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, basename(FI->context.path));
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\", directory: \"");
		CharAccumulator_appendChars(FI->LLVMmetadataSource, dirname(FI->context.path));
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\")");
		FI->debugInformationFileScopeID = FI->metadataCount;
		FI->metadataCount++;
		
		CharAccumulator_appendChars(FI->LLVMmetadataSource, "\n!");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->metadataCount);
		// the language field is required
		CharAccumulator_appendChars(FI->LLVMmetadataSource, " = distinct !DICompileUnit(language: DW_LANG_C, file: !");
		CharAccumulator_appendInt(FI->LLVMmetadataSource, FI->debugInformationFileScopeID);
		CharAccumulator_appendChars(FI->LLVMmetadataSource, ", runtimeVersion: 0, emissionKind: FullDebug)");
		FI->debugInformationCompileUnitID = FI->metadataCount;
		FI->metadataCount++;
	}
	
	struct timespec readFileStartTime = {0};
	struct timespec readFileEndTime = {0};
	
	if (compilerMode == CompilerMode_query && strcmp(FI->context.path, startFilePath) == 0) {
		FI->context.currentSource = queryText;
	} else {
		readFileStartTime = getTimespec();
		FI->context.currentSource = readFile(FI->context.path);
		readFileEndTime = getTimespec();
	}
	
//	printf("Source (%s): %s\n", path, FI->context.currentSource);
	
	struct timespec lexStartTime = getTimespec();
	linkedList_Node *tokens = lex(FI);
	struct timespec lexEndTime = getTimespec();
//	printTokens(tokens);
	
	linkedList_Node *currentToken = tokens;
	
	struct timespec parseStartTime = getTimespec();
	linkedList_Node *AST = parse(FI, &currentToken, ParserMode_codeBlock, 0, 0);
	struct timespec parseEndTime = getTimespec();
	
//	struct timespec buildStartTime = getTimespec();
	buildLLVM(FI, NULL, &LLVMsource, NULL, NULL, NULL, AST, 0, 0, 0);
//	struct timespec buildEndTime = getTimespec();
	
	CharAccumulator_appendChars(&LLVMsource, FI->topLevelConstantSource->data);
	CharAccumulator_appendChars(&LLVMsource, FI->topLevelFunctionSource->data);
	CharAccumulator_appendChars(&LLVMsource, FI->LLVMmetadataSource->data);
	
	CharAccumulator_free(FI->topLevelConstantSource);
	CharAccumulator_free(FI->topLevelFunctionSource);
	CharAccumulator_free(FI->LLVMmetadataSource);
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(&LLVMsource, "\n!llvm.module.flags = !{!");
		CharAccumulator_appendInt(&LLVMsource, FI->metadataCount);
		CharAccumulator_appendChars(&LLVMsource, ", !");
		CharAccumulator_appendInt(&LLVMsource, FI->metadataCount + 1);
		CharAccumulator_appendChars(&LLVMsource, "}\n!");
		CharAccumulator_appendInt(&LLVMsource, FI->metadataCount);
		CharAccumulator_appendChars(&LLVMsource, " = !{i32 7, !\"Dwarf Version\", i32 4}\n!");
		CharAccumulator_appendInt(&LLVMsource, FI->metadataCount + 1);
		CharAccumulator_appendChars(&LLVMsource, " = !{i32 2, !\"Debug Info Version\", i32 3}");
		FI->metadataCount += 2;
		
		CharAccumulator_appendChars(&LLVMsource, "\n!llvm.dbg.cu = !{!");
		CharAccumulator_appendInt(&LLVMsource, FI->debugInformationCompileUnitID);
		CharAccumulator_appendChars(&LLVMsource, "}");
		
		FI->metadataCount++;
	}
	
	struct timespec llcStartTime = {0};
	struct timespec llcEndTime = {0};
	
	if (compilerMode == CompilerMode_check) {
		printf("Finished checking file at %s\n", FI->context.path);
	} else {
		if (compilerMode != CompilerMode_compilerTesting && compilerMode != CompilerMode_query) {
			if (compilerOptions.printIR) {
				printf("LLVMsource: %s\n", LLVMsource.data);
			}
			
			CharAccumulator outputFilePath = {100, 0, 0};
			CharAccumulator_initialize(&outputFilePath);
			CharAccumulator_appendChars(&outputFilePath, buildDirectory);
			CharAccumulator_appendChars(&outputFilePath, "/");
			CharAccumulator_appendInt(&outputFilePath, FI->ID);
			CharAccumulator_appendChars(&outputFilePath, ".o");
			
			CharAccumulator LLC_command = {100, 0, 0};
			CharAccumulator_initialize(&LLC_command);
			CharAccumulator_appendChars(&LLC_command, LLC_path);
			CharAccumulator_appendChars(&LLC_command, " -filetype=obj -o ");
			CharAccumulator_appendChars(&LLC_command, outputFilePath.data);
			
			llcStartTime = getTimespec();
			FILE *fp = popen(LLC_command.data, "w");
			fprintf(fp, "%s", LLVMsource.data);
			int LLC_status = pclose(fp);
			int LLC_exitCode = WEXITSTATUS(LLC_status);
			CharAccumulator_free(&LLC_command);
			llcEndTime = getTimespec();
			
			if (LLC_exitCode != 0) {
				printf("llc error\n");
				exit(1);
			}
			
			CharAccumulator_appendChars(&objectFiles, outputFilePath.data);
			CharAccumulator_appendChars(&objectFiles, " ");
			
			if (compilerOptions.verbose) printf("Object file saved to %s\n", outputFilePath.data);
			
			CharAccumulator_free(&outputFilePath);
		} else {
			CharAccumulator LLC_command = {100, 0, 0};
			CharAccumulator_initialize(&LLC_command);
			CharAccumulator_appendChars(&LLC_command, LLC_path);
			CharAccumulator_appendChars(&LLC_command, " > /dev/null");
			
			llcStartTime = getTimespec();
			FILE *fp = popen(LLC_command.data, "w");
			fprintf(fp, "%s", LLVMsource.data);
			int LLC_status = pclose(fp);
			int LLC_exitCode = WEXITSTATUS(LLC_status);
			CharAccumulator_free(&LLC_command);
			llcEndTime = getTimespec();
			
			if (LLC_exitCode != 0) {
				abort();
			}
		}
	}
	
	CharAccumulator_free(&LLVMsource);
	
	FileInformation **filePointerData = linkedList_addNode(&alreadyCompiledFiles, sizeof(void *));
	*filePointerData = FI;
	
	if (compilerOptions.timed) {
		printf("compiled file %s\n", FI->context.path);
		if (!(compilerMode == CompilerMode_query && strcmp(FI->context.path, startFilePath) == 0)) {
			printf(" readFile: %llu milliseconds\n", getMilliseconds(readFileStartTime, readFileEndTime));
		}
		printf(" lex: %llu milliseconds\n", getMilliseconds(lexStartTime, lexEndTime));
		printf(" parse: %llu milliseconds\n", getMilliseconds(parseStartTime, parseEndTime));
//		printf(" build: %llu milliseconds\n", getMilliseconds(buildStartTime, buildEndTime));
		if (compilerMode != CompilerMode_check) {
			printf(" llc: %llu milliseconds\n", getMilliseconds(llcStartTime, llcEndTime));
		}
//		printf("total: %llu milliseconds\n\n", getMilliseconds(fileStartTime, getTimespec()));
		printf("\n");
	}
}
