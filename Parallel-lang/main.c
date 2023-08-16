/*
 This file has the main function.
 
 The main function has a simple command line argument parser and then calls compileModule from "compiler.c".
 
 The printHelp function comes from "help.parallel" (#include "fromParallel.h");
 */

#include <stdio.h>
#include <string.h>

#include "fromParallel.h"

#include "error.h"
#include "jsmn.h"
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
	
	char *target_triple = argv[2];
	char *path = argv[3];
	
	for (int i = 4; i < argc; i++) {
		char *arg = argv[i];
//		printf("arg %d: %s\n", i, arg);
		if (strcmp(arg, "-d") == 0) {
			printf("'-d' (includeDebugInformation) is not currently supported\n");
			exit(1);
//			compilerOptions.includeDebugInformation = 1;
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
	
	char *LLC_path = getJsmnString(globalConfigJSON, t, 1, globalConfigJSONcount, "LLC_path");
	if (LLC_path == 0 || LLC_path[0] == 0) {
		printf("No LLC_path in file at: %s\n", globalConfigPath);
		exit(1);
	}

	char *clang_path = getJsmnString(globalConfigJSON, t, 1, globalConfigJSONcount, "clang_path");
	if (clang_path == 0 || clang_path[0] == 0) {
		printf("No clang_path in file at: %s\n", globalConfigPath);
		exit(1);
	}
	
	CharAccumulator LLVMsource = {100, 0, 0};
	CharAccumulator_initialize(&LLVMsource);
	
	CharAccumulator_initialize(&errorMsg);
	CharAccumulator_initialize(&errorIndicator);
	
	compileModule(compilerMode, target_triple,  path, &LLVMsource, LLC_path, clang_path);
	
	free(globalConfigPath);
	free(globalConfigJSON);
	CharAccumulator_free(&LLVMsource);
	
	return 0;
}
