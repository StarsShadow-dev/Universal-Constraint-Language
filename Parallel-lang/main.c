#include <stdio.h>
#include <string.h>

#include "fromParallel.h"

#include "jsmn.h"
#include "main.h"
#include "types.h"
#include "globals.h"
#include "compiler.h"

void printHelp(void) {
	printf("Compiler version: %s\n", CURRENT_VERSION);
	printf("\n");
	printf("Usage: parallel-lang <command> [<args>]\n");
	printf("\n");
	printf("Commands:\n");
	printf("\n");
	printf("parallel-lang build_objectFile <config_path>\n");
	printf("\tcreates an object file\n\n");
	printf("parallel-lang build_binary <config_path>\n");
	printf("\tbuild_objectFile and links the object file using clang (clang also links the C standard library)\n\n");
	printf("parallel-lang run <config_path>\n");
	printf("\tbuild_binary and runs the binary\n\n");
}

int main(int argc, char **argv) {
	CompilerMode compilerMode;
	
	if (argc == 1) {
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
	
	if (argc != 3) {
		printf("Unexpected amount of arguments, expected: 2, but got: %d\n", argc - 1);
		exit(1);
	}
	
	char *homePath = getenv("HOME");
	if (homePath == NULL) {
		printf("No home (getenv(\"HOME\")) failed");
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
	
	compileModule(argv[2], compilerMode, &LLVMsource, LLC_path, clang_path);
	
	free(globalConfigPath);
	free(globalConfigJSON);
	CharAccumulator_free(&LLVMsource);
	
	return 0;
}
