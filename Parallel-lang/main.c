/*
 This file has the main function.
 
 The main function has a simple command line argument parser and then calls compileModule from "compiler.c".
 
 The printHelp function comes from "help.parallel" (#include "fromParallel.h");
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fromParallel.h"

#include "jsmn.h"
#include "error.h"
#include "main.h"
#include "types.h"
#include "globals.h"
#include "compiler.h"
#include "utilities.h"

int main(int argc, char **argv) {
	CompilerMode compilerMode;
	
	if (argc == 1) {
		printf("Compiler version: %s\n", CURRENT_VERSION);
		printf("\n");
		printHelp();
		exit(1);
	}
	
	if (strcmp(argv[1], "build_objectFile") == 0) {
		compilerMode = CompilerMode_build_objectFile;
	}
	
	else if (strcmp(argv[1], "build_binary") == 0) {
		compilerMode = CompilerMode_build_binary;
	}
	
	else if (strcmp(argv[1], "run") == 0) {
		compilerMode = CompilerMode_run;
	}
	
	else if (strcmp(argv[1], "compilerTesting") == 0) {
		compilerMode = CompilerMode_compilerTesting;
	}
	
	else {
		printf("Unexpected compiler mode: %s\n", argv[1]);
		exit(1);
	}
	
	char *path = argv[2];
	
	for (int i = 3; i < argc; i++) {
		char *arg = argv[i];
//		printf("arg %d: %s\n", i, arg);
		if (strcmp(arg, "-d") == 0) {
			compilerOptions.includeDebugInformation = 1;
		}
		
		else if (strcmp(arg, "-v") == 0) {
			compilerOptions.verbose = 1;
		}
		
		else {
			printf("'%s' is not a valid argument\n", arg);
			exit(1);
		}
	}
	
	if (compilerMode != CompilerMode_compilerTesting) {
		printf("Compiler version: %s\n", CURRENT_VERSION);
	}
	
	char *homePath = getenv("HOME");
	if (homePath == NULL) {
		printf("No home (getenv(\"HOME\")) failed");
		exit(1);
	}
	
	char *globalConfigRelativePath = "/.Parallel_Lang/config.json";
	char *globalConfigPath = safeMalloc(strlen(homePath) + strlen(globalConfigRelativePath) + 1);
	sprintf(globalConfigPath, "%s%s", homePath, globalConfigRelativePath);

	char *globalConfigJSON = readFile(globalConfigPath);

	jsmn_parser p;
	jsmntok_t t[128] = {}; // expect no more than 128 JSON tokens
	jsmn_init(&p);
	int globalConfigJSONcount = jsmn_parse(&p, globalConfigJSON, strlen(globalConfigJSON), t, 128);
	if (t[0].type != JSMN_OBJECT) {
		printf("Global config JSON parse error");
		exit(1);
	}
	
	LLC_path = getJsmnString(globalConfigJSON, t, 1, globalConfigJSONcount, "LLC_path");
	if (LLC_path == 0 || LLC_path[0] == 0) {
		printf("No LLC_path in file at: %s\n", globalConfigPath);
		exit(1);
	}

	clang_path = getJsmnString(globalConfigJSON, t, 1, globalConfigJSONcount, "clang_path");
	if (clang_path == 0 || clang_path[0] == 0) {
		printf("No clang_path in file at: %s\n", globalConfigPath);
		exit(1);
	}
	
	CharAccumulator_initialize(&errorMsg);
	CharAccumulator_initialize(&errorIndicator);
	
	CharAccumulator_initialize(&objectFiles);
	
	CharAccumulator full_build_directoryCA = {100, 0, 0};
	CharAccumulator_initialize(&full_build_directoryCA);
	CharAccumulator_appendChars(&full_build_directoryCA, path);
	CharAccumulator_appendChars(&full_build_directoryCA, "/build");
	
	if (compilerMode != CompilerMode_compilerTesting) {
		struct stat stat_buffer = {0};
		if (stat(full_build_directoryCA.data, &stat_buffer) == -1) {
			printf("The \"build_directory\" (%s) does not exist.\n", full_build_directoryCA.data);
			exit(1);
		} else {
			if (!S_ISDIR(stat_buffer.st_mode)) {
				printf("The \"build_directory\" (%s) exists but is not a directory.\n", full_build_directoryCA.data);
				exit(1);
			}
		}
		
		full_build_directory = full_build_directoryCA.data;
	}
	
	ModuleInformation coreModule = {
		.name = "__core__",
		.path = NULL,
		.currentSource = NULL,
		.currentFilePath = NULL,
		.topLevelConstantSource = NULL,
		.LLVMmetadataSource = NULL,
		
		.stringCount = 0,
		.metadataCount = 0,
		
		.level = 0,
		
		.context = {0},
		.importedModules = NULL,
		
		.debugInformationCompileUnitID = 0,
		.debugInformationFileScopeID = 0
	};
	coreModulePointer = &coreModule;
	addContextBinding_simpleType(&coreModule.context[0], "Void", "void", 0, 4);
	addContextBinding_simpleType(&coreModule.context[0], "Int8", "i8", 1, 4);
	addContextBinding_simpleType(&coreModule.context[0], "Int16", "i16", 2, 4);
	addContextBinding_simpleType(&coreModule.context[0], "Int32", "i32", 4, 4);
	addContextBinding_simpleType(&coreModule.context[0], "Int64", "i64", 8, 4);
	// how much space should be made for an i1?
	// I will do one byte for now
	addContextBinding_simpleType(&coreModule.context[0], "Bool", "i1", 1, 4);
	addContextBinding_simpleType(&coreModule.context[0], "Pointer", "ptr", pointer_byteSize, pointer_byteSize);
	
	CharAccumulator *topLevelConstantSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelConstantSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelConstantSource);

	CharAccumulator *LLVMmetadataSource = safeMalloc(sizeof(CharAccumulator));
	(*LLVMmetadataSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(LLVMmetadataSource);
	
	ModuleInformation *MI = ModuleInformation_new(path, topLevelConstantSource, LLVMmetadataSource);
	compileModule(MI, compilerMode, path);
	linkedList_freeList(&MI->context[0]);
	
	if (compilerMode != CompilerMode_compilerTesting) {
		CharAccumulator clang_command = {100, 0, 0};
		CharAccumulator_initialize(&clang_command);
		CharAccumulator_appendChars(&clang_command, clang_path);
		CharAccumulator_appendChars(&clang_command, " ");
	//	CharAccumulator_appendChars(&clang_command, getcwdBuffer);
	//	CharAccumulator_appendChars(&clang_command, "/");
		CharAccumulator_appendChars(&clang_command, objectFiles.data);
		CharAccumulator_appendChars(&clang_command, "-o ");
		CharAccumulator_appendChars(&clang_command, full_build_directory);
		CharAccumulator_appendChars(&clang_command, "/binary");
		
		if (compilerOptions.verbose) {
			printf("clang_command: %s\n", clang_command.data);
		}
		
		printf("Binary saved to %s/binary\n", full_build_directory);
		
		int clang_status = system(clang_command.data);

		int clang_exitCode = WEXITSTATUS(clang_status);

		if (clang_exitCode != 0) {
			printf("clang error\n");
			exit(1);
		}
		
		CharAccumulator_free(&clang_command);
	}
	
	if (compilerMode == CompilerMode_run) {
		abort();
//		printf("Running program at: %s\n", outputFilePath.data);
//		int program_status = system(outputFilePath.data);
//
//		int program_exitCode = WEXITSTATUS(program_status);
//
//		printf("Program ended with exit code: %d\n", program_exitCode);
	}
	
	free(globalConfigPath);
	free(globalConfigJSON);
	
	free(LLC_path);
	free(clang_path);
	
	CharAccumulator_free(&full_build_directoryCA);
	
	return 0;
}
