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
#define testPath_fail "./Parallel-lang-tests/fail/"
#define testPath_succeed "./Parallel-lang-tests/succeed/"
#define compilerPath "./DerivedData/Parallel-lang/Build/Products/Debug/Parallel-lang"

#define SEC_TO_MS(sec) ((sec)*1000)
#define NS_TO_MS(ns) ((ns)/1000000)
uint64_t getMilliseconds() {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
    return ms;
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
	if (stdout == NULL) abort();
	char *startOfBlockCommentText = testFileData + 3;
	char *endOfBlockComment = strstr(testFileData, "*/");
	size_t lengthOfTestExpectation = endOfBlockComment - startOfBlockCommentText;

	return strncmp(stdout, startOfBlockCommentText, lengthOfTestExpectation) == 0;
}

void runTest(char* filePath, int shouldSucceed) {
	printf("\nRuning: %s\n", filePath);

	char *text = readFile(filePath);

	// printf("file contains:\n%s\n", text);

	int status;
	pid_t pid;
	int out[2];
	posix_spawn_file_actions_t action;
	char *args[] = {compilerPath, "compilerTesting", filePath, NULL};

	posix_spawn_file_actions_init(&action);

	pipe(out);

	posix_spawn_file_actions_adddup2(&action, out[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&action, out[0]);

	status = posix_spawn(&pid, args[0], &action, NULL, args, environ);


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
			} else {
				printf("    \x1B[32mTest succeeded.\x1B[0m\n");
				testsSucceeded++;
			}
		} else {
			if (!checkTestOutput(compiler_stdout, text)) {
				printf("\x1B[31m"); // red
				printf("Test failed.\n");
				printf("compiler_stdout:\n%s", compiler_stdout);
				printf("\x1B[0m"); // reset

				testsFailed++;
			} else {
				printf("    \x1B[32mTest succeeded.\x1B[0m\n");
				testsSucceeded++;
			}
			
		}

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

	uint64_t startMilliseconds = getMilliseconds();

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

		runTest(path, 1);
	}
	closedir(d);

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

		runTest(path, 0);
	}
	closedir(d);

	if (testsFailed > 0) {
		printf("\nRan all tests in %llu milliseconds.\n", getMilliseconds() - startMilliseconds);
	} else {
		printf("\nAll tests succeeded in %llu milliseconds.\n", getMilliseconds() - startMilliseconds);
	}
	printf("\x1B[32mtests succeeded: %d\x1B[0m\n", testsSucceeded);
	printf("\x1B[31mtests failed: %d\x1B[0m\n", testsFailed);

	return 0;
}