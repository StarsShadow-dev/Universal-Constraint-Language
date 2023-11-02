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

void compileFile(char *path, ModuleInformation *MI, CharAccumulator *LLVMsource) {
	MI->context.currentSource = readFile(path);
//	printf("Source (%s): %s\n", path, MI->currentSource);
	
	linkedList_Node *tokens = lex(MI);
//	printTokens(tokens);
	
	linkedList_Node *currentToken = tokens;
	
	linkedList_Node *AST = parse(MI, &currentToken, ParserMode_codeBlock, 0, 0);
	
	buildLLVM(MI, NULL, LLVMsource, NULL, NULL, NULL, AST, 0, 0, 0);
}

void compileModule(ModuleInformation *MI, CompilerMode compilerMode, char *path) {
	char *name = NULL;
	linkedList_Node *file_paths = NULL;
	
	// add the coreModule to this module's importedModules list
	ModuleInformation **coreModulePointerData = linkedList_addNode(&MI->context.importedModules, sizeof(void *));
	*coreModulePointerData = coreModulePointer;
	
	if (compilerMode != CompilerMode_compilerTesting) {
		CharAccumulator configJSONPath = {100, 0, 0};
		CharAccumulator_initialize(&configJSONPath);
		CharAccumulator_appendChars(&configJSONPath, path);
		CharAccumulator_appendChars(&configJSONPath, "/config.json");
		
		char *configJSON = readFile(configJSONPath.data);
		
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
		
		free(configJSON);
		CharAccumulator_free(&configJSONPath);
		
		linkedList_Node *currentModule = alreadyCompiledModules;
		while (currentModule != NULL) {
			ModuleInformation *moduleInformation = *(ModuleInformation **)currentModule->data;
			
			if (strcmp(moduleInformation->name, name) == 0) {
				if (compilerOptions.verbose) printf("Module already compiled %s\n", name);
				*MI = *moduleInformation;
				return;
			}
			
			currentModule = currentModule->next;
		}
		
		MI->name = name;
		
		printf("Compiling module %s\n", name);
	} else {
		MI->name = "compilerTest";
	}
	
	CharAccumulator LLVMsource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMsource);
	
	if (compilerMode != CompilerMode_compilerTesting) {
		linkedList_Node *currentFilePath = file_paths;
		while (currentFilePath != NULL) {
			MI->context.currentFilePath = (char *)currentFilePath->data;
			
			CharAccumulator fullFilePath = {100, 0, 0};
			CharAccumulator_initialize(&fullFilePath);
			CharAccumulator_appendChars(&fullFilePath, path);
			CharAccumulator_appendChars(&fullFilePath, "/");
			CharAccumulator_appendChars(&fullFilePath, MI->context.currentFilePath);
			
			if (compilerOptions.includeDebugInformation) {
				CharAccumulator_appendChars(MI->LLVMmetadataSource, "\n!");
				CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->metadataCount);
				CharAccumulator_appendChars(MI->LLVMmetadataSource, " = !DIFile(filename: \"");
				CharAccumulator_appendChars(MI->LLVMmetadataSource, MI->context.currentFilePath);
				CharAccumulator_appendChars(MI->LLVMmetadataSource, "\", directory: \"");
				CharAccumulator_appendChars(MI->LLVMmetadataSource, path);
				CharAccumulator_appendChars(MI->LLVMmetadataSource, "\")");
				MI->debugInformationFileScopeID = MI->metadataCount;
				MI->metadataCount++;
				
				CharAccumulator_appendChars(MI->LLVMmetadataSource, "\n!");
				CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->metadataCount);
				// the language field is required
				CharAccumulator_appendChars(MI->LLVMmetadataSource, " = distinct !DICompileUnit(language: DW_LANG_C, file: !");
				CharAccumulator_appendInt(MI->LLVMmetadataSource, MI->debugInformationFileScopeID);
				CharAccumulator_appendChars(MI->LLVMmetadataSource, ", runtimeVersion: 0, emissionKind: FullDebug)");
				MI->debugInformationCompileUnitID = MI->metadataCount;
				MI->metadataCount++;
			}
			
			compileFile(fullFilePath.data, MI, &LLVMsource);
			
			CharAccumulator_free(&fullFilePath);
			
			currentFilePath = currentFilePath->next;
		}
	} else {
		compileFile(path, MI, &LLVMsource);
	}
	
	CharAccumulator_appendChars(&LLVMsource, MI->topLevelConstantSource->data);
	CharAccumulator_appendChars(&LLVMsource, MI->topLevelFunctionSource->data);
	CharAccumulator_appendChars(&LLVMsource, MI->LLVMmetadataSource->data);
	
	CharAccumulator_free(MI->topLevelConstantSource);
	CharAccumulator_free(MI->topLevelFunctionSource);
	CharAccumulator_free(MI->LLVMmetadataSource);
	
	if (compilerOptions.includeDebugInformation) {
		CharAccumulator_appendChars(&LLVMsource, "\n!llvm.module.flags = !{!");
		CharAccumulator_appendInt(&LLVMsource, MI->metadataCount);
		CharAccumulator_appendChars(&LLVMsource, ", !");
		CharAccumulator_appendInt(&LLVMsource, MI->metadataCount + 1);
		CharAccumulator_appendChars(&LLVMsource, "}\n!");
		CharAccumulator_appendInt(&LLVMsource, MI->metadataCount);
		CharAccumulator_appendChars(&LLVMsource, " = !{i32 7, !\"Dwarf Version\", i32 4}\n!");
		CharAccumulator_appendInt(&LLVMsource, MI->metadataCount + 1);
		CharAccumulator_appendChars(&LLVMsource, " = !{i32 2, !\"Debug Info Version\", i32 3}");
		MI->metadataCount += 2;
		
		CharAccumulator_appendChars(&LLVMsource, "\n!llvm.dbg.cu = !{!");
		CharAccumulator_appendInt(&LLVMsource, MI->debugInformationCompileUnitID);
		CharAccumulator_appendChars(&LLVMsource, "}");
		
		MI->metadataCount++;
	}
	
	if (compilerMode == CompilerMode_check) {
		printf("Finished checking module %s\n", name);
	} else {
		if (compilerMode != CompilerMode_compilerTesting) {
			if (compilerOptions.verbose) {
				printf("LLVMsource: %s\n", LLVMsource.data);
			}
			
			CharAccumulator outputFilePath = {100, 0, 0};
			CharAccumulator_initialize(&outputFilePath);
			CharAccumulator_appendChars(&outputFilePath, full_build_directory);
			CharAccumulator_appendChars(&outputFilePath, "/");
			CharAccumulator_appendChars(&outputFilePath, name);
			
			CharAccumulator LLC_command = {100, 0, 0};
			CharAccumulator_initialize(&LLC_command);
			CharAccumulator_appendChars(&LLC_command, LLC_path);
			CharAccumulator_appendChars(&LLC_command, " -filetype=obj -o ");
			CharAccumulator_appendChars(&LLC_command, outputFilePath.data);
			CharAccumulator_appendChars(&LLC_command, ".o");
			FILE *fp = popen(LLC_command.data, "w");
			fprintf(fp, "%s", LLVMsource.data);
			int LLC_status = pclose(fp);
			int LLC_exitCode = WEXITSTATUS(LLC_status);
			CharAccumulator_free(&LLC_command);
			
			if (LLC_exitCode != 0) {
				printf("llc error\n");
				exit(1);
			}
			
			CharAccumulator_appendChars(&objectFiles, outputFilePath.data);
			CharAccumulator_appendChars(&objectFiles, ".o ");
			
			printf("Object file saved to %s.o\n", outputFilePath.data);
			
			CharAccumulator_free(&outputFilePath);
		} else {
			CharAccumulator LLC_command = {100, 0, 0};
			CharAccumulator_initialize(&LLC_command);
			CharAccumulator_appendChars(&LLC_command, LLC_path);
			CharAccumulator_appendChars(&LLC_command, " > /dev/null");
			FILE *fp = popen(LLC_command.data, "w");
			fprintf(fp, "%s", LLVMsource.data);
			int LLC_status = pclose(fp);
			int LLC_exitCode = WEXITSTATUS(LLC_status);
			CharAccumulator_free(&LLC_command);
			
			if (LLC_exitCode != 0) {
				abort();
			}
		}
	}
	
	CharAccumulator_free(&LLVMsource);
	
	ModuleInformation **modulePointerData = linkedList_addNode(&alreadyCompiledModules, sizeof(void *));
	*modulePointerData = MI;
}
