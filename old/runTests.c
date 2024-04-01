#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
#include <time.h>

int testsSucceeded = 0;
int testsFailed = 0;

extern char **environ;

#define BUFFER_SIZE 1024
#define testPath_fail "tests/fail/"
#define testPath_succeed "tests/succeed/"
#define testPath_describe "tests/describe/"
#define testPath_warning "tests/warning/"
#define compilerPath "./DerivedData/LLCL/Build/Products/Debug/LLCL"
// #define target_triple "arm64-apple-macosx13.0.0"

struct timespec getTimespec(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
//	timespec_get(&ts, TIME_UTC);
	return ts;
}

uint64_t getMilliseconds(struct timespec start) {
	struct timespec end = getTimespec();
	return (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
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

char *read_stdout(int filedes) {
	ssize_t num_read;
	// we are using calloc so that we do not have to add a NULL byte
	char *buffer = calloc(BUFFER_SIZE, 1);

	num_read = read(filedes, buffer, BUFFER_SIZE - 1);
	if (num_read > 0) {
		if (num_read == BUFFER_SIZE - 1) {
			char throwAway[1];
			ssize_t count = read(filedes, &throwAway, 1);
			if (count == 1) {
				printf("Compiler put too much to stdout.\nLast output:\n%s", buffer);
				exit(1);
			}
		}
	}
	else {
		free(buffer);
		return NULL;
	}

	return buffer;
}

int checkTestOutput(char *stdout, char *testFileData) {
	if (stdout == NULL) return 0;
	char *startOfBlockCommentText = testFileData + 3;
	char *endOfBlockComment = strstr(testFileData, "*/");
	size_t lengthOfTestExpectation = endOfBlockComment - startOfBlockCommentText;

	return strncmp(stdout, startOfBlockCommentText, lengthOfTestExpectation) == 0;
}

void runTest(char* filePath, int shouldSucceed, int shouldCheckTestOutput) {
	printf("\nRuning: %s\n", filePath);

	char *text = readFile(filePath);

	// printf("file contains:\n%s\n", text);

	pid_t pid;
	int out[2];
	posix_spawn_file_actions_t action;
	
	char *args[] = {compilerPath, "check", filePath, "-compilerTesting", NULL};

	posix_spawn_file_actions_init(&action);

	pipe(out);

	posix_spawn_file_actions_adddup2(&action, out[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&action, out[0]);

	int status = posix_spawn(&pid, args[0], &action, NULL, args, environ);

	if (status != 0) {
		printf("posix_spawn error: %s\n", strerror(status));
		abort();
	}

	// printf("child pid: %d\n", pid);
	
	if (waitpid(pid, &status, 0) < 0) {
		printf("waitpid error");
		abort();
	} else {
		if (!WIFEXITED(status)) {
			printf("Compiler failed unexpectedly.\n");
			abort();
		}

		close(out[1]);

		// printf("exit code: %d\n", WEXITSTATUS(status));

		char *compiler_stdout = read_stdout(out[0]);
		
		if (shouldSucceed) {
			if (WEXITSTATUS(status) != 0) {
				printf("\x1B[31m"); // red
				printf("Test failed.\n");
				printf("Expected exit code 0 but got exit code: %d\n", WEXITSTATUS(status));
				printf("compiler_stdout:\n%s", compiler_stdout);
				printf("\x1B[0m"); // reset

				testsFailed++;
				free(compiler_stdout);
				posix_spawn_file_actions_destroy(&action);
				free(text);
				return;
			}
		}
		
		if (shouldCheckTestOutput) {
			if (!checkTestOutput(compiler_stdout, text)) {
				printf("\x1B[31m"); // red
				printf("Test failed.\n");
				printf("Wrong test output.\n");
				if (compiler_stdout == NULL) {
					printf("No compiler standard out.\n");
				} else {
					printf("compiler_stdout:\n%s", compiler_stdout);
				}
				printf("\x1B[0m"); // reset

				testsFailed++;
				free(compiler_stdout);
				posix_spawn_file_actions_destroy(&action);
				free(text);
				return;
			}
		}
		
		printf("    \x1B[32mTest succeeded.\x1B[0m\n");
		testsSucceeded++;

		free(compiler_stdout);
	}

	posix_spawn_file_actions_destroy(&action);

	free(text);
}

int main(int argc, char **argv) {
	printf("Runing Tests:\n");
	
	struct dirent *dir;
	char path[1024];
	int pathLength;
	DIR *d;

	struct timespec start = getTimespec();
	
	//
	// testPath_fail
	//

	pathLength = strlen(testPath_fail);
	snprintf(path, pathLength + 1, "%s", testPath_fail);
	d = opendir(path);
	if (d == NULL) {
		printf("Could not open \"%s\"\n", path);
		return 0;
	}
	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		snprintf(path + pathLength, sizeof(path) - pathLength, "%s", dir->d_name);

		runTest(path, 0, 1);
	}
	closedir(d);

	//
	// testPath_succeed
	//

	pathLength = strlen(testPath_succeed);
	snprintf(path, pathLength + 1, "%s", testPath_succeed);

	d = opendir(path);
	if (d == NULL) {
		printf("Could not open \"%s\"\n", path);
		return 0;
	}
	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		snprintf(path + pathLength, sizeof(path) - pathLength, "%s", dir->d_name);

		runTest(path, 1, 0);
	}
	closedir(d);
	
	//
	// testPath_describe
	//
	
	pathLength = strlen(testPath_describe);
	snprintf(path, pathLength + 1, "%s", testPath_describe);
	d = opendir(path);
	if (d == NULL) {
		printf("Could not open \"%s\"\n", path);
		return 0;
	}
	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		snprintf(path + pathLength, sizeof(path) - pathLength, "%s", dir->d_name);

		runTest(path, 1, 1);
	}
	closedir(d);
	
	//
	// testPath_warning
	//
	
	pathLength = strlen(testPath_warning);
	snprintf(path, pathLength + 1, "%s", testPath_warning);
	d = opendir(path);
	if (d == NULL) {
		printf("Could not open \"%s\"\n", path);
		return 0;
	}
	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		snprintf(path + pathLength, sizeof(path) - pathLength, "%s", dir->d_name);

		runTest(path, 1, 1);
	}
	closedir(d);

	if (testsFailed > 0) {
		printf("\nRan all tests in %llu milliseconds.\n", getMilliseconds(start));
	} else {
		printf("\nAll tests succeeded in %llu milliseconds.\n", getMilliseconds(start));
	}
	printf("\x1B[32mtests succeeded: %d\x1B[0m\n", testsSucceeded);
	printf("\x1B[31mtests failed: %d\x1B[0m\n", testsFailed);

	return 0;
}