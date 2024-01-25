/*
 This file has the main function.
 
 The main function has a simple command line argument parser and then calls compileFile from "compiler.c".
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#include "fromLLCL.h"

#include "jsmn.h"
#include "report.h"
#include "main.h"
#include "types.h"
#include "globals.h"
#include "compiler.h"
#include "utilities.h"

int main(int argc, char **argv) {
	if (argc == 1) {
		printf("Compiler version: %s\n", CURRENT_VERSION);
		printf("\n");
		printf("no help for you.\n");
		exit(1);
	}
	
	// the current argument
	int currentArg = 1;
	
	if (strcmp(argv[currentArg], "build_objectFile") == 0) {
		currentArg++;
		compilerMode = CompilerMode_build_objectFile;
		
		startFilePath = argv[currentArg];
		currentArg++;
	}
	
	else if (strcmp(argv[currentArg], "build_binary") == 0) {
		currentArg++;
		compilerMode = CompilerMode_build_binary;
		
		startFilePath = argv[currentArg];
		currentArg++;
	}
	
	else if (strcmp(argv[currentArg], "run") == 0) {
		printf("CompilerMode_run is not available right now\n");
		exit(1);
	}
	
	else if (strcmp(argv[currentArg], "check") == 0) {
		currentArg++;
		compilerMode = CompilerMode_check;
		
		startFilePath = argv[currentArg];
		currentArg++;
	}
	
	else if (strcmp(argv[currentArg], "query") == 0) {
		currentArg++;
		compilerMode = CompilerMode_query;
		if (argc < 7) {
			printf("not (argc < 8)");
			exit(1);
		}
		
		if (strcmp(argv[currentArg], "hover") == 0) {
			queryMode = QueryMode_hover;
		} else if (strcmp(argv[currentArg], "suggestions") == 0) {
			queryMode = QueryMode_suggestions;
		} else {
			abort();
		}
		currentArg++;
		
		startFilePath = argv[currentArg];
		currentArg++;
		queryTextLength = atoi(argv[currentArg]);
		currentArg++;
		queryLine = atoi(argv[currentArg]);
		currentArg++;
		queryColumn = atoi(argv[currentArg]);
		currentArg++;
		queryText = safeMalloc(queryTextLength);
		fread(queryText, queryTextLength, 1, stdin);
	}
	
	else {
		printf("Unexpected compiler mode: %s\n", argv[currentArg]);
		exit(1);
	}
	
	for (; currentArg < argc; currentArg++) {
		char *arg = argv[currentArg];
//		printf("arg %d: %s\n", currentArg, arg);
		if (strcmp(arg, "-d") == 0) {
			if (compilerMode == CompilerMode_check) {
				printf("'-d' is not allowed with check mode\n");
				exit(1);
			}
			compilerOptions.includeDebugInformation = 1;
		}
		
		else if (strcmp(arg, "-v") == 0) {
			compilerOptions.verbose = 1;
		}
		
		else if (strcmp(arg, "-ir") == 0) {
			compilerOptions.printIR = 1;
		}
		
		else if (strcmp(arg, "-timed") == 0) {
			compilerOptions.timed = 1;
			startTime = getTimespec();
		}
		
		else if (strcmp(arg, "-compilerTesting") == 0) {
			compilerOptions.compilerTesting = 1;
		}
		
		else {
			printf("'%s' is not a valid argument\n", arg);
			exit(1);
		}
	}
	
	if (compilerOptions.verbose) {
		printf("Compiler version: %s\n", CURRENT_VERSION);
	}
	
	char *homePath = getenv("HOME");
	if (homePath == NULL) {
		printf("No home (getenv(\"HOME\")) failed");
		exit(1);
	}
	
	char *globalConfigRelativePath = "/.LLCL/config.json";
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
	
	CharAccumulator_initialize(&reportMsg);
	CharAccumulator_initialize(&reportIndicator);
	
	CharAccumulator_initialize(&objectFiles);
	
	if (
		!compilerOptions.compilerTesting &&
		compilerMode != CompilerMode_query &&
		compilerMode != CompilerMode_check
	) {
		CharAccumulator buildDirectoryCA = {100, 0, 0};
		CharAccumulator_initialize(&buildDirectoryCA);
		CharAccumulator_appendChars(&buildDirectoryCA, dirname(startFilePath));
		CharAccumulator_appendChars(&buildDirectoryCA, "/build");
		buildDirectory = buildDirectoryCA.data;
		
		struct stat stat_buffer = {0};
		if (stat(buildDirectory, &stat_buffer) == -1) {
			printf("The buildDirectory (%s) does not exist.\n", buildDirectory);
			exit(1);
		} else {
			if (!S_ISDIR(stat_buffer.st_mode)) {
				printf("The buildDirectory (%s) exists but is not a directory.\n", buildDirectory);
				exit(1);
			}
		}
	}
	
	FileInformation coreFile = {
		.ID = 0,
		.topLevelConstantSource = NULL,
		.topLevelFunctionSource = NULL,
		.LLVMmetadataSource = NULL,
		
		.stringCount = 0,
		.metadataCount = 0,
		
		.level = 0,
		.context = {
			.currentSource = NULL,
			.path = NULL,
			.declaredInLLVM = NULL,
			
			.bindings = {0},
			.importedFiles = NULL,
		},
		
		.debugInformationCompileUnitID = 0,
		.debugInformationFileScopeID = 0
	};
	coreFilePointer = &coreFile;
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Void", "void", 0, 4);
	
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Int8", "i8", 1, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Int16", "i16", 2, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Int32", "i32", 4, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Int64", "i64", 8, 4);
	
	addContextBinding_simpleType(&coreFile.context.bindings[0], "UInt8", "i8", 1, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "UInt16", "i16", 2, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "UInt32", "i32", 4, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "UInt64", "i64", 8, 4);
	
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Float16", "half", 2, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Float32", "float", 4, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Float64", "double", 8, 4);
	
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Vector", "", 0, pointer_byteSize);
	
	// how much space should be made for an i1?
	// I will do one byte for now
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Bool", "i1", 1, 4);
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Pointer", "ptr", pointer_byteSize, pointer_byteSize);
	
	addContextBinding_simpleType(&coreFile.context.bindings[0], "Function", "ptr", pointer_byteSize, pointer_byteSize);
	// if "__Macro_and_it_should_not_be_in_IR" ever shows up in the IR then I know that something went horribly wrong...
	addContextBinding_simpleType(&coreFile.context.bindings[0], "__Macro", "__Macro_and_it_should_not_be_in_IR", 0, 0);
	// same for "__Number_and_it_should_not_be_in_IR"
	addContextBinding_simpleType(&coreFile.context.bindings[0], "__Number", "__Number_and_it_should_not_be_in_IR", 0, 0);
	
	addContextBinding_compileTimeSetting(&coreFile.context.bindings[0], "core.nextSymbol", NULL);
	
	addContextBinding_macro(&coreFile, "error");
	addContextBinding_macro(&coreFile, "warning");
	addContextBinding_macro(&coreFile, "describe");
	addContextBinding_macro(&coreFile, "insertLLVMIR");
	addContextBinding_macro(&coreFile, "set");
	
	CharAccumulator *topLevelStructSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelStructSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelStructSource);
	
	CharAccumulator *topLevelConstantSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelConstantSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelConstantSource);
	
	CharAccumulator *topLevelFunctionSource = safeMalloc(sizeof(CharAccumulator));
	(*topLevelFunctionSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(topLevelFunctionSource);

	CharAccumulator *LLVMmetadataSource = safeMalloc(sizeof(CharAccumulator));
	(*LLVMmetadataSource) = (CharAccumulator){100, 0, 0};
	CharAccumulator_initialize(LLVMmetadataSource);
	
	FileInformation *FI = FileInformation_new(realpath(startFilePath, NULL), topLevelStructSource, topLevelConstantSource, topLevelFunctionSource, LLVMmetadataSource);
	compileFile(FI);
	linkedList_freeList(&FI->context.bindings[0]);
	
	if (compilerMode == CompilerMode_build_binary || compilerMode == CompilerMode_run) {
		CharAccumulator clang_command = {100, 0, 0};
		CharAccumulator_initialize(&clang_command);
		CharAccumulator_appendChars(&clang_command, clang_path);
		CharAccumulator_appendChars(&clang_command, " ");
	//	CharAccumulator_appendChars(&clang_command, getcwdBuffer);
	//	CharAccumulator_appendChars(&clang_command, "/");
		CharAccumulator_appendChars(&clang_command, objectFiles.data);
		CharAccumulator_appendChars(&clang_command, "-o ");
		CharAccumulator_appendChars(&clang_command, buildDirectory);
		CharAccumulator_appendChars(&clang_command, "/binary");
		
		if (compilerOptions.verbose) {
			printf("clang_command: %s\n", clang_command.data);
		}
		
		struct timespec clangStartTime = getTimespec();
		int clang_status = system(clang_command.data);
		
		int clang_exitCode = WEXITSTATUS(clang_status);
		
		if (clang_exitCode != 0) {
			printf("clang error\n");
			exit(1);
		}
		
		printf("Binary saved to %s/binary\n", buildDirectory);
		
		if (compilerOptions.timed) {
			printf(" clang in %llu milliseconds\n", getMilliseconds(clangStartTime, getTimespec()));
		}
		
		CharAccumulator_free(&clang_command);
	}
	
//	if (compilerMode == CompilerMode_run) {
//		printf("Running program at: %s\n", outputFilePath.data);
//		int program_status = system(outputFilePath.data);
//
//		int program_exitCode = WEXITSTATUS(program_status);
//
//		printf("Program ended with exit code: %d\n", program_exitCode);
//	}
	
	if (compilerMode == CompilerMode_check && !compilerOptions.compilerTesting) {
		printf("\nChecking complete.\n");
		if (warningCount == 1) {
			printf("1 warning generated.\n");
		} else if (warningCount > 1) {
			printf("%d warnings generated.\n", warningCount);
		} else {
			printf("No warnings generated.\n");
		}
	}
	
	free(globalConfigPath);
	free(globalConfigJSON);
	
	free(LLC_path);
	free(clang_path);
	
	if (compilerOptions.timed) {
		printf("\nfinished compiling in %llu milliseconds\n", getMilliseconds(startTime, getTimespec()));
	}
	
	return 0;
}
