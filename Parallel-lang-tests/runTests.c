#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <spawn.h>
#include <time.h>

extern char **environ;

#define BUFFER_SIZE 1024
#define testPath "./Parallel-lang-tests/tests/"
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
	char *buf;
	// we are using calloc so that we do not have to add a NULL byte
	buf = calloc(BUFFER_SIZE, 1);

	num_read = read(filedes, buf, BUFFER_SIZE - 1);
	if (num_read > 0) {
		if (num_read == BUFFER_SIZE - 1) {
			char throwAway[1];
			ssize_t count = read(filedes, &throwAway, 1);
			if (count == 1) {
				printf("Compiler put too much to stdout.\nLast output:\n%s", buf);
				exit(1);
			}
		}
	}
	else {
		printf("No stdout from the compiler.\n");
		exit(1);
	}

	return buf;
}

int checkTestOutput(char *stdout, char *testFileData) {
	char *startOfBlockCommentText = testFileData + 3;
	char *endOfBlockComment = strstr(testFileData, "*/");
	size_t lengthOfTestExpectation = endOfBlockComment - startOfBlockCommentText;

	return strncmp(stdout, startOfBlockCommentText, lengthOfTestExpectation) == 0;
}

void runTest(char* filePath) {
	printf("runing: %s\n", filePath);

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

	if (status == 0) {
		// printf("child pid: %d\n", pid);
		if (waitpid(pid, &status, 0) < 0) {
			printf("waitpid error");
			abort();
		}
		else {
			if (WIFEXITED(status)) {
				// printf("exit code: %d\n", WEXITSTATUS(status));
			}
			else {
				printf("Compiler failed unexpectedly.\n");
			}

			close(out[1]);
			// dump_child_stdout(out[0]);

			char *compiler_stdout = read_stdout(out[0]);

			if (checkTestOutput(compiler_stdout, text)) {
				printf("Test succeeded.\n");
			} else {
				printf("Test failed.\n");
				printf("compiler_stdout:\n%s", compiler_stdout);
				exit(1);
			}

			free(compiler_stdout);
		}
	}
	else
	{
		printf("posix_spawn error: %s\n", strerror(status));
		close(out[1]);
	}

	posix_spawn_file_actions_destroy(&action);

	free(text);
}

int main(int argc, char **argv) {
	printf("Runing Tests:\n\n");

	char filePath[1024] = testPath;
	int testPathLength = strlen(testPath);
	
	struct dirent *dir;
	DIR *d = opendir(testPath);

	if (d == NULL) {
		printf("Could not open \"%s\"\n", testPath);
		return 0;
	}

	uint64_t startMilliseconds = getMilliseconds();

	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		snprintf(filePath + testPathLength, sizeof(filePath) - testPathLength, "%s", dir->d_name);

		runTest(filePath);
	}

	closedir(d);

	printf("\nAll tests succeeded in %llu milliseconds.\n", getMilliseconds() - startMilliseconds);

	return 0;
}