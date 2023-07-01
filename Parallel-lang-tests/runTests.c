#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#define testPath "./Parallel-lang-tests/tests/"

#define compilerPath "./DerivedData/Parallel-lang/Build/Products/Debug/Parallel-lang"

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

int main(int argc, char **argv) {
	printf("Runing Tests:\n");

	char filePath[1024] = testPath;
	int testPathLength = strlen(testPath);
	
	struct dirent *dir;
	DIR *d = opendir(testPath);

	if (d == NULL) {
		printf("Could not open \"%s\"\n", testPath);
		return 0;
	}

	while ((dir = readdir(d)) != NULL) {
		if (*dir->d_name == '.') {
			continue;
		}
		
		// this is probably not very safe
		snprintf(filePath + testPathLength, 1024 - testPathLength, "%s", dir->d_name);

		printf("runing: %s\n", filePath);

		char *text = readFile(filePath);

		printf("file contains:\n%s\n", text);

		free(text);
	}

	closedir(d);
	return 0;
}